/*
 * core_portme.c — Coremark port functions for TX32F01.
 *
 * Time keeping
 * ------------
 *   Cortex-M0 has SysTick (24-bit, DOWN-counter, no DWT). Coremark wants
 *   a free-running monotonic clock. We synthesize one by reloading
 *   SysTick to its max, polling it from get_time(), and accumulating
 *   wraps in software. This is enough resolution because Coremark only
 *   calls start_time/stop_time once per run.
 *
 *   At 24 MHz, SysTick wraps every (2^24)/24e6 ≈ 0.7 s. A run with
 *   ITERATIONS=400 lasts ~10 s ⇒ ~14 wraps, easily tracked in 32 bits.
 *
 * Output
 * ------
 *   Coremark prints results via ee_printf. We provide cm_printf — a
 *   tiny printf that supports %d %u %x %s %c, writing to the same UART
 *   that bench_core uses. No malloc, no float.
 */
#include "core_portme.h"
#include "TX32F01_periph.h"
#include <stdarg.h>

/* ------------------------------------------------------------------------- */
/*  SysTick-based monotonic timer                                            */
/* ------------------------------------------------------------------------- */
#define CM_RELOAD        0x00FFFFFFu

static volatile uint32_t s_wraps;   /* incremented from get_time when CVR appears to have wrapped */
static uint32_t          s_last_cvr;
static uint32_t          s_t_start;
static uint32_t          s_t_stop;

/* Convert the wrap counter + current CVR into a single monotonic 32-bit
 * tick value at core-clock rate (modulo 2^32 — won't wrap during a run). */
static uint32_t now_ticks(void)
{
    /* SysTick is DOWN-counter. Elapsed since last reload = RELOAD - CVR.
     * Each wrap adds (RELOAD + 1) elapsed ticks. */
    uint32_t cvr = SysTick->VAL;
    /* Detect wrap: if CVR > last_cvr, the counter rolled over. */
    if (cvr > s_last_cvr) s_wraps++;
    s_last_cvr = cvr;
    return s_wraps * (CM_RELOAD + 1u) + (CM_RELOAD - cvr);
}

void portable_init(void *p1, int *argc, char *argv[])
{
    (void)p1; (void)argc; (void)argv;
    /* Start a free-running SysTick. */
    SysTick->CTRL = 0;
    SysTick->LOAD = CM_RELOAD;
    SysTick->VAL  = 0;
    s_wraps    = 0;
    s_last_cvr = CM_RELOAD;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
}

void portable_fini(void *p1) { (void)p1; }

void start_time(void) { s_t_start = now_ticks(); }
void stop_time(void)  { s_t_stop  = now_ticks(); }

CORE_TICKS get_time(void) { return (CORE_TICKS)(s_t_stop - s_t_start); }

ee_f64 time_in_secs(CORE_TICKS ticks)
{
    /* 24 MHz core. Returned as double for the Coremark scoring formula. */
    return ((double)ticks) / 24000000.0;
}

/* ------------------------------------------------------------------------- */
/*  UART output                                                              */
/* ------------------------------------------------------------------------- */
static void cm_putc(char c)
{
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}

static void cm_puts(const char *s) { while (*s) cm_putc(*s++); }

static void cm_put_u32(uint32_t v, int base, int width, char pad)
{
    char buf[12]; int n = 0;
    if (v == 0) buf[n++] = '0';
    else {
        while (v && n < 12) {
            uint32_t d = v % (uint32_t)base;
            buf[n++] = (char)((d < 10) ? ('0' + d) : ('a' + d - 10));
            v /= (uint32_t)base;
        }
    }
    while (n < width) { cm_putc(pad); n++; }
    while (n--) cm_putc(buf[n]);
}

static void cm_put_s32(int32_t v, int width, char pad)
{
    if (v < 0) { cm_putc('-'); v = -v; if (width) width--; }
    cm_put_u32((uint32_t)v, 10, width, pad);
}

/* Minimal printf: %d %u %x %s %c %%. No %f. Width up to two digits. */
int cm_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    while (*fmt) {
        char ch = *fmt++;
        if (ch != '%') { cm_putc(ch); continue; }

        int  width = 0;
        char pad   = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        char spec = *fmt++;
        switch (spec) {
        case 'd': case 'i': cm_put_s32(va_arg(ap, int), width, pad); break;
        case 'u':           cm_put_u32(va_arg(ap, unsigned int), 10, width, pad); break;
        case 'x':           cm_put_u32(va_arg(ap, unsigned int), 16, width, pad); break;
        case 'c':           cm_putc((char)va_arg(ap, int)); break;
        case 's':           cm_puts(va_arg(ap, const char *)); break;
        case '%':           cm_putc('%'); break;
        default:            cm_putc('%'); cm_putc(spec); break;
        }
    }
    va_end(ap);
    return 0;
}
