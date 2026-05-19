/*
 * 最小 App，用于验证 BL → APP 跳转 + YMODEM 升级链路。
 * - 链接基址 0x01002000
 * - 不使用任何中断（避免依赖 BL 的软向量表）
 * - SysTick 用作 polling delay
 * - 每 200ms 翻 LED；每秒 print 一行带计数的 "alive"
 */
#include "TX32F01_periph.h"

#define APP_LED_TOG()  GPIO_Toggle(GPIO0, PIN03)

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
}

static void uart_init_115200(void)
{
    UART_InitTypeDef u;
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    SCU_PeriphClockCmd(Periph_UART,  ENABLE);
    SCU_Lock();
    GPIO_Init(GPIO3, PIN06, GPIO_MODE_AF);
    GPIO_Init(GPIO3, PIN07, GPIO_MODE_AF);
    GPIO_PinRemapConfig(GPIO3, PIN07, GPIO_AF_UART_TX);
    GPIO_PinRemapConfig(GPIO3, PIN06, GPIO_AF_UART_RX);
    UART_DeInit();
    u.UART_BaudRate   = 115200;
    u.UART_WordLength = UART_8DATABIT;
    u.UART_StopBits   = UART_1STOPBIT;
    u.UART_Parity     = UART_Pority_None;
    u.UART_Mode       = UART_Mode_Rx | UART_Mode_Tx;
    UART_Init(&u);
    UART_Cmd(ENABLE);
}

static void uart_putc(uint8_t c)
{
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}

static void uart_puts(const char *s)
{
    while (*s) uart_putc((uint8_t)*s++);
}

/* polling delay，不开 SysTick 中断 */
static void delay_ms(uint32_t ms)
{
    SysTick->LOAD = 24000U - 1U;            /* 1 ms @ 24MHz processor clk */
    SysTick->VAL  = 0;
    SysTick->CTRL = 0x5;                    /* ENABLE | CLKSOURCE=proc, no IRQ */
    while (ms--) {
        while ((SysTick->CTRL & 0x10000U) == 0U) { }   /* COUNTFLAG */
    }
    SysTick->CTRL = 0;
}

static void put_u32_dec(uint32_t v)
{
    char buf[11]; int n = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v && n < 10) { buf[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) uart_putc((uint8_t)buf[n]);
}

int main(void)
{
    uint32_t tick = 0;

    scu_init_24mhz();
    led_init();
    uart_init_115200();

    uart_puts("\r\n[APP] hello from 0x01002000\r\n");

    for (;;) {
        APP_LED_TOG();
        delay_ms(200);
        APP_LED_TOG();
        delay_ms(200);
        if ((tick & 1U) == 0U) {
            uart_puts("[APP] alive tick=");
            put_u32_dec(tick);
            uart_puts("\r\n");
        }
        tick++;
    }
}
