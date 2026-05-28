/*
 * APP_TEST — minimal APP for verifying the bootloader → APP handoff path.
 *
 *   Purpose: confirm BL + APP_* framework is alive on a stripped-down
 *   target with ZERO external hardware (no SPI NOR, no motor, no shunt,
 *   no Hall sensors). If this APP runs and prints heartbeats, the entire
 *   BL → softvec → APP chain is healthy and any breakage in the bigger
 *   APP_FOC / APP_LOG / APP_BENCH is in their own logic, not infra.
 *
 *   What it does:
 *     - inits clock, LED, UART
 *     - registers soft-vector handlers for SysTick + HardFault
 *     - drains any pending fault dump from previous boot
 *     - main loop: blink LED 2 Hz, print "[TEST] alive #N up=...ms" every 1 s
 *     - shell: any keypress prints status; type "crash" to test fault_dump
 *
 *   What it does NOT do:
 *     - no ADC, PWM, SPI, I2C, EXTI, IWDT — all of those are removed so
 *       a broken peripheral can't mask an infra issue
 *
 * Expected UART output (115200, 8N1):
 *
 *     ============================
 *       TX32F01 Bootloader v1.0
 *     ============================
 *     [BL] alive ×N
 *     [TEST] boot reason=...  boots=1
 *     [TEST] alive #1 up=1000ms
 *     [TEST] alive #2 up=2000ms
 *     ...
 */
#include "TX32F01_periph.h"
#include "app_softvec.h"
#include "fault_dump.h"

/* ---------- BSP ---------- */
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
    u.UART_BaudRate=115200; u.UART_WordLength=UART_8DATABIT;
    u.UART_StopBits=UART_1STOPBIT; u.UART_Parity=UART_Pority_None;
    u.UART_Mode=UART_Mode_Rx|UART_Mode_Tx;
    UART_Init(&u);
    UART_Cmd(ENABLE);
}

/* ---------- UART helpers ---------- */
static void uart_putc(char c)
{
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_put_u32(uint32_t v)
{
    char b[11]; int n = 0;
    if (!v) { uart_putc('0'); return; }
    while (v && n < 10) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) uart_putc(b[n]);
}
static void uart_put_hex(uint32_t v)
{
    static const char H[] = "0123456789ABCDEF";
    int i;
    uart_putc('0'); uart_putc('x');
    for (i = 7; i >= 0; i--) uart_putc(H[(v >> (i * 4)) & 0xFU]);
}

/* ---------- 1 ms tick via soft-vector ---------- */
static volatile uint32_t s_ms_tick;
static void on_systick(void) { s_ms_tick++; }

/* ---------- Trigger a HardFault to validate the dump path ---------- */
static void do_crash(void)
{
    /* Invalid Thumb target — guaranteed HardFault. */
    typedef void (*vfn_t)(void);
    vfn_t f = (vfn_t)0xFFFFFFFEUL;
    f();
    for (;;) { }
}

/* ---------- Print the SCU reset reason in human form ---------- */
static void print_reset_reason(void)
{
    uint32_t rsr = SCU->RSR;
    uart_puts("[TEST] SCU_RSR=");
    uart_put_hex(rsr);
    if (rsr & 0x02) uart_puts(" POR");
    if (rsr & 0x04) uart_puts(" BOR");
    if (rsr & 0x08) uart_puts(" PIN");
    if (rsr & 0x10) uart_puts(" IWDT");
    if (rsr & 0x20) uart_puts(" SOFT");
    uart_puts("\r\n");
    /* Note: SCU_RSR is cleared by fault_dump_init() internally — so this
     * print must run BEFORE fault_dump_init() to see the raw value. */
}

int main(void)
{
    scu_init_24mhz();
    led_init();
    uart_init_115200();

    uart_puts("\r\n[TEST] APP_TEST start @ 0x01002000\r\n");
    print_reset_reason();

    /* HardFault catcher: register before anything that could fault. */
    app_softvec_register_sys(APP_SVEC_HARDFAULT, fault_dump_hardfault_entry);
    if (fault_dump_init()) {
        uart_puts("[TEST] previous boot crashed -- dumping:\r\n");
        fault_dump_drain_to_uart(uart_putc);
    } else {
        uart_puts("[TEST] no pending fault. boot_count=");
        uart_put_u32(fault_dump_boot_count());
        uart_puts(" total_faults=");
        uart_put_u32(fault_dump_fault_count());
        uart_puts("\r\n");
    }
    fault_dump_set_module("test");

    /* 1 ms SysTick via soft-vector */
    app_softvec_register_systick(on_systick);
    SysTick_Config(24000U);
    NVIC_SetPriority(SysTick_IRQn, 3);

    uart_puts("[TEST] BL+APP handoff OK. heartbeat starting...\r\n");

    uint32_t last_led  = 0;
    uint32_t last_beat = 0;
    uint32_t beat_cnt  = 0;

    for (;;) {
        uint32_t now = s_ms_tick;

        /* 500 ms LED toggle */
        if (now - last_led >= 500U) {
            last_led = now;
            GPIO_Toggle(GPIO0, PIN03);
        }

        /* 1000 ms heartbeat */
        if (now - last_beat >= 1000U) {
            last_beat = now;
            beat_cnt++;
            uart_puts("[TEST] alive #");
            uart_put_u32(beat_cnt);
            uart_puts(" up=");
            uart_put_u32(now);
            uart_puts("ms\r\n");
        }

        /* Tiny shell: read a byte if available */
        if (UART_GetFlagStatus(UART_RDNEIF)) {
            char c = (char)UART_ReceiveData();
            switch (c) {
            case '?':
            case 'h':
                uart_puts("\r\n  ? - help\r\n  s - status\r\n  c - crash (test fault_dump)\r\n");
                break;
            case 's':
                uart_puts("\r\n[TEST] up=");      uart_put_u32(now);
                uart_puts("ms beats=");           uart_put_u32(beat_cnt);
                uart_puts(" boots=");             uart_put_u32(fault_dump_boot_count());
                uart_puts(" faults=");            uart_put_u32(fault_dump_fault_count());
                uart_puts("\r\n");
                break;
            case 'c':
                uart_puts("\r\n[TEST] crashing intentionally...\r\n");
                do_crash();
                break;
            default:
                /* echo printable */
                if (c >= 0x20 && c < 0x7F) uart_putc(c);
                break;
            }
        }
    }
}
