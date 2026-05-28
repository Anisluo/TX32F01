/*
 * q15_math.h — Q15 fixed-point math for TX32F01 (Cortex-M0, 24 MHz, no FPU).
 *
 * Why this exists
 * ---------------
 *   Cortex-M0 has no FPU and no hardware divider. Anything that needs
 *   sin/cos/sqrt/atan2 inside the FOC ADC ISR must be fixed-point and
 *   table-driven, or the loop won't close in 50 µs at 24 MHz.
 *
 * Number formats
 * --------------
 *   q15_t    — signed Q1.15. Range [-1.0, +1.0 − 2^-15], encoded as int16_t.
 *              +1.0 is approximated as 32767. Multiplication is
 *              ((int32)a * (int32)b) >> 15.
 *   angle    — uint16_t covering one full electrical revolution. 0x0000 = 0,
 *              0x4000 = π/2, 0x8000 = π, 0xC000 = 3π/2. Wraps automatically
 *              on add/sub, which is the point — angles are modular.
 *
 * Conventions
 * -----------
 *   - All public functions are pure (no globals touched).
 *   - sin/cos use a 65-entry quarter-wave LUT + linear interp. RMS error
 *     vs float sin is < 5e-5 over the full circle. ROM cost 130 B.
 *   - atan2 uses 16-iter vectoring CORDIC. Angle error < 1 LSB of uint16_t.
 *   - sqrt is digit-by-digit (no divide, constant time ~150 cycles).
 *
 * Intended call sites in the FOC pipeline
 * ---------------------------------------
 *   Park   :  q15_sin_cos(angle, &s, &c);
 *             Id =  q15_mul(Ialpha, c) + q15_mul(Ibeta, s);
 *             Iq = -q15_mul(Ialpha, s) + q15_mul(Ibeta, c);
 *   inv-Park: Valpha = q15_mul(Vd, c) - q15_mul(Vq, s);
 *             Vbeta  = q15_mul(Vd, s) + q15_mul(Vq, c);
 *   amplitude  : mag = q15_sqrt_u32((uint32_t)Vd*Vd + (uint32_t)Vq*Vq);
 *   angle wrap : just add/sub uint16_t — modular by design.
 */
#ifndef Q15_MATH_H
#define Q15_MATH_H

#include <stdint.h>

typedef int16_t q15_t;

#define Q15_ONE      ((q15_t) 32767)
#define Q15_NEG_ONE  ((q15_t)-32768)
#define Q15_HALF     ((q15_t) 16384)

/* Q15 multiply: (a * b) shifted right by 15. No saturation — caller must
 * ensure operands won't produce |result| > 1.0. For the FOC inner loop
 * this is naturally satisfied since SVPWM clips voltages and currents are
 * bounded by the shunt amp. */
static __inline q15_t q15_mul(q15_t a, q15_t b)
{
    return (q15_t)(((int32_t)a * (int32_t)b) >> 15);
}

/* Saturating variant. Clips to [-1, +1]. ~3-4 cycles more than q15_mul. */
static __inline q15_t q15_mul_sat(q15_t a, q15_t b)
{
    int32_t r = ((int32_t)a * (int32_t)b) >> 15;
    if (r >  32767) return  32767;
    if (r < -32768) return -32768;
    return (q15_t)r;
}

/* Clamp signed 32-bit to Q15 range. Useful after accumulators. */
static __inline q15_t q15_sat32(int32_t v)
{
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return (q15_t)v;
}

/* Single-quadrant sine + cosine in Q15, given a uint16_t angle covering
 * the full electrical circle. Branch-light; suitable for the ADC ISR. */
void q15_sin_cos(uint16_t angle, q15_t *sin_out, q15_t *cos_out);

/* Convenience scalar versions if you only need one. */
q15_t q15_sin(uint16_t angle);
q15_t q15_cos(uint16_t angle);

/* Integer square root, floor(sqrt(x)). 32-bit input, 16-bit output.
 * Constant time, no divide. Use for vector magnitudes like sqrt(Vd^2+Vq^2). */
uint16_t q15_sqrt_u32(uint32_t x);

/* atan2: returns angle in the same uint16_t format as q15_sin_cos accepts.
 * Vectoring CORDIC, 16 iterations. Handles all four quadrants and the
 * y=0,x<0 (= π) edge. Returns 0 if both inputs are zero. */
uint16_t q15_atan2(int32_t y, int32_t x);

#endif
