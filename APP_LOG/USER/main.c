/*
 * APP_LOG — data-acquisition firmware: cooperative scheduler + ring log
 * over external SPI NOR (W25Q16 default).
 *
 *   - Links @ 0x01002000 behind the bootloader (OTA-able via YMODEM).
 *   - 5 tasks:
 *       task_led        500 ms   visual heartbeat
 *       task_sample     1000 ms  append one sensor record
 *       task_log_pump   5 ms     pump async-erase state machine
 *       task_shell      10 ms    UART command shell
 *       task_status     5000 ms  print uptime + stats
 *
 * Wiring (matches vendor SPI flash demo, no changes needed):
 *     LED   GPIO0.PIN03
 *     UART  GPIO3.PIN07 TX / GPIO3.PIN06 RX, 115200 8N1
 *     SPI   CS=GPIO2.04 CLK=GPIO2.05 MOSI=GPIO3.00 MISO=GPIO3.01
 */
#include "TX32F01_periph.h"
#include "coop_sched.h"
#include "app_softvec.h"
#include "fault_dump.h"
#include "log_ring.h"
#include "log_shell.h"
#include "log_bd.h"

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

/* ---------- UART tx helpers (used only by task_status / startup banner) ---------- */
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

/* ---------- Tasks ---------- */
static void task_led(void)
{
    static uint8_t on;
    if ((on ^= 1)) GPIO_SetBits(GPIO0, PIN03);
    else           GPIO_ResetBits(GPIO0, PIN03);
}

/* Synthetic "sensor reading": 4-byte tick + 4-byte counter. */
static void task_sample(void)
{
    static uint32_t s_count;
    uint8_t  rec[8];
    uint32_t ms = coop_now_ms();
    rec[0]=(uint8_t)ms;        rec[1]=(uint8_t)(ms>>8);
    rec[2]=(uint8_t)(ms>>16);  rec[3]=(uint8_t)(ms>>24);
    rec[4]=(uint8_t)s_count;        rec[5]=(uint8_t)(s_count>>8);
    rec[6]=(uint8_t)(s_count>>16);  rec[7]=(uint8_t)(s_count>>24);
    s_count++;
    /* Drop silently on BUSY — next tick we'll log a fresh sample. For real
       sensors prefer a tiny RAM FIFO + retry; this is the demo signal. */
    (void)log_ring_append(0x01 /* type=sample */, rec, sizeof(rec));
}

static void task_log_pump(void) { log_ring_tick(); }
static void task_shell   (void) { log_shell_tick(); }

static void task_status(void)
{
    uint32_t tot, hs, hp, ts, ns;
    log_ring_stats(&tot, &hs, &hp, &ts, &ns);
    uart_puts("[LOG] up="); uart_put_u32(coop_now_ms());
    uart_puts("ms  head=");    uart_put_u32(hs); uart_putc('.'); uart_put_u32(hp);
    uart_puts(" tail=");       uart_put_u32(ts);
    uart_puts(" seq=");        uart_put_u32(ns);
    uart_puts("\r\n");
}

static coop_task_t s_tasks[] = {
    { task_led,        500,  0, 0 },
    { task_sample,    1000,  0, 0 },
    { task_log_pump,     5,  0, 0 },
    { task_shell,       10,  0, 0 },
    { task_status,    5000,  0, 0 },
};
#define N_TASKS (sizeof(s_tasks)/sizeof(s_tasks[0]))

static void on_systick(void) { coop_tick(); }

int main(void)
{
    scu_init_24mhz();
    led_init();
    uart_init_115200();

    /* HardFault diagnostic: register handler in soft-vector BEFORE anything
       that could fault, then drain any record left by the previous boot. */
    app_softvec_register_sys(APP_SVEC_HARDFAULT, fault_dump_hardfault_entry);
    (void)fault_dump_init();
    fault_dump_drain_to_uart(uart_putc);

    uart_puts("\r\n[LOG] boot — initializing SPI NOR...\r\n");

    int rc = log_ring_init();
    if (rc != 0) {
        uart_puts("[LOG] SPI NOR not found (jedec=");
        uart_put_u32(log_bd_jedec());
        uart_puts(") — halting.\r\n");
        for (;;) { }
    }

    uint32_t tot;
    log_ring_stats(&tot, 0, 0, 0, 0);
    uart_puts("[LOG] jedec ok, sectors=");
    uart_put_u32(tot);
    uart_puts("\r\n");

    log_shell_init();

    app_softvec_register_systick(on_systick);
    SysTick_Config(24000U);                /* 24MHz / 1000 → 1 ms */
    NVIC_SetPriority(SysTick_IRQn, 0x3);

    coop_init(s_tasks, N_TASKS);
    coop_run();
    return 0;
}
