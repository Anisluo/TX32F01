/*
 * q15_math.c — see q15_math.h for design rationale.
 *
 * Tables
 * ------
 *   sin_qtr[i] for i in 0..64 = round(32767 * sin(i * π / 128)).
 *   This is sin over [0, π/2] sampled every 1.40625°. Endpoint included
 *   so the linear interpolator never goes out of bounds.
 *
 *   atan_cordic[i] = round(atan(2^-i) / (2π) * 65536) for i in 0..15.
 *   Standard vectoring-CORDIC angle table.
 *
 * Cycle budget at 24 MHz (measured on similar M0+ cores; rough numbers
 * to size the ISR — verify on this part with SysTick CVR before relying):
 *   q15_sin_cos   ~70 cycles  (two LUT fetches + two interps)
 *   q15_sqrt_u32  ~150 cycles
 *   q15_atan2     ~250 cycles
 */
#include "q15_math.h"

/* ------------------------------------------------------------------------- */
/*  sin table: 65 entries covering [0, π/2]                                  */
/* ------------------------------------------------------------------------- */
static const int16_t sin_qtr[65] = {
        0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
     6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
    12540, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767
};

/* CORDIC angles: atan(2^-i) re-scaled into our uint16_t-per-circle format.
 * atan(2^-i) / (2π) * 65536, rounded. */
static const uint16_t atan_cordic[16] = {
    0x2000, 0x12E4, 0x09FB, 0x0511, 0x028B, 0x0145, 0x00A3, 0x0051,
    0x0029, 0x0014, 0x000A, 0x0005, 0x0003, 0x0001, 0x0001, 0x0000
};

/* ------------------------------------------------------------------------- */
/*  Internal helper: sine over the full circle using the quarter table.     */
/* ------------------------------------------------------------------------- */
static q15_t sin_full(uint16_t angle)
{
    /* angle layout: 0x0000..0xFFFF spans 0..2π.
     *   bit 15        = sign of result (= top bit of half-cycle)
     *   bit 14        = mirror (back-fold inside the half-cycle)
     *   bits 13..0    = phase inside quadrant, 0..16383 == 0..π/2
     */
    uint16_t phase = angle & 0x3FFFu;       /* 0..16383 */
    if (angle & 0x4000u) {
        /* second half of the half-cycle — mirror */
        phase = 0x4000u - phase;
    }

    uint16_t idx  = phase >> 8;             /* 0..64 */
    uint16_t frac = phase & 0xFFu;          /* 0..255 */

    int32_t a0 = sin_qtr[idx];
    int32_t a1 = sin_qtr[(idx < 64u) ? (idx + 1u) : 64u];
    int32_t v  = a0 + (((a1 - a0) * (int32_t)frac) >> 8);

    if (angle & 0x8000u) v = -v;
    return (q15_t)v;
}

/* ------------------------------------------------------------------------- */
/*  Public sin / cos / sincos                                               */
/* ------------------------------------------------------------------------- */
void q15_sin_cos(uint16_t angle, q15_t *sin_out, q15_t *cos_out)
{
    if (sin_out) *sin_out = sin_full(angle);
    if (cos_out) *cos_out = sin_full((uint16_t)(angle + 0x4000u));
}

q15_t q15_sin(uint16_t angle) { return sin_full(angle); }
q15_t q15_cos(uint16_t angle) { return sin_full((uint16_t)(angle + 0x4000u)); }

/* ------------------------------------------------------------------------- */
/*  Integer sqrt, floor(sqrt(x))                                            */
/* ------------------------------------------------------------------------- */
uint16_t q15_sqrt_u32(uint32_t x)
{
    /* Digit-by-digit "long division" style. No multiply, no divide.
     * Standard textbook implementation; constant time per bit pair. */
    uint32_t root = 0;
    uint32_t bit  = 1UL << 30;

    while (bit > x) bit >>= 2;

    while (bit) {
        if (x >= root + bit) {
            x   -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (uint16_t)root;
}

/* ------------------------------------------------------------------------- */
/*  atan2 via vectoring CORDIC                                              */
/* ------------------------------------------------------------------------- */
uint16_t q15_atan2(int32_t y, int32_t x)
{
    if (x == 0 && y == 0) return 0;

    uint16_t a = 0;

    /* Pre-rotate so the vector ends up in the right half-plane (x > 0).
     * Each pre-rotation is a ±90° step. After this, CORDIC operates
     * within the principal range −π/2..+π/2 where 16 iters converge. */
    if (x < 0) {
        int32_t t;
        if (y >= 0) {
            /* Quadrant II: rotate by −π/2.  (x,y) → (y, −x). */
            t = x; x = y; y = -t;
            a = 0x4000u;       /* +π/2 */
        } else {
            /* Quadrant III: rotate by +π/2. (x,y) → (−y, x). */
            t = x; x = -y; y = t;
            a = 0xC000u;       /* −π/2 (== 3π/2 in our wraparound) */
        }
    }

    /* Vectoring loop: drive y toward 0, accumulate angle. */
    for (int i = 0; i < 16; i++) {
        int32_t tx = x;
        int32_t ty = y;
        if (y > 0) {
            x = tx + (ty >> i);
            y = ty - (tx >> i);
            a = (uint16_t)(a + atan_cordic[i]);
        } else if (y < 0) {
            x = tx - (ty >> i);
            y = ty + (tx >> i);
            a = (uint16_t)(a - atan_cordic[i]);
        } else {
            break;
        }
    }
    return a;
}
