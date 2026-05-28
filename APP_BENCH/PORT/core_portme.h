/*
 * core_portme.h — EEMBC Coremark port for TX32F01 (Cortex-M0 @ 24 MHz).
 *
 * To use:
 *   1. Drop the official Coremark sources (core_main.c, core_list_join.c,
 *      core_matrix.c, core_state.c, core_util.c, coremark.h) into
 *      APP_BENCH/COREMARK/. Get them from
 *          https://github.com/eembc/coremark
 *      under the EEMBC Coremark license — read it before redistributing.
 *   2. Make sure THIS header (and core_portme.c) is on the include path
 *      ahead of any Coremark default port.
 *   3. From main(), call coremark's `main()` instead of bench_run_all(),
 *      OR call the renamed `coremark_main()` (recommended — rename
 *      core_main.c::main to coremark_main to avoid the C startup clash).
 *
 * The numbers below are calibrated for a 22 KB APP region. ITERATIONS is
 * intentionally low so a run completes in well under 60 s; bump it if
 * you have time and want lower statistical noise.
 */
#ifndef CORE_PORTME_H
#define CORE_PORTME_H

#include <stdint.h>
#include <stddef.h>

/* ----- EEMBC types ----- */
typedef int8_t      ee_s8;
typedef uint8_t     ee_u8;
typedef int16_t     ee_s16;
typedef uint16_t    ee_u16;
typedef int32_t     ee_s32;
typedef uint32_t    ee_u32;
typedef size_t      ee_size_t;
typedef double      ee_f64;        /* unused on M0; declared for API */

/* Coremark expects a timer type that grows with time. Cortex-M0 SysTick
 * counts DOWN, so we wrap it in a 32-bit virtual counter that ticks at
 * the core frequency. See core_portme.c::start_time / get_time. */
typedef ee_u32 CORE_TICKS;

/* ----- Compile-time knobs Coremark queries ----- */

/* Set MAIN_HAS_NOARGC=1 so the EEMBC main signature is `int main(void)`
 * (the alternative is the argc/argv form, which our startup won't pass). */
#define MAIN_HAS_NOARGC      1
#define MAIN_HAS_NORETURN    0

/* ITERATIONS=0 means Coremark will auto-scale to ~10 s. On a 24 MHz M0
 * with 22 KB Flash you almost certainly want to pin this manually.
 *
 *   400  → ~10 s on this part, decent statistics, no SysTick wrap.
 *   100  → ~3  s, fine for quick A/B comparisons of -O flags.
 *
 * Set via Keil "C/C++ Define" to override at build time. */
#ifndef ITERATIONS
#define ITERATIONS           400
#endif

/* The benchmark needs a memory area for its working set. Coremark's
 * smallest sensible footprint is around 2 KB. We have 4 KB SRAM total
 * and ~96 B already taken by the soft vector + a few hundred bytes of
 * stack — so 2 KB is what's left. */
#ifndef TOTAL_DATA_SIZE
#define TOTAL_DATA_SIZE      2000
#endif

#define MEM_LOCATION         "STATIC"
#define MEM_METHOD           MEM_STATIC

#define MULTITHREAD          1                /* single-threaded */
#define USE_PTHREAD          0
#define USE_FORK             0
#define USE_SOCKET           0

/* The "compiler info" string Coremark prints at the end. Fill what you
 * want shown alongside the result number. */
#define COMPILER_VERSION     "armcc"          /* or "armclang" */
#define COMPILER_FLAGS       "-O2"            /* set at build time too */

/* Number of physical cores. Always 1 on this part. */
#define HAS_FLOAT            0
#define HAS_TIME_H           0
#define HAS_STDIO            0                /* we route printf via UART manually */
#define HAS_PRINTF           1
#define SEED_METHOD          SEED_VOLATILE
#define MEM_METHOD_NAME      "STATIC"

/* ----- Time API used by core_main.c ----- */
void start_time(void);
void stop_time(void);
CORE_TICKS get_time(void);
ee_f64    time_in_secs(CORE_TICKS ticks);

/* ----- Coremark output sink. Wired to UART in core_portme.c. ----- */
void portable_init(void *p1, int *argc, char *argv[]);
void portable_fini(void *p1);

#define ee_printf            cm_printf      /* minimal printf in our port */
int cm_printf(const char *fmt, ...);

#endif
