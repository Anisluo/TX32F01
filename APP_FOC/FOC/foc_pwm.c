#include "foc_pwm.h"
#include "TX32F01_periph.h"

static void enable_phase_clocks(void)
{
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_TIM0,  ENABLE);
    SCU_PeriphClockCmd(Periph_TIM1,  ENABLE);
    SCU_PeriphClockCmd(Periph_TIM2,  ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO0, ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO1, ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO2, ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);   /* Phase V HI now lives on GPIO3.PIN05 */
    SCU_Lock();
}

/* Hard-coded list of pins the chip's debug port owns. Reconfiguring any
 * of these — even to GPIO input — disconnects J-Link instantly. The
 * runtime check below asserts that none of our phase pins land here. */
#define IS_SWD_PIN(port, pin) \
    ((port) == GPIO0 && ((pin) == PIN00 || (pin) == PIN01))

static int assert_not_swd(GPIO_Type *port, uint8_t pin)
{
    return IS_SWD_PIN(port, pin) ? 0 : 1;
}

static void cfg_pins(void)
{
    /* Belt-and-suspenders: if anyone ever drops a SWD pin back into
     * foc_config.h, hang here with the line that did it. Better than
     * shipping another brick. */
    if (!assert_not_swd(FOC_U_HI_PORT, FOC_U_HI_PIN)) for (;;) { }
    if (!assert_not_swd(FOC_U_LO_PORT, FOC_U_LO_PIN)) for (;;) { }
    if (!assert_not_swd(FOC_V_HI_PORT, FOC_V_HI_PIN)) for (;;) { }
    if (!assert_not_swd(FOC_V_LO_PORT, FOC_V_LO_PIN)) for (;;) { }
    if (!assert_not_swd(FOC_W_HI_PORT, FOC_W_HI_PIN)) for (;;) { }
    if (!assert_not_swd(FOC_W_LO_PORT, FOC_W_LO_PIN)) for (;;) { }

    GPIO_Init(FOC_U_HI_PORT, FOC_U_HI_PIN, GPIO_MODE_AF);
    GPIO_Init(FOC_U_LO_PORT, FOC_U_LO_PIN, GPIO_MODE_AF);
    GPIO_Init(FOC_V_HI_PORT, FOC_V_HI_PIN, GPIO_MODE_AF);
    GPIO_Init(FOC_V_LO_PORT, FOC_V_LO_PIN, GPIO_MODE_AF);
    GPIO_Init(FOC_W_HI_PORT, FOC_W_HI_PIN, GPIO_MODE_AF);
    GPIO_Init(FOC_W_LO_PORT, FOC_W_LO_PIN, GPIO_MODE_AF);

    GPIO_PinRemapConfig(FOC_U_HI_PORT, FOC_U_HI_PIN, FOC_U_HI_AF);
    GPIO_PinRemapConfig(FOC_U_LO_PORT, FOC_U_LO_PIN, FOC_U_LO_AF);
    GPIO_PinRemapConfig(FOC_V_HI_PORT, FOC_V_HI_PIN, FOC_V_HI_AF);
    GPIO_PinRemapConfig(FOC_V_LO_PORT, FOC_V_LO_PIN, FOC_V_LO_AF);
    GPIO_PinRemapConfig(FOC_W_HI_PORT, FOC_W_HI_PIN, FOC_W_HI_AF);
    GPIO_PinRemapConfig(FOC_W_LO_PORT, FOC_W_LO_PIN, FOC_W_LO_AF);
}

static void cfg_one_timer(TIM_Type *tim)
{
    TIM_InitTypeDef ts;
    TIM_DeInit(tim);

    ts.TIM_Mode      = TIM_Mode_CompareOut;
    ts.TIM_Prescaler = (uint16_t)(FOC_TIM_PRESC - 1U);
    ts.TIM_Period    = (uint16_t)FOC_ARR_TOP;
    TIM_Init(tim, &ts);

    /* Half-duty centered start to avoid an initial transient at enable.
     * commit() will overwrite this within one PWM period. */
    TIM_SetCompare(tim, (uint16_t)(FOC_ARR_TOP / 2U));

    TIM_SetOCx_Polarity (tim, TIM_OCx_High);   /* OCx  initial HIGH = high-side ON */
    TIM_SetOCxN_Polarity(tim, TIM_OCxN_Low);   /* OCxN initial LOW  = low-side OFF */
    TIM_SetDeadTime     (tim, FOC_DEADTIME_TCK);

    /* Preload ARR & CCR so updates take effect on next update event,
     * not mid-period.  The library spelling for that lives in CR bits
     * ARPE/CCPE — we set them directly. */
    tim->CR |= (1U << 8) | (1U << 9);          /* ARPE | CCPE */

    TIM_OCxOutEnable (tim, ENABLE);
    TIM_OCxNOutEnable(tim, ENABLE);
}

int foc_pwm_init(void)
{
    enable_phase_clocks();
    cfg_pins();

    cfg_one_timer(TIM0);
    cfg_one_timer(TIM1);
    cfg_one_timer(TIM2);

    /* TIM0 is master and the ADC trigger source.  Tell it to emit a
     * trigger every update event so the ADC sequence fires once per
     * PWM period.  See the ADC driver for how it consumes this. */
    TIM0->CR |= (1U << 1);                     /* MMS = 1, master mode */

    /* Outputs disabled until foc_pwm_start() to keep gates quiet. */
    return 0;
}

void foc_pwm_commit(uint16_t t_a, uint16_t t_b, uint16_t t_c)
{
    /* high-on time t  →  CCR = (ARR+1) - t   (see header for derivation) */
    uint32_t period_p1 = (uint32_t)FOC_ARR_TOP + 1U;
    uint16_t ccr_a = (uint16_t)(period_p1 - t_a);
    uint16_t ccr_b = (uint16_t)(period_p1 - t_b);
    uint16_t ccr_c = (uint16_t)(period_p1 - t_c);

    /* Back-to-back writes — preload makes them all latch on the next
     * common update event (TIM0 master).  No partial-write hazard. */
    TIM0->CCR = ccr_a;
    TIM1->CCR = ccr_b;
    TIM2->CCR = ccr_c;
}

void foc_pwm_start(void)
{
    /* Reset counters together so the three carriers share phase. */
    TIM0->CNT = 0;
    TIM1->CNT = 0;
    TIM2->CNT = 0;

    /* Set CEN on all three within a handful of cycles. */
    TIM0->CR |= 1U;
    TIM1->CR |= 1U;
    TIM2->CR |= 1U;
}

void foc_pwm_stop(void)
{
    TIM0->CR &= ~1U;
    TIM1->CR &= ~1U;
    TIM2->CR &= ~1U;
}

void foc_pwm_force_safe(void)
{
    /* Drive both OCx and OCxN to OCxBreakState_Low using the BDTR
     * break path — independent of CR.CEN, works even during a fault. */
    TIM_BDTRInitTypeDef bd;
    bd.TIM_BreakPolarity   = TIM_Breaklarity_Falling;
    bd.TIM_OCxBreakState   = TIM_OCxBreakState_Low;
    bd.TIM_OCxNBreakState  = TIM_OCxNBreakState_Low;
    bd.TIM_BreakFilterValue= 4U;
    bd.TIM_AutomaticOutput = TIM_AutomaticOutput_Disable;
    TIM_BDTRConfig(TIM0, &bd);  TIM_BDTRCmd(TIM0, ENABLE);
    TIM_BDTRConfig(TIM1, &bd);  TIM_BDTRCmd(TIM1, ENABLE);
    TIM_BDTRConfig(TIM2, &bd);  TIM_BDTRCmd(TIM2, ENABLE);

    /* Software-trigger break via BRKCLR=0 + falling edge expectation —
     * if no break input is wired, we just stop the counters here. */
    foc_pwm_stop();
}
