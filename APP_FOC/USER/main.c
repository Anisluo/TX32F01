/*
 * APP_FOC — TX32F01 sensored FOC demo top-level.
 *
 * Boot order matters:
 *   1) clocks @ 24 MHz, UART for shell, LED for heart-beat
 *   2) PWM init (outputs DISABLED, BDTR safe state)
 *   3) ADC init with FOC ISR as EOC callback
 *   4) Hall init (EXTI primed)
 *   5) calibrate Ia/Ib offset with PWM at 50% center but outputs gated
 *      → call foc_adc_calibrate_offset() while pwm started but mode=IDLE
 *   6) start PWM, enter shell
 */
#include "TX32F01_periph.h"
#include "app_softvec.h"
#include "fault_dump.h"
#include "foc_pwm.h"
#include "foc_adc.h"
#include "foc_hall.h"
#include "foc_loop.h"
#include "foc_shell.h"

#define APP_LED_TOG()  GPIO_Toggle(GPIO0, PIN03)

/* ---------- BSP (mirrors APP/USER/main.c style) ---------- */
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

static void uart_puts(const char *s) {
    while (*s) {
        UART_ClearFlag(UART_TCIF);
        UART_SendData((uint16_t)*s++);
        while (UART_GetFlagStatus(UART_TCIF) == 0) { }
    }
}

/* Adapter so fault_dump can spit its report through our UART. */
static void uart_putc_for_dump(char c) {
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}

/* ---------- 1 ms tick: drives shell + LED + outer speed loop ---------- */
static volatile uint32_t s_ms_tick;
static void on_systick(void)
{
    s_ms_tick++;
}

int main(void)
{
    scu_init_24mhz();
    led_init();
    uart_init_115200();
    uart_puts("\r\n[FOC] APP_FOC start @ 0x01002000\r\n");

    /* Fault dump: hook into the BL's HardFault trampoline via the soft
     * vector table, then drain any pending report from the last reset.
     * MUST happen before any IRQ that could itself fault, so the next
     * fault has a place to land. */
    app_softvec_register_sys(APP_SVEC_HARDFAULT, fault_dump_hardfault_entry);
    if (fault_dump_init()) {
        uart_puts("[FOC] previous boot crashed -- dumping:\r\n");
        fault_dump_drain_to_uart(uart_putc_for_dump);
    }
    fault_dump_set_module("main");

    /* 1 ms SysTick via soft-vector trampoline */
    app_softvec_register_systick(on_systick);
    SysTick_Config(24000U);
    NVIC_SetPriority(SysTick_IRQn, 1);   /* below FOC ADC IRQ */

    /* Bring the controller up but keep PWM gated */
    foc_pwm_init();
    foc_hall_init();
    foc_loop_init();

    /* Start the carriers in lock-step.  Mode is IDLE so the loop just
     * writes 50/50/50 — no motor current flows even though the ADC is
     * sequencing every period. */
    foc_pwm_start();

    /* Offset calibration: shunt amp output with zero current.
     * SAFE because mode=IDLE → SVPWM puts each leg at 50%, common-mode
     * cancels at the motor, no phase-to-phase voltage. */
    foc_adc_calibrate_offset();
    {
        uint16_t a0, b0;
        foc_adc_get_offsets(&a0, &b0);
        uart_puts("[FOC] Ia0="); { char b[8]; int n=0; uint16_t v=a0;
            if (!v) b[n++]='0'; else while (v){ b[n++]=(char)('0'+v%10); v/=10; }
            while (n--) {
                UART_ClearFlag(UART_TCIF);
                UART_SendData((uint16_t)b[n]);
                while (UART_GetFlagStatus(UART_TCIF)==0){}
            }
        }
        uart_puts(" Ib0="); { char b[8]; int n=0; uint16_t v=b0;
            if (!v) b[n++]='0'; else while (v){ b[n++]=(char)('0'+v%10); v/=10; }
            while (n--) {
                UART_ClearFlag(UART_TCIF);
                UART_SendData((uint16_t)b[n]);
                while (UART_GetFlagStatus(UART_TCIF)==0){}
            }
        }
        uart_puts("\r\n");
    }

    foc_shell_init();

    uint32_t last_ms = 0;
    uint32_t led_ms  = 0;
    for (;;) {
        uint32_t now = s_ms_tick;
        if (now != last_ms) {
            last_ms = now;
            foc_shell_tick();
        }
        if (now - led_ms >= 250U) {
            led_ms = now;
            APP_LED_TOG();
        }
    }
}
