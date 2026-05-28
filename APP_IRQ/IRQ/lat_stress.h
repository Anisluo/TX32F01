/*
 * lat_stress.h — selectable background stressors for the IRQ-latency lab.
 *
 * Each stressor changes the state of the CPU at the moment SysTick fires,
 * which is what determines L_pipeline + L_block in the latency model.
 *
 *   NONE       cooperative scheduler ends up in WFI between ticks.
 *              → measures wakeup-from-sleep latency (the *best case*).
 *   BUSY       tight ALU loop that never sleeps.
 *              → measures latency with CPU mid-instruction (typical case).
 *   CRIT_64    PRIMASK held for ~64 cycles in a loop, with brief gaps.
 *              → histogram is bimodal: short bin (gap hit) + long bin (held hit).
 *   CRIT_256   same, ~256 cycles.
 *   CRIT_1024  same, ~1024 cycles. Worst case for missed RT deadlines.
 *
 * Hold durations are *approximate* (M0 pipeline timing varies slightly with
 * branch prediction & flash fetch). Read the histogram — measurement is
 * ground truth, the labels are just hints.
 */
#ifndef _LAT_STRESS_H
#define _LAT_STRESS_H

#include <stdint.h>

typedef enum {
    STRESS_NONE     = 0,
    STRESS_BUSY     = 1,
    STRESS_CRIT_64  = 2,
    STRESS_CRIT_256 = 3,
    STRESS_CRIT_1024 = 4,
    STRESS_COUNT
} stress_t;

void        lat_stress_set(stress_t s);
stress_t    lat_stress_get(void);
const char *lat_stress_name(stress_t s);

/* Hot path: COOP task body. Returns quickly enough that the scheduler
   keeps polling; for STRESS_NONE it returns immediately and the scheduler
   falls into WFI. */
void        lat_stress_step(void);

#endif
