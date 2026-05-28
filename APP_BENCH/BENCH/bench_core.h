/*
 * bench_core.h — built-in CPU benchmark harness for TX32F01.
 *
 * Why this exists alongside Coremark
 * -----------------------------------
 *   Coremark gives one big composite number per chip that's good for
 *   inter-vendor comparison. But during day-to-day tuning of the FOC
 *   ISR you want to know things like:
 *     - "how many cycles is q15_mul actually costing me?"
 *     - "does the compiler emit a software-divide here?"
 *     - "what's the cost of a Park transform end-to-end?"
 *   That's what this harness measures. Each test reports cycles/iter
 *   so you can budget your ISR confidently.
 *
 * Timing source
 * -------------
 *   We use SysTick (CVR) — counts down at 24 MHz, 24-bit (wraps every
 *   ~700 ms). The harness reloads SysTick to its max value, runs the
 *   workload, reads CVR, and computes (RELOAD − CVR) = elapsed cycles.
 *   Each test is repeated N times to amortize call overhead.
 *
 * Output
 * ------
 *   void bench_run_all(void (*putc)(char));
 *   prints a table like:
 *
 *     test                  iters    cycles    cyc/iter
 *     -------------------------------------------------
 *     q15_mul                4096      8430        2.06
 *     q15_sin_cos            4096    303104       74.00
 *     q15_sqrt_u32           4096    618496      151.00
 *     q15_atan2              1024    268288      262.00
 *     memcpy_256B            1024    495616      484.00
 *     bubble_sort_64          512   1245184     2432.00
 *     crc32_1KB               256     78848      308.00
 *
 *   These numbers are illustrative — actual values depend on -O level.
 *
 * Memory footprint
 * ----------------
 *   ~1.5 KB ROM, ~520 B RAM (test buffers). Fits inside APP_BENCH with
 *   room for Coremark on top.
 */
#ifndef APP_BENCH_BENCH_CORE_H
#define APP_BENCH_BENCH_CORE_H

#include <stdint.h>

/* Run every benchmark and stream a report through `putc`. Synchronous. */
void bench_run_all(void (*putc_fn)(char));

/* Helper exposed in case you want to time a custom snippet:
 *   bench_start();
 *   ... your code ...
 *   uint32_t cyc = bench_end();
 */
void     bench_start(void);
uint32_t bench_end(void);

#endif
