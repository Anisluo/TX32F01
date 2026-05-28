/*
 * TX32F01 Bootloader
 *
 *  上电流程：
 *    1. SCU 初始化（24MHz、BOR 2.5V）
 *    2. UART 初始化（115200）
 *    3. SysTick 1ms（仅 BL 用，跳前会关）
 *    4. 读 boot flag / 检查 App meta
 *        a. boot flag == 0xA55AF00D            → 强制留 BL，等 YMODEM
 *        b. meta 校验通过                       → 跳 App
 *        c. 否则                                → 留 BL，等 YMODEM
 *    5. 在 BL 模式下：
 *        - 上电 3 秒内若收到任意字节 → 进入升级
 *        - 升级成功：写 meta，清 boot flag，软复位
 *        - 升级失败：闪烁错误码 LED，等下一次
 */
#include "TX32F01_periph.h"
#include "boot_layout.h"
#include "boot_flash.h"
#include "boot_uart.h"
#include "boot_ymodem.h"
#include "boot_jump.h"

/* 板载 LED：GPIO0 / PIN03，推挽。
   与 demo LED.h 中 LED(1)=亮 一致：高电平点亮。
   如果你的板子是低有效，把 ON/OFF 两个宏对调即可。 */
#define BL_LED_ON()      GPIO_SetBits(GPIO0, PIN03)
#define BL_LED_OFF()     GPIO_ResetBits(GPIO0, PIN03)
#define BL_LED_TOGGLE()  GPIO_Toggle(GPIO0, PIN03)

static void scu_init_24mhz(void)
{
    SCU_Unlock();
    SCU_SetSysClock(SysClock_24M);
    SCU_ResetPeriphClock(Periph_ALL);
    SCU_SetBor(BOR_2P5V, ENABLE);
    SCU_ClearPWR_Flag();
    SCU_Lock();
}

static void led_init(void)
{
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO0, ENABLE);
    SCU_Lock();
    GPIO_Init(GPIO0, PIN03, GPIO_MODE_OUTPUT_PP);
    BL_LED_OFF();
}

static void systick_1ms_init(void)
{
    /* 24MHz / 1000 = 24000 → 1 ms tick */
    SysTick_Config(24000U);
    NVIC_SetPriority(SysTick_IRQn, 0x3);
}

static void busy_delay_ms(uint32_t ms)
{
    uint32_t t0 = buart_now_ms();
    while ((uint32_t)(buart_now_ms() - t0) < ms) { }
}

static void error_blink_forever(uint8_t code)
{
    /* 长亮 1s + 短闪 code 次循环 */
    for (;;) {
        BL_LED_ON();  busy_delay_ms(1000);
        BL_LED_OFF(); busy_delay_ms(300);
        for (uint8_t i = 0; i < code; i++) {
            BL_LED_ON();  busy_delay_ms(150);
            BL_LED_OFF(); busy_delay_ms(150);
        }
        busy_delay_ms(800);
    }
}

static void print_banner(void)
{
    static const char msg[] =
        "\r\n"
        "============================\r\n"
        "  TX32F01 Bootloader v1.0\r\n"
        "  Send firmware via YMODEM-1K\r\n"
        "  Timeout: 3s, then boot APP\r\n"
        "============================\r\n";
    buart_send((const uint8_t *)msg, sizeof(msg) - 1);
}

static BOOL wait_first_byte(uint32_t timeout_ms)
{
    /* 期间 LED 慢闪指示"等待中"，同时每 500ms 发一行心跳，
       方便上位机/远程抓到 BL 在跑 */
    uint32_t t0 = buart_now_ms();
    uint32_t last_toggle = t0;
    uint32_t last_beat   = t0;
    uint8_t  ch;
    while ((uint32_t)(buart_now_ms() - t0) < timeout_ms) {
        if ((uint32_t)(buart_now_ms() - last_toggle) >= 250U) {
            BL_LED_TOGGLE();
            last_toggle = buart_now_ms();
        }
        if ((uint32_t)(buart_now_ms() - last_beat) >= 500U) {
            buart_send((const uint8_t *)"[BL] alive\r\n", 12);
            last_beat = buart_now_ms();
        }
        if (buart_recv_byte(&ch, 10)) {
            (void)ch;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL do_upgrade_session(void)
{
    uint32_t size = 0;
    BL_LED_ON();
    bflash_prepare();
    uint32_t got = ymodem_recv_to_app(&size);
    if (got == 0) {
        buart_send((const uint8_t *)"\r\nERR: ymodem failed\r\n", 22);
        return FALSE;
    }
    /* 计算 CRC，写 meta */
    uint16_t crc = bflash_crc16(FLASH_APP_BASE,
                                FLASH_APP_BASE + size - 1U);
    app_meta_t m;
    m.magic    = APP_META_MAGIC;
    m.app_size = size;
    m.crc16    = crc;
    m.reserved = 0;
    if (!bflash_write_meta(&m)) {
        buart_send((const uint8_t *)"\r\nERR: write meta\r\n", 19);
        return FALSE;
    }
    /* 升级成功就清掉 boot 请求标志 */
    bflash_clear_bootflag();
    buart_send((const uint8_t *)"\r\nOK: rebooting...\r\n", 21);
    busy_delay_ms(100);
    NVIC_SystemReset();
    return TRUE;
}

static void soft_vector_clear(void)
{
    /* SRAM 头 96B 是软向量表，未被 CRT 初始化，必须先清零，
       否则 BL 自己的 SysTick 跳板会跳到垃圾地址 */
    volatile uint32_t *p = (volatile uint32_t *)SOFT_VECTOR_BASE;
    for (uint32_t i = 0; i < SOFT_VECTOR_COUNT; i++) p[i] = 0;
}

int main(void)
{
    scu_init_24mhz();
    soft_vector_clear();        /* 必须早于任何中断使能 */
    led_init();
    buart_init(115200);
    systick_1ms_init();

    print_banner();

    BOOL force_bl     = (bflash_read_bootflag() == BOOT_REQ_UPDATE);
    BOOL app_ok       = bl_app_meta_valid();

    /* DEV MODE: SWD-flashed APPs don't have meta written, so meta CRC is
     * always invalid. Treating that as "APP OK" lets us iterate with
     * Keil + J-Link instead of doing a full YMODEM cycle every build.
     *
     * Before shipping: delete this line so BL refuses to boot an APP
     * whose meta hasn't been written (i.e., one that didn't come from
     * a verified YMODEM session). */
    app_ok = TRUE;

    if (!force_bl && app_ok) {
        /* 8 秒升级窗口 —— 给 PC 端工具足够时间响应（PS 启动 + 打开 COM 慢）
         * DEV: 调成 1000U（1 秒）加快 debug 循环；上线前改回 8000U。 */
        if (wait_first_byte(1000U)) {
            do_upgrade_session();
            error_blink_forever(2);
        }
        BL_LED_OFF();
        bl_jump_to_app();
        /* never returns */
    }

    /* 强制升级 或 App 无效：阻塞等 PC */
    for (;;) {
        if (force_bl)
            buart_send((const uint8_t *)"FORCE update mode\r\n", 19);
        else
            buart_send((const uint8_t *)"APP invalid, need update\r\n", 26);

        wait_first_byte(0xFFFFFFFFU);   /* 一直等 */
        if (do_upgrade_session()) break;
        error_blink_forever(3);         /* 失败则错误码 3 */
    }
    return 0;
}
