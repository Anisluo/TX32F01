#include "foc_math.h"
#include "foc_config.h"

/*
 * Quarter-sine table, 64 entries × Q15, covers 0..π/2.  The look-up
 * function folds the remaining three quadrants via symmetry.  Total ROM
 * cost = 128 B.
 */
static const int16_t s_sin_q15[64] = {
        0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
     6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
    18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
    32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
};

static int16_t sin_lookup_256(uint8_t idx)
{
    uint8_t quad = idx >> 6;            /* 0..3 */
    uint8_t sub  = idx & 0x3F;          /* 0..63 */
    int16_t v;
    switch (quad) {
    case 0: v =  s_sin_q15[sub];                                        break;
    case 1: v =  (sub == 0) ? Q15_ONE :  s_sin_q15[64 - sub];           break;
    case 2: v = -s_sin_q15[sub];                                        break;
    default:v =  (sub == 0) ? -Q15_ONE : (int16_t)-s_sin_q15[64 - sub]; break;
    }
    return v;
}

int16_t foc_sin_q15(uint16_t angle)
{
    return sin_lookup_256((uint8_t)(angle >> 8));
}

int16_t foc_cos_q15(uint16_t angle)
{
    /* cos(θ) = sin(θ + π/2);  π/2 = 64 indices in our 256-step table. */
    return sin_lookup_256((uint8_t)((angle >> 8) + 64U));
}

void foc_sincos_q15(uint16_t angle, int16_t *s, int16_t *c)
{
    uint8_t idx = (uint8_t)(angle >> 8);
    *s = sin_lookup_256(idx);
    *c = sin_lookup_256((uint8_t)(idx + 64U));
}

int16_t foc_pi_step(foc_pi_t *p, int16_t err)
{
    int32_t prop   = (int32_t)q15_mul(err, p->kp);
    int32_t i_step = (int32_t)q15_mul(err, p->ki);
    int32_t inew   = p->integ + i_step;
    int32_t out    = prop + inew;

    if (out > p->out_max) {
        out = p->out_max;
        if (i_step > 0) inew = p->integ;       /* freeze integrator on + sat */
    } else if (out < p->out_min) {
        out = p->out_min;
        if (i_step < 0) inew = p->integ;       /* freeze integrator on − sat */
    }
    p->integ = inew;
    return (int16_t)out;
}

void foc_svpwm(int16_t v_alpha, int16_t v_beta, uint16_t arr_top,
               uint16_t *t_a, uint16_t *t_b, uint16_t *t_c)
{
    /* ---- Inverse Clarke → phase voltages, Q15 ----------------------- */
    int32_t va = v_alpha;
    int32_t sqrt3_vb_2 = (int32_t)q15_mul(v_beta, Q15_SQRT3_OVER_2);
    int32_t vb = -(va >> 1) + sqrt3_vb_2;
    int32_t vc = -(va >> 1) - sqrt3_vb_2;

    /* ---- Common-mode min-mid-max injection -------------------------- *
     * Mathematically equivalent to standard SVPWM but uses only adds,
     * shifts and comparisons — no divisions, no sector tables, no sqrt.
     * Extends modulation range from ±1/2 Vbus to ±√3/2 Vbus / per phase.
     */
    int32_t vmn = va, vmx = va;
    if (vb < vmn) vmn = vb; else if (vb > vmx) vmx = vb;
    if (vc < vmn) vmn = vc; else if (vc > vmx) vmx = vc;
    int32_t voff = (vmn + vmx) >> 1;
    va -= voff;  vb -= voff;  vc -= voff;

    /* ---- Map Q15 [-1,+1] → duty [0, period] via one 32-bit multiply -- *
     * Trick: (signed_q15 + 32768) is uint16 in [0, 65535];
     *        (uint16 * period) >> 16 gives uint16 in [0, period].
     * No division, no float.
     */
    uint32_t period = (uint32_t)arr_top + 1U;
    uint32_t da = ((uint32_t)((int32_t)va + 32768) * period) >> 16;
    uint32_t db = ((uint32_t)((int32_t)vb + 32768) * period) >> 16;
    uint32_t dc = ((uint32_t)((int32_t)vc + 32768) * period) >> 16;

    /* Reserve dead-time margin on both ends to avoid 0%/100% glitches */
    uint32_t lo = FOC_DEADTIME_TCK;
    uint32_t hi = period - FOC_DEADTIME_TCK;
    if (da < lo) da = lo; else if (da > hi) da = hi;
    if (db < lo) db = lo; else if (db > hi) db = hi;
    if (dc < lo) dc = lo; else if (dc > hi) dc = hi;

    *t_a = (uint16_t)da;
    *t_b = (uint16_t)db;
    *t_c = (uint16_t)dc;
}
