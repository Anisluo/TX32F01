#ifndef APP_FOC_FOC_MATH_H
#define APP_FOC_FOC_MATH_H

#include <stdint.h>

/*
 * Q15 fixed-point math primitives.  No floats, no division.  All values
 * are in [-1, +1) range mapped to int16_t [-32768, 32767].
 *
 * Angle convention:  uint16_t,  0 .. 65535 ≡ 0 .. 2π,  CCW positive.
 */

#define Q15_ONE             ((int16_t)32767)
#define Q15_HALF            ((int16_t)16384)
#define Q15_SQRT3_OVER_2    ((int16_t)28377)   /* 0.8660254 * 32768 */
#define Q15_INV_SQRT3       ((int16_t)18919)   /* 0.5773503 * 32768 */
#define Q15_TWO_OVER_SQRT3  ((int16_t)0x7E8B)  /* 1.1547005 wrapped — see use site */

static __inline int16_t q15_clip(int32_t v)
{
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

/* Signed Q15 * Q15 → Q15, rounded. */
static __inline int16_t q15_mul(int16_t a, int16_t b)
{
    int32_t p = (int32_t)a * (int32_t)b;
    return (int16_t)((p + (1 << 14)) >> 15);
}

/* 256-entry quarter-resolution sine table is folded internally. */
int16_t foc_sin_q15(uint16_t angle);
int16_t foc_cos_q15(uint16_t angle);

/* Combined sin+cos in one call to share the table lookup. */
void    foc_sincos_q15(uint16_t angle, int16_t *s, int16_t *c);

/* Clarke transform (balanced, a+b+c=0): Iα = Ia, Iβ = (Ia + 2·Ib)/√3 */
static __inline void foc_clarke(int16_t ia, int16_t ib,
                                int16_t *i_alpha, int16_t *i_beta)
{
    *i_alpha = ia;
    /* (ia + 2*ib) can be up to 3·Q15 so use 32-bit, multiply by 1/√3 */
    int32_t sum = (int32_t)ia + (int32_t)ib + (int32_t)ib;
    *i_beta  = (int16_t)((sum * (int32_t)Q15_INV_SQRT3 + (1 << 14)) >> 15);
}

/* Park transform: d = α·cos+β·sin, q = -α·sin+β·cos */
static __inline void foc_park(int16_t a, int16_t b, int16_t s, int16_t c,
                              int16_t *d, int16_t *q)
{
    *d = q15_clip((int32_t)q15_mul(a, c) + (int32_t)q15_mul(b, s));
    *q = q15_clip((int32_t)q15_mul(b, c) - (int32_t)q15_mul(a, s));
}

/* Inverse Park: α = d·cos - q·sin, β = d·sin + q·cos */
static __inline void foc_ipark(int16_t d, int16_t q, int16_t s, int16_t c,
                               int16_t *a, int16_t *b)
{
    *a = q15_clip((int32_t)q15_mul(d, c) - (int32_t)q15_mul(q, s));
    *b = q15_clip((int32_t)q15_mul(d, s) + (int32_t)q15_mul(q, c));
}

/* PI controller, Q15.  Anti-windup by clamping integrator. */
typedef struct {
    int16_t kp, ki;
    int16_t out_max, out_min;
    int32_t integ;       /* internal, Q15 * loop iterations */
} foc_pi_t;

static __inline void foc_pi_reset(foc_pi_t *p)
{
    p->integ = 0;
}

int16_t foc_pi_step(foc_pi_t *p, int16_t err);

/*
 * SVPWM via min-mid-max zero-sequence injection.
 *
 *   v_alpha, v_beta in Q15 normalized to ±1 (max amplitude = √(α²+β²) ≤ √3/2
 *   for full hexagon utilization).
 *   arr_top = PWM ARR (== period-1).
 *   Returns three high-side compare duties t_a/t_b/t_c in [0, arr_top+1].
 */
void foc_svpwm(int16_t v_alpha, int16_t v_beta, uint16_t arr_top,
               uint16_t *t_a, uint16_t *t_b, uint16_t *t_c);

#endif /* APP_FOC_FOC_MATH_H */
