/*
 * foc_smo.c — see foc_smo.h
 *
 * Math identity check
 * -------------------
 *   At lock, the equivalent injection equals back-EMF:
 *      E = Z · sat((Î - I) / δ)
 *   For a salient or unbalanced motor add a feed-forward term;
 *   this implementation assumes surface-PM with negligible saliency.
 *
 * Numerical notes
 * ---------------
 *   Everything stays Q15. Intermediate products are int32 and we shift
 *   right by 15 (or use q15_mul). For the discrete observer step we
 *   keep an int32 accumulator so we don't lose bits in the rolling sum.
 *
 *   This file is deliberately allocation-free and uses no globals other
 *   than the const tables in q15_math.c. Any number of foc_smo_t
 *   instances can coexist (e.g. dual-motor).
 */
#include "foc_smo.h"

/* ------------------------------------------------------------------------- */
/*  Saturated sigmoid g(x) ≈ sat(x · 2^SHIFT) — replaces sgn() to kill       */
/*  chatter. Left-shift avoids the M0 software divide; ~3 cycles total.      */
/* ------------------------------------------------------------------------- */
static __inline q15_t smo_sat(q15_t x)
{
    int32_t y = (int32_t)x << SMO_SAT_SHIFT;
    if (y >  Q15_ONE)            return  Q15_ONE;
    if (y < -(int32_t)Q15_ONE)   return (q15_t)-Q15_ONE;
    return (q15_t)y;
}

/* ------------------------------------------------------------------------- */
/*  Init / reset                                                             */
/* ------------------------------------------------------------------------- */
void foc_smo_init(foc_smo_t *o)
{
    o->i_alpha_hat = 0;
    o->i_beta_hat  = 0;
    o->e_alpha     = 0;
    o->e_beta      = 0;
    o->pll_integ   = 0;
    o->omega_q15   = 0;
    o->theta_hat   = 0;
    o->locked_cnt  = 0;
}

/* ------------------------------------------------------------------------- */
/*  One observer + PLL step. Critical-path ordering matters for jitter.      */
/* ------------------------------------------------------------------------- */
void foc_smo_step(foc_smo_t *o,
                  q15_t i_alpha, q15_t i_beta,
                  q15_t v_alpha, q15_t v_beta)
{
    /* --- 1. Error between estimated and measured currents --- */
    q15_t err_a = (q15_t)(o->i_alpha_hat - i_alpha);
    q15_t err_b = (q15_t)(o->i_beta_hat  - i_beta);

    /* --- 2. Injection term  z·g(err) --- */
    q15_t inj_a = q15_mul(SMO_Z_Q15, smo_sat(err_a));
    q15_t inj_b = q15_mul(SMO_Z_Q15, smo_sat(err_b));

    /* --- 3. Observer step:  Î[k+1] = F·Î[k] + G·(V - inj) --- */
    int32_t drv_a = (int32_t)v_alpha - (int32_t)inj_a;
    int32_t drv_b = (int32_t)v_beta  - (int32_t)inj_b;

    int32_t next_a = (int32_t)q15_mul(SMO_F_Q15, o->i_alpha_hat) +
                     ((int32_t)SMO_G_Q15 * drv_a >> 15);
    int32_t next_b = (int32_t)q15_mul(SMO_F_Q15, o->i_beta_hat) +
                     ((int32_t)SMO_G_Q15 * drv_b >> 15);

    o->i_alpha_hat = q15_sat32(next_a);
    o->i_beta_hat  = q15_sat32(next_b);

    /* --- 4. Back-EMF LPF.  At lock, inj ≈ EMF up to sign.       ---
     *      Sign convention: Eα = -ψω·sin(θ),  Eβ =  ψω·cos(θ).
     *      The injection points the *other* way (it opposes the
     *      measured-vs-estimated error), hence the negation here.
     */
    q15_t e_a_raw = (q15_t)-inj_a;
    q15_t e_b_raw = (q15_t)-inj_b;

    /* y += α(x - y) */
    o->e_alpha = (q15_t)(o->e_alpha +
                         q15_mul(SMO_EMF_LPF_Q15, (q15_t)(e_a_raw - o->e_alpha)));
    o->e_beta  = (q15_t)(o->e_beta  +
                         q15_mul(SMO_EMF_LPF_Q15, (q15_t)(e_b_raw - o->e_beta)));

    /* --- 5. PLL: drive θ̂ so that  err_pll = Eα·cos(θ̂) + Eβ·sin(θ̂) → 0.
     *      The dot product is zero exactly when θ̂ matches the EMF angle.
     */
    q15_t s, c;
    q15_sin_cos(o->theta_hat, &s, &c);

    int32_t err_pll = (int32_t)q15_mul(o->e_alpha, c) +
                      (int32_t)q15_mul(o->e_beta,  s);

    /* PI on the PLL error → ω̂. err_pll is already small (≪ Q15_ONE),
     * so we don't need extra scaling. Integrator stays in int32. */
    o->pll_integ += ((int32_t)SMO_PLL_KI_Q15 * err_pll) >> 15;

    int32_t omega = ((int32_t)SMO_PLL_KP_Q15 * err_pll >> 15) +
                    (o->pll_integ >> 15);

    /* Clip so a runaway integrator can't blow past one full rev per sample. */
    if (omega >  8192) omega =  8192;   /* < 1/8 rev per Ts = 1.25 kHz elec */
    if (omega < -8192) omega = -8192;
    o->omega_q15 = (int16_t)omega;

    /* Angle wraps modulo 2π automatically thanks to uint16_t arithmetic. */
    o->theta_hat = (uint16_t)(o->theta_hat + (uint16_t)(int16_t)omega);

    /* --- 6. Lock detector ---
     *      |err_pll| < threshold for several samples → consider locked.
     */
    int32_t abs_err = err_pll < 0 ? -err_pll : err_pll;
    if (abs_err < 800) {
        if (o->locked_cnt < 0xFFFFu) o->locked_cnt++;
    } else {
        o->locked_cnt = 0;
    }
}

/* ------------------------------------------------------------------------- */
/*  Blend two angles using the SHORTEST arc, so we never sweep the wrong way */
/*  across the 0/2π boundary during handoff.                                 */
/* ------------------------------------------------------------------------- */
uint16_t foc_smo_blend_angle(uint16_t theta_iface, uint16_t theta_smo,
                             q15_t blend_q15)
{
    /* delta = signed shortest distance from iface to smo (wraps correctly
     * because of int16 cast). */
    int16_t delta = (int16_t)(theta_smo - theta_iface);

    /* Step from iface toward smo by (blend_q15 / Q15_ONE) of the delta. */
    int16_t step = (int16_t)(((int32_t)delta * (int32_t)blend_q15) >> 15);
    return (uint16_t)(theta_iface + (uint16_t)step);
}
