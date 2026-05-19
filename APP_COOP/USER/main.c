/*
 * APP_COOP — production-grade cooperative scheduler demo.
 *
 *  - Links to 0x01002000, sits behind bootloader (OTA-able via YMODEM)
 *  - 3 tasks demonstrating the 3 canonical patterns:
 *      1. simple periodic           (task_led)
 *      2. periodic with own state   (task_status)
 *      3. non-blocking sequence     (task_flash_seq)
 *  - SysTick @ 1 ms via bootloader soft-vector table
 *
 *  Triggering update from APP:
 *      send 'u' over UART → calls app_request_bootloader_update()
 */
#include "TX32F01_periph.h"
#include "coop_sched.h"
#include "app_softvec.h"

/* ============ BSP boilerplate ============ */
static void scu_init_24mhz(void) {
    SCU_Unlock();
    SCU_SetSysClock(SysClock_24M);
    SCU_ResetPeriphClock(Periph_ALL);
    SCU_SetBor(BOR_2P5V, ENABLE);
    SCU_ClearPWR_Flag();
    SCU_Lock();
}

static void led_init(void) {
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO0, ENABLE);
    SCU_Lock();
    GPIO_Init(GPIO0, PIN03, GPIO_MODE_OUTPUT_PP);
}

static void uart_init_115200(void) {
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

/* Polling uart helpers — non-blocking enough for tasks (~90 us per byte @115200) */
static void uart_putc(char c) {
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_put_u32(uint32_t v) {
    char b[11]; int n=0;
    if (!v) { uart_putc('0'); return; }
    while (v && n < 10) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) uart_putc(b[n]);
}

/* ============ Pattern 1: Simple periodic（保留私有状态） ============
 * 这个任务不抢 LED（LED 给 task_flash_seq 演示用），
 * 只演示"最简单的周期任务"：累加一个计数器，task_status 会打印它。
 */
static volatile uint32_t g_heartbeat;
static void task_heartbeat(void)
{
    g_heartbeat++;
}

/* forward decl for task_status */
static coop_task_t s_tasks[];

/* ============ Pattern 2: Periodic with own state ============ */
static void task_status(void)
{
    static uint32_t s_count = 0;
    uart_puts("[COOP] status tick=");
    uart_put_u32(s_count++);
    uart_puts(" heartbeat=");
    uart_put_u32(g_heartbeat);
    uart_puts(" flash_seq_runs=");
    uart_put_u32(s_tasks[2].run_count);   /* 直接读调度器的运行计数 */
    uart_puts("\r\n");
}

/* ============ Pattern 3: Non-blocking multi-step sequence ============
 * 这是协作式调度的 _关键_ 技能：把 "blink-blink-blink-pause" 这种本来想用
 * delay_ms() 的逻辑，改写成 state machine。
 *
 * 旧的写法（被禁止）：
 *     LED on; delay 50ms; LED off; delay 50ms; LED on; delay 50ms; LED off; delay 800ms;
 *
 * 协作式写法：调度器每 50ms 调用一次 task_flash_seq，每次做一步：
 */
typedef enum {
    SEQ_ON_1, SEQ_OFF_1,
    SEQ_ON_2, SEQ_OFF_2,
    SEQ_ON_3, SEQ_LONG_PAUSE
} seq_state_t;

static void task_flash_seq(void)
{
    static seq_state_t s_state = SEQ_ON_1;
    static uint8_t     s_pause_ticks = 0;     /* counts 50ms units */

    switch (s_state) {
        case SEQ_ON_1:       GPIO_SetBits(GPIO0, PIN03);   s_state = SEQ_OFF_1; break;
        case SEQ_OFF_1:      GPIO_ResetBits(GPIO0, PIN03); s_state = SEQ_ON_2;  break;
        case SEQ_ON_2:       GPIO_SetBits(GPIO0, PIN03);   s_state = SEQ_OFF_2; break;
        case SEQ_OFF_2:      GPIO_ResetBits(GPIO0, PIN03); s_state = SEQ_ON_3;  break;
        case SEQ_ON_3:       GPIO_SetBits(GPIO0, PIN03);   s_state = SEQ_LONG_PAUSE; s_pause_ticks = 0; break;
        case SEQ_LONG_PAUSE:
            GPIO_ResetBits(GPIO0, PIN03);
            if (++s_pause_ticks >= 16) {      /* 16 × 50ms = 800ms 长停顿 */
                s_pause_ticks = 0;
                s_state = SEQ_ON_1;
            }
            break;
    }
}

/* ============ Task table (priority = order in array) ============ */
static coop_task_t s_tasks[] = {
    /* func              period  next  count */
    { task_heartbeat,      100,    0,    0 },   /* 100ms 累加心跳 */
    { task_status,        1000,    0,    0 },   /* 每秒打印一次 */
    { task_flash_seq,      50,    0,    0 },   /* 50ms 步进的 LED 序列 */
};
#define N_TASKS (sizeof(s_tasks)/sizeof(s_tasks[0]))

/* ============ SysTick @ 1 ms via bootloader soft-vector ============ */
static void on_systick(void)
{
    coop_tick();
}

/* ============ main ============ */
int main(void)
{
    scu_init_24mhz();
    led_init();
    uart_init_115200();

    /* 先注册 SysTick handler 到软向量表，再使能 SysTick，
       否则 1ms 后 SysTick 中断进来会跳到 NULL */
    app_softvec_register_systick(on_systick);

    /* 24MHz / 1000 = 24000 → 1ms tick */
    SysTick_Config(24000U);
    NVIC_SetPriority(SysTick_IRQn, 0x3);

    uart_puts("\r\n[COOP] cooperative scheduler starting, ");
    uart_put_u32(N_TASKS);
    uart_puts(" tasks\r\n");

    coop_init(s_tasks, N_TASKS);
    coop_run();    /* 永不返回 */

    return 0;
}
