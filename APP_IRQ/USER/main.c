/*
 * APP_IRQ — interrupt-latency lab.
 *
 *   - SysTick @ 1 ms drives both the latency probe and the COOP tick.
 *   - SysTick handler reads SYST_CVR as its FIRST action → latency in cycles.
 *   - Stressor task runs in main context; histogram visualizes the impact
 *     of pre-emption / critical sections / WFI vs busy.
 *
 *   See README.md for the theoretical model and how to map the measured
 *   numbers onto your own (e.g. FOC) high-priority ISR.
 */
#include "TX32F01_periph.h"
#include "coop_sched.h"
#include "app_softvec.h"
#include "lat_meas.h"
#include "lat_stress.h"
#include "lat_shell.h"

#define SYSCLK_HZ           24000000UL
#define TICK_PERIOD_CYCLES  24000U      /* 1 ms */
#define TICK_RVR            (TICK_PERIOD_CYCLES - 1U)

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
static void uart_putc(char c) {
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

/* ---------- SysTick handler (CRITICAL: lat_meas_record MUST be first) ---------- */
static void on_systick(void)
{
    lat_meas_record();      /* reads SYST_CVR before doing anything else */
    coop_tick();
}

/* ---------- Tasks ---------- */
static void task_led(void)
{
    static uint8_t on;
    if ((on ^= 1)) GPIO_SetBits(GPIO0, PIN03); else GPIO_ResetBits(GPIO0, PIN03);
}
static void task_stress(void) { lat_stress_step(); }
static void task_shell (void) { lat_shell_tick(); }

static coop_task_t s_tasks[] = {
    { task_stress,  0,  0, 0 },     /* run every loop iter — see lat_stress.c */
    { task_shell,  10,  0, 0 },
    { task_led,   500,  0, 0 },
};
#define N_TASKS (sizeof(s_tasks)/sizeof(s_tasks[0]))

int main(void)
{
    scu_init_24mhz();
    led_init();
    uart_init_115200();

    uart_puts("\r\n[lab] APP_IRQ — Cortex-M0 IRQ latency lab\r\n");
    uart_puts("[lab] sysclk=24MHz, SysTick reload="); /* RVR+1 */
    {
        char buf[12]; int n=0; uint32_t v = TICK_PERIOD_CYCLES;
        if (!v) buf[n++]='0';
        else while (v && n<11) { buf[n++]=(char)('0'+v%10); v/=10; }
        while (n--) uart_putc(buf[n]);
    }
    uart_puts(" cyc\r\n");

    lat_meas_init(TICK_RVR);
    lat_shell_init();

    /* Register SysTick handler via BL soft-vector.
       The trampoline adds a small constant offset (~7 cyc) to the measured
       latency vs a bare-metal app that owns the vector table — see README. */
    app_softvec_register_systick(on_systick);

    /* Highest priority, by design — this is what FOC's current-loop IRQ
       would also be set to. */
    SysTick_Config(TICK_PERIOD_CYCLES);
    NVIC_SetPriority(SysTick_IRQn, 0);

    coop_init(s_tasks, N_TASKS);
    coop_run();
    return 0;
}
