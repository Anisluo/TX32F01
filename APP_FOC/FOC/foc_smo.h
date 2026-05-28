/*
 * foc_smo.h — Sliding-Mode Observer (SMO) for sensorless PMSM rotor
 * angle estimation. Designed to run inside the FOC ADC ISR at 10 kHz
 * on a 24 MHz Cortex-M0 with no FPU.
 *
 *
 * The textbook PMSM model in the stationary (α, β) frame
 * --------------------------------------------------------
 *   L · dIα/dt = -R·Iα + Vα - Eα
 *   L · dIβ/dt = -R·Iβ + Vβ - Eβ
 *
 *   where Eα = -ψ·ω·sin(θ), Eβ = ψ·ω·cos(θ)
 *
 * The SMO replaces the unknown back-EMF with a discontinuous injection
 * term and forces the estimated current to track the measured one:
 *
 *   L · dÎα/dt = -R·Îα + Vα - Z·g(Îα - Iα)
 *
 * When the surface (Îα - Iα = 0) is reached, the equivalent value of
 * Z·g(·) equals the unknown EMF. A low-pass filter extracts it. We use
 * a saturated sigmoid for g(·) instead of pure sign() — kills the
 * chattering you'd otherwise get on a 12-bit ADC.
 *
 *
 * Discrete-time form (Ts = 1 / FOC_PWM_HZ = 100 µs)
 * --------------------------------------------------
 *   Îα[k+1] = F·Îα[k] + G·(Vα[k] - Z·sat(Îα[k] - Iα[k]))
 *
 *   F = 1 − R·Ts/L       (≈ 0.5 for a typical hobby BLDC at 10 kHz)
 *   G =     Ts/L         (= 1.0  in normalized units)
 *
 *
 * Angle extraction
 * ----------------
 *   Naive:    θ̂ = atan2(-Eα, Eβ)
 *   Better:   PLL drives θ̂ so that  err = Eα·cos(θ̂) + Eβ·sin(θ̂) → 0,
 *             which removes the LPF phase lag and produces ω̂ as a
 *             byproduct.
 *
 *
 * Cycle budget at 24 MHz, inside the 100 µs FOC ISR
 * --------------------------------------------------
 *   smo_step           ≈ 250 cycles  (≈ 10 µs)
 *   leaves ≥ 80 µs for Park/Clarke + PI + SVPWM. Verify with SysTick CVR.
 *
 *
 * What this module does NOT do
 * ----------------------------
 *   - Startup. SMO needs Eα,Eβ which need ω which needs motion. The
 *     practical recipe is I/f open-loop → sense BEMF → blend → close.
 *     foc_smo_startup helper handles the blend; the I/f ramp itself
 *     lives in foc_loop.
 *   - Parameter ID. R, L, ψ must be known or measured beforehand.
 *   - Field weakening (high-speed angle drift compensation).
 */
#ifndef APP_FOC_FOC_SMO_H
#define APP_FOC_FOC_SMO_H

#include <stdint.h>
#include "../MATH/q15_math.h"

/* ------------------------------------------------------------------------- */
/*  Tunable parameters — pick reasonable defaults for a small BLDC.          */
/*  ALL of these must be re-tuned for the actual motor + Vbus + Ts.          */
/* ------------------------------------------------------------------------- */

/* Plant model. F = 1 - R·Ts/L. G = Ts/L. Both in Q15.
 * For R = 0.5 Ω, L = 100 µH, Ts = 100 µs : F = 0.5 → 16384, G = 1.0 → 32767.
 * If G ≥ 1.0 you must pre-scale voltage; see q15_mul_sat below. */
#ifndef SMO_F_Q15
#define SMO_F_Q15        ((q15_t)16384)
#endif
#ifndef SMO_G_Q15
#define SMO_G_Q15        ((q15_t)16384)   /* 0.5 — keeps math in-range */
#endif

/* Switching gain Z. Higher = faster convergence + more chatter.
 * Should be ≥ max|EMF| in normalized units. */
#ifndef SMO_Z_Q15
#define SMO_Z_Q15        ((q15_t)12000)   /* ≈ 0.37 */
#endif

/* Saturation slope. The sigmoid g(x) = sat(x · 2^SMO_SAT_SHIFT) over
 * the range [-1, +1]. Using a left-shift instead of a divide avoids
 * the Cortex-M0 software divide call (~120 cycles → ~3 cycles).
 *   shift = 4  →  δ ≈ 0.06  →  gentle, low chatter
 *   shift = 6  →  δ ≈ 0.015 →  nearly sgn(), fast convergence but chatters */
#ifndef SMO_SAT_SHIFT
#define SMO_SAT_SHIFT    4
#endif

/* Back-EMF LPF coefficient α: y[k] = y[k-1] + α(x - y[k-1]).
 * α ≈ 2π·fc·Ts. For fc = 500 Hz, Ts = 100 µs : α = 0.314 → 10295.
 * Smaller α → smoother but more phase lag. */
#ifndef SMO_EMF_LPF_Q15
#define SMO_EMF_LPF_Q15  ((q15_t)10000)
#endif

/* PLL gains. Drive θ̂ so that the projection err = Eα·cos(θ̂) + Eβ·sin(θ̂)
 * goes to zero. Kp affects bandwidth, Ki removes steady-state lag. */
#ifndef SMO_PLL_KP_Q15
#define SMO_PLL_KP_Q15   ((q15_t)6000)
#endif
#ifndef SMO_PLL_KI_Q15
#define SMO_PLL_KI_Q15   ((q15_t)100)
#endif

/* ------------------------------------------------------------------------- */
/*  Observer state                                                           */
/* ------------------------------------------------------------------------- */
typedef struct {
    /* Current estimates Îα, Îβ in Q15 (same scale as the measured ones). */
    q15_t    i_alpha_hat;
    q15_t    i_beta_hat;

    /* Filtered back-EMF. Sign convention matches Eα = -ψω·sin(θ). */
    q15_t    e_alpha;
    q15_t    e_beta;

    /* PLL */
    int32_t  pll_integ;          /* ω̂ integrator, Q15·sample */
    int16_t  omega_q15;          /* ω̂ in Q15-per-sample (== angle step) */
    uint16_t theta_hat;          /* estimated rotor electrical angle */

    /* Health */
    uint16_t locked_cnt;         /* incremented while |err| < threshold */
} foc_smo_t;

/* ------------------------------------------------------------------------- */
/*  API                                                                      */
/* ------------------------------------------------------------------------- */

/* Reset all state. Call once at boot and whenever you re-arm the motor. */
void foc_smo_init(foc_smo_t *o);

/* One sample. Call from the FOC ADC ISR with the latest measured
 * stationary-frame currents and the most recent commanded voltages
 * (Vα, Vβ from inverse Park). Updates Î, Ê, θ̂, ω̂ in place. */
void foc_smo_step(foc_smo_t *o,
                  q15_t i_alpha, q15_t i_beta,
                  q15_t v_alpha, q15_t v_beta);

/* True when the observer has been within the lock band continuously for
 * at least n_samples — safe to hand the inner loop the SMO angle. */
static __inline int foc_smo_is_locked(const foc_smo_t *o, uint16_t n_samples)
{
    return o->locked_cnt >= n_samples;
}

/* Smooth handoff from I/f open-loop angle to the SMO estimate. Returns
 * a blended angle that ramps from theta_iface (weight = 1) to
 * theta_smo (weight = 0) as 'blend_q15' walks 0 → 32767. Caller drives
 * the blend variable over a few hundred ms once foc_smo_is_locked().
 *
 * The handoff is done on the *delta* between the two estimates so wrap
 * is handled correctly — naive linear blend of two angles would jump
 * across 0/2π boundaries. */
uint16_t foc_smo_blend_angle(uint16_t theta_iface, uint16_t theta_smo,
                             q15_t blend_q15);

#endif /* APP_FOC_FOC_SMO_H */
