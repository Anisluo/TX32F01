/*
 * lat_meas.h — IRQ entry-latency sampler.
 *
 * Design:
 *   SysTick is configured as the periodic high-priority IRQ (priority 0).
 *   On every handler entry the very first action reads SYST_CVR. Since
 *   SysTick counts down at SYSCLK, the elapsed cycles since the underflow
 *   event are LOAD - CVR. That is the total entry latency, including:
 *       L_arch + L_trampoline + L_pipeline + L_preempt + L_block.
 *
 * One-pass online stats: min, max, count, running sum (avg = sum/count),
 * plus a 16-bin histogram with 8-cycle bins covering 0..127 cyc. Latencies
 * ≥ 128 fall into the "outlier" bucket and you should look at max for them.
 *
 * Thread safety: lat_meas_record() is ISR-only. lat_meas_snapshot() copies
 * under PRIMASK so callers see a consistent picture.
 */
#ifndef _LAT_MEAS_H
#define _LAT_MEAS_H

#include <stdint.h>

#define LAT_BUCKETS         16U
#define LAT_BUCKET_WIDTH    8U      /* cycles per bin */

typedef struct {
    uint32_t count;
    uint32_t min, max;
    uint32_t last;                  /* most recent sample, for live monitoring */
    uint64_t sum;
    uint32_t bucket[LAT_BUCKETS];   /* bin i covers [i*8, i*8+8) cycles */
    uint32_t outliers;              /* samples ≥ 128 cyc */
} lat_stats_t;

/* Must be called before SysTick is enabled. Passes the SYST_RVR value
   (== SysTick_Config's `ticks - 1`). Resets stats. */
void lat_meas_init(uint32_t systick_rvr);

/* Called as the FIRST line of the SysTick handler. Reads CVR internally. */
void lat_meas_record(void);

/* Atomic copy of current stats. */
void lat_meas_snapshot(lat_stats_t *out);

/* Reset stats (but keep RVR config). */
void lat_meas_reset(void);

#endif
