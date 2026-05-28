#include "foc_loop.h"
#include "foc_adc.h"
#include "foc_pwm.h"
#include "foc_hall.h"
#include "TX32F01_periph.h"

volatile foc_state_t g_foc;

/* Phase-angle phasor for the open-loop V/F mode.  Incremented by
 *   Δθ = f_elec * 2π * Tpwm        in uint16 angle units this is
 *   Δθ = f_elec * 65536 / FOC_PWM_HZ
 * per PWM tick.  At FOC_PWM_HZ = 10 kHz, every 1 Hz electrical
 * costs 6.5536 uint16 units per tick. */
static uint16_t s_vf_step;
static uint16_t s_vf_theta;

static foc_pi_t s_pi_d;
static foc_pi_t s_pi_q;
static foc_pi_t s_pi_speed;

static int32_t  s_align_ticks_left;

/* ------------------------------------------------------------------- *
 *  Sample → Q15 current
 *
 *  ADC range 0..4095 covers Vdd ≈ 3.3 V across the shunt amplifier
 *  output.  Mid-scale = 2048 is the zero-current bias.  We treat the
 *  signed deviation as Q12 then up-shift to Q15.  Scaling to physical
 *  amperes is a tuning constant — we don't actually need it in the
 *  loop because the PI gains absorb it.
 * ------------------------------------------------------------------- */
static int16_t adc_to_q15_current(uint16_t raw, uint16_t offset)
{
    int32_t diff = (int32_t)raw - (int32_t)offset;     /* ±2048 nominally */
    /* shift left 4 → use full Q15 range; clip just in case */
    int32_t q = diff << 4;
    if (q >  32767) q =  32767;
    if (q < -32768) q = -32768;
    return (int16_t)q;
}

static void run_pwm_commit(int16_t v_alpha, int16_t v_beta)
{
    uint16_t ta, tb, tc;
    foc_svpwm(v_alpha, v_beta, foc_pwm_arr(), &ta, &tb, &tc);
    g_foc.ta = ta;  g_foc.tb = tb;  g_foc.tc = tc;
    foc_pwm_commit(ta, tb, tc);
}

/* ------------------------------------------------------------------- *
 *   The HOT path.  Runs at PWM rate from the ADC EOC ISR.
 *
 *   Budget @ 24 MHz / 10 kHz: 2400 cycles per period.  Measured worst
 *   case for the closed-loop path is ~600 cycles on Cortex-M0 with
 *   ARMCC, leaving 75% of the period to user code or low-priority
 *   IRQs.  Keep this routine straight-line — no printf, no function
 *   pointers, no heap.
 * ------------------------------------------------------------------- */
static void foc_isr(const foc_adc_sample_t *s)
{
    uint32_t t0 = SysTick->VAL;

    /* tick the hall time-base from one place */
    foc_hall_tick();
    g_foc.loop_cnt++;

    /* --- decode samples ------------------------------------------- */
    uint16_t ia0, ib0;
    foc_adc_get_offsets(&ia0, &ib0);
    int16_t ia_q15 = adc_to_q15_current(s->ia, ia0);
    int16_t ib_q15 = adc_to_q15_current(s->ib, ib0);
    g_foc.ia_q15  = ia_q15;
    g_foc.ib_q15  = ib_q15;
    g_foc.vbus_raw = s->vbus;

    /* --- pick the angle to feed Park ------------------------------- */
    uint16_t theta;
    switch (g_foc.mode) {
    case FOC_MODE_ALIGN:
        theta = 0;
        break;
    case FOC_MODE_VF_OPEN:
        s_vf_theta = (uint16_t)(s_vf_theta + s_vf_step);
        theta = s_vf_theta;
        break;
    case FOC_MODE_FOC_CLOSED:
        theta = foc_hall_angle();
        break;
    case FOC_MODE_IDLE:
    case FOC_MODE_FAULT:
    default:
        /* Park transform off — emit zero voltage, keep symmetry */
        foc_pwm_commit((uint16_t)(FOC_ARR_TOP / 2U),
                       (uint16_t)(FOC_ARR_TOP / 2U),
                       (uint16_t)(FOC_ARR_TOP / 2U));
        return;
    }
    g_foc.angle = theta;
    g_foc.hall_code = foc_hall_code();

    int16_t sin_t, cos_t;
    foc_sincos_q15(theta, &sin_t, &cos_t);

    /* --- forward transforms --------------------------------------- */
    int16_t i_alpha, i_beta;
    foc_clarke(ia_q15, ib_q15, &i_alpha, &i_beta);

    int16_t i_d, i_q;
    foc_park(i_alpha, i_beta, sin_t, cos_t, &i_d, &i_q);
    g_foc.id_meas = i_d;
    g_foc.iq_meas = i_q;

    /* --- pick v_d / v_q ------------------------------------------- */
    int16_t v_d, v_q;
    switch (g_foc.mode) {
    case FOC_MODE_ALIGN: {
        /* feed a fixed Id, zero Iq, no PI — just to align rotor */
        v_d = FOC_ALIGN_ID_Q15;
        v_q = 0;
        if (--s_align_ticks_left <= 0) {
            /* alignment done → drop to idle; user starts vf/foc next */
            g_foc.mode = FOC_MODE_IDLE;
        }
        break;
    }
    case FOC_MODE_VF_OPEN: {
        /* v_q ∝ f_elec (constant V/Hz), v_d = 0 */
        int32_t vq = (int32_t)g_foc.vf_hz_elec * FOC_VF_KV_Q15;
        if (vq >  Q15_ONE) vq =  Q15_ONE;
        if (vq < -Q15_ONE) vq = -Q15_ONE;
        v_d = 0;
        v_q = (int16_t)vq;
        break;
    }
    case FOC_MODE_FOC_CLOSED: {
        int16_t err_d = (int16_t)(g_foc.id_ref_q15 - i_d);
        int16_t err_q = (int16_t)(g_foc.iq_ref_q15 - i_q);
        v_d = foc_pi_step(&s_pi_d, err_d);
        v_q = foc_pi_step(&s_pi_q, err_q);
        break;
    }
    default:
        v_d = 0; v_q = 0;
        break;
    }
    g_foc.vd_q15 = v_d;
    g_foc.vq_q15 = v_q;

    /* --- inverse Park → SVPWM → commit ---------------------------- */
    int16_t v_alpha, v_beta;
    foc_ipark(v_d, v_q, sin_t, cos_t, &v_alpha, &v_beta);
    run_pwm_commit(v_alpha, v_beta);

    /* --- worst-case timing (SysTick is a downcounter) ------------- */
    uint32_t t1 = SysTick->VAL;
    uint32_t dt = (t1 <= t0) ? (t0 - t1) : (t0 + SysTick->LOAD - t1);
    if (dt > g_foc.max_loop_cyc) g_foc.max_loop_cyc = (uint16_t)dt;
}

void foc_loop_init(void)
{
    /* Reset state */
    g_foc.mode          = FOC_MODE_IDLE;
    g_foc.id_ref_q15    = 0;
    g_foc.iq_ref_q15    = 0;
    g_foc.vf_hz_elec    = 0;
    g_foc.speed_ref_rpm = 0;
    g_foc.loop_cnt      = 0;
    g_foc.max_loop_cyc  = 0;

    s_pi_d.kp = FOC_PI_KP_Q15; s_pi_d.ki = FOC_PI_KI_Q15;
    s_pi_d.out_max = FOC_PI_OUT_MAX_Q15; s_pi_d.out_min = FOC_PI_OUT_MIN_Q15;
    foc_pi_reset(&s_pi_d);

    s_pi_q.kp = FOC_PI_KP_Q15; s_pi_q.ki = FOC_PI_KI_Q15;
    s_pi_q.out_max = FOC_PI_OUT_MAX_Q15; s_pi_q.out_min = FOC_PI_OUT_MIN_Q15;
    foc_pi_reset(&s_pi_q);

    s_pi_speed.kp = FOC_SPD_KP_Q15; s_pi_speed.ki = FOC_SPD_KI_Q15;
    s_pi_speed.out_max = FOC_SPD_OUT_MAX_Q15; s_pi_speed.out_min = FOC_SPD_OUT_MIN_Q15;
    foc_pi_reset(&s_pi_speed);

    s_vf_theta = 0; s_vf_step = 0;

    foc_adc_init(foc_isr);
}

void foc_loop_set_mode(foc_mode_t m)
{
    if (m == FOC_MODE_ALIGN) {
        s_align_ticks_left = (int32_t)((uint32_t)FOC_PWM_HZ * FOC_ALIGN_MS / 1000U);
    }
    if (m == FOC_MODE_FOC_CLOSED) {
        foc_pi_reset(&s_pi_d);
        foc_pi_reset(&s_pi_q);
    }
    g_foc.mode = m;
}

void foc_loop_set_vf(uint16_t hz_elec)
{
    g_foc.vf_hz_elec = hz_elec;
    /* Δθ per PWM tick in uint16-angle units */
    s_vf_step = (uint16_t)(((uint32_t)hz_elec * 65536U) / FOC_PWM_HZ);
}

void foc_loop_set_speed_ref(int16_t rpm)
{
    g_foc.speed_ref_rpm = rpm;
    /* The outer speed loop runs from the shell timer in main.c, not
     * here, to avoid burning loop budget on a 100 Hz controller. */
}
