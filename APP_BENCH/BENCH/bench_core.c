/*
 * bench_core.c — see bench_core.h
 *
 * Implementation notes
 * --------------------
 *   SysTick is configured for the maximum 24-bit reload (0x00FFFFFF).
 *   Each individual test must run in < 700 ms or the counter wraps;
 *   the iteration counts below are sized accordingly for 24 MHz.
 *
 *   We use `volatile` on accumulator sinks so the optimizer can't
 *   discard the workload. If you see suspiciously low numbers, check
 *   that the compiler isn't constant-folding the entire test.
 *
 *   The q15_math source needs to be on the include path. We don't
 *   link APP_FOC directly — copy the math files into APP_BENCH or use
 *   a relative include (the README shows both).
 */
#include "bench_core.h"
#include "TX32F01_periph.h"
#include "../../APP_FOC/MATH/q15_math.h"

/* ------------------------------------------------------------------------- */
/*  Cycle counter via SysTick                                                */
/* ------------------------------------------------------------------------- */
#define BENCH_SYSTICK_RELOAD  0x00FFFFFFu     /* 24-bit max */

void bench_start(void)
{
    SysTick->CTRL = 0;                        /* disable */
    SysTick->LOAD = BENCH_SYSTICK_RELOAD;
    SysTick->VAL  = 0;                        /* writing CVR also clears countflag */
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk |
                    SysTick_CTRL_CLKSOURCE_Msk;   /* core clock, no IRQ */
}

uint32_t bench_end(void)
{
    uint32_t cvr = SysTick->VAL;
    SysTick->CTRL = 0;
    return (BENCH_SYSTICK_RELOAD - cvr) & BENCH_SYSTICK_RELOAD;
}

/* ------------------------------------------------------------------------- */
/*  Tiny report formatter — no printf, no malloc                             */
/* ------------------------------------------------------------------------- */
static void (*g_putc)(char);

static void p_str(const char *s) { while (*s) g_putc(*s++); }

static void p_u32(uint32_t v)
{
    char b[11]; int n = 0;
    if (!v) { g_putc('0'); return; }
    while (v && n < 10) { b[n++] = (char)('0' + v % 10U); v /= 10U; }
    while (n--) g_putc(b[n]);
}

/* width-right-padded u32 */
static void p_u32w(uint32_t v, int width)
{
    char b[11]; int n = 0;
    if (!v) b[n++] = '0';
    else while (v && n < 10) { b[n++] = (char)('0' + v % 10U); v /= 10U; }
    while (n < width) { g_putc(' '); n++; }
    while (n--) g_putc(b[n]);
}

/* Decimal: cycles_per_iter as XXX.YY (two fractional digits). */
static void p_cpi(uint32_t cyc, uint32_t iters)
{
    uint32_t int_part  = cyc / iters;
    uint32_t remainder = cyc - int_part * iters;
    uint32_t frac      = (remainder * 100U + iters / 2U) / iters;
    p_u32w(int_part, 8);
    g_putc('.');
    if (frac < 10) g_putc('0');
    p_u32(frac);
}

static void report(const char *name, uint32_t iters, uint32_t cyc)
{
    p_str("  ");
    /* pad name to 18 chars */
    int i = 0;
    while (name[i] && i < 18) { g_putc(name[i]); i++; }
    while (i < 18) { g_putc(' '); i++; }
    p_u32w(iters, 6);
    p_str("  ");
    p_u32w(cyc,   10);
    p_str("  ");
    p_cpi(cyc, iters);
    p_str("\r\n");
}

/* ------------------------------------------------------------------------- */
/*  Test workloads                                                           */
/* ------------------------------------------------------------------------- */

/* Sinks must be volatile so the optimizer can't constant-fold the loop. */
static volatile int32_t  s_sink32;
static volatile uint32_t s_sinku32;
static volatile uint16_t s_sinku16;
static volatile q15_t    s_sinkq15a;
static volatile q15_t    s_sinkq15b;

static uint32_t test_q15_mul(uint32_t iters)
{
    q15_t a = 23170;  /* sin(45°)  */
    q15_t b = -16384; /* -0.5      */
    q15_t acc = 0;
    bench_start();
    for (uint32_t i = 0; i < iters; i++) {
        acc = q15_mul(acc + a, b);
    }
    s_sinkq15a = acc;
    return bench_end();
}

static uint32_t test_q15_sin_cos(uint32_t iters)
{
    uint16_t a = 0;
    q15_t s, c;
    bench_start();
    for (uint32_t i = 0; i < iters; i++) {
        q15_sin_cos(a, &s, &c);
        a += 0x281;       /* step ~0.5° each iter, hits all quadrants */
    }
    s_sinkq15a = s; s_sinkq15b = c;
    return bench_end();
}

static uint32_t test_q15_sqrt(uint32_t iters)
{
    uint32_t x = 1u;
    uint16_t r = 0;
    bench_start();
    for (uint32_t i = 0; i < iters; i++) {
        r ^= q15_sqrt_u32(x);
        x = x * 1664525u + 1013904223u;   /* LCG to vary input */
    }
    s_sinku16 = r;
    return bench_end();
}

static uint32_t test_q15_atan2(uint32_t iters)
{
    int32_t x = 0x1000, y = 0;
    uint16_t acc = 0;
    bench_start();
    for (uint32_t i = 0; i < iters; i++) {
        acc += q15_atan2(y, x);
        /* simple rotation to cycle through angles */
        int32_t nx = x - (y >> 4);
        int32_t ny = y + (x >> 4);
        x = nx; y = ny;
    }
    s_sinku16 = acc;
    return bench_end();
}

/* ------- memcpy 256 B ------- */
static uint8_t  s_buf_src[256];
static uint8_t  s_buf_dst[256];

static uint32_t test_memcpy_256(uint32_t iters)
{
    for (int i = 0; i < 256; i++) s_buf_src[i] = (uint8_t)i;
    bench_start();
    for (uint32_t i = 0; i < iters; i++) {
        uint8_t *d = s_buf_dst;
        const uint8_t *s = s_buf_src;
        for (int j = 0; j < 256; j++) *d++ = *s++;
    }
    s_sinku32 = (uint32_t)s_buf_dst[(iters - 1) & 0xFFu];
    return bench_end();
}

/* ------- bubble sort 64 elements ------- */
static uint16_t s_sort_buf[64];

static uint32_t test_bubble_sort(uint32_t iters)
{
    uint32_t cyc_total = 0;
    for (uint32_t k = 0; k < iters; k++) {
        /* Refill with reverse-sorted data: worst case for bubble sort. */
        for (int i = 0; i < 64; i++) s_sort_buf[i] = (uint16_t)(63 - i);
        bench_start();
        for (int i = 0; i < 64 - 1; i++) {
            for (int j = 0; j < 64 - 1 - i; j++) {
                if (s_sort_buf[j] > s_sort_buf[j + 1]) {
                    uint16_t t = s_sort_buf[j];
                    s_sort_buf[j]     = s_sort_buf[j + 1];
                    s_sort_buf[j + 1] = t;
                }
            }
        }
        cyc_total += bench_end();
    }
    s_sinku16 = s_sort_buf[0];
    return cyc_total;
}

/* ------- CRC32 over 1 KB ------- */
static uint8_t s_crc_buf[1024];

static uint32_t crc32_run(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (n--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static uint32_t test_crc32_1kb(uint32_t iters)
{
    for (int i = 0; i < 1024; i++) s_crc_buf[i] = (uint8_t)(i * 31);
    uint32_t acc = 0;
    bench_start();
    for (uint32_t i = 0; i < iters; i++) {
        acc ^= crc32_run(s_crc_buf, 1024);
    }
    s_sinku32 = acc;
    return bench_end();
}

/* ------- End-to-end synthetic FOC inner loop ----------
 * 1× Clarke + 1× Park + 2× PI + 1× inverse Park, on Q15. Approximates
 * what your ADC ISR does between SVPWM updates (without the SVPWM
 * itself, which is timer-write bound, not CPU bound). */
static uint32_t test_foc_inner(uint32_t iters)
{
    uint16_t theta = 0;
    int32_t  iq_int = 0, id_int = 0;
    q15_t    ia = 1000, ib = -800;
    int32_t  vd_acc = 0, vq_acc = 0;
    bench_start();
    for (uint32_t i = 0; i < iters; i++) {
        /* Clarke (a,b) → (α,β) */
        q15_t ialpha = ia;
        q15_t ibeta  = q15_mul((q15_t)18919, (q15_t)(ia + 2 * ib)); /* 1/√3 ≈ 0.5774 */

        /* Park (α,β) → (d,q) */
        q15_t s, c;
        q15_sin_cos(theta, &s, &c);
        q15_t id_meas =  q15_mul(ialpha, c) + q15_mul(ibeta, s);
        q15_t iq_meas = -q15_mul(ialpha, s) + q15_mul(ibeta, c);

        /* PI on Id, Iq (kp=0.1, ki=0.005, Q15) */
        int32_t id_err = (int32_t)0 - id_meas;
        int32_t iq_err = (int32_t)5000 - iq_meas;
        id_int += (id_err * 164) >> 15;     /* ki */
        iq_int += (iq_err * 164) >> 15;
        int32_t vd = ((id_err * 3277) >> 15) + id_int;   /* kp */
        int32_t vq = ((iq_err * 3277) >> 15) + iq_int;
        if (vd >  28000) vd =  28000; else if (vd < -28000) vd = -28000;
        if (vq >  28000) vq =  28000; else if (vq < -28000) vq = -28000;

        /* Inverse Park (d,q) → (α,β) */
        int32_t valpha = q15_mul((q15_t)vd, c) - q15_mul((q15_t)vq, s);
        int32_t vbeta  = q15_mul((q15_t)vd, s) + q15_mul((q15_t)vq, c);
        vd_acc += valpha; vq_acc += vbeta;

        theta += 0x80;   /* advance angle */
    }
    s_sink32 = vd_acc ^ vq_acc;
    return bench_end();
}

/* ------------------------------------------------------------------------- */
/*  Driver                                                                   */
/* ------------------------------------------------------------------------- */
void bench_run_all(void (*putc_fn)(char))
{
    g_putc = putc_fn;

    p_str("\r\n==============================================================\r\n");
    p_str(" TX32F01 CPU baseline @ 24 MHz, SysTick cycle counter\r\n");
    p_str("==============================================================\r\n");
    p_str("  test                iters       cycles     cyc/iter\r\n");
    p_str("  ------------------------------------------------------------\r\n");

    report("q15_mul",         4096,  test_q15_mul(4096));
    report("q15_sin_cos",     4096,  test_q15_sin_cos(4096));
    report("q15_sqrt_u32",    4096,  test_q15_sqrt(4096));
    report("q15_atan2",       1024,  test_q15_atan2(1024));
    report("memcpy_256B",     1024,  test_memcpy_256(1024));
    report("bubble_sort_64",   512,  test_bubble_sort(512));
    report("crc32_1KB",        256,  test_crc32_1kb(256));
    report("foc_inner_loop",  4096,  test_foc_inner(4096));

    p_str("==============================================================\r\n");
    p_str(" Tips:\r\n");
    p_str("   * lower cyc/iter is better.\r\n");
    p_str("   * if q15_sin_cos > 100 cyc/iter, your -O level is too low.\r\n");
    p_str("   * foc_inner_loop * PWM_HZ must fit your ISR budget:\r\n");
    p_str("       at 10 kHz PWM and 24 MHz core, budget = 2400 cyc/loop.\r\n");
    p_str("==============================================================\r\n\r\n");
}
