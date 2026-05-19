/*
 * Cooperative round-robin scheduler for resource-constrained MCUs.
 *
 *   - No preemption, no stack switching, no dynamic memory.
 *   - Tasks must be non-blocking: enter, do one step, return.
 *   - Time base: 1 ms tick from SysTick (you call coop_tick() in ISR).
 *   - Period semantics: fixed-cadence (next_run += period), no drift.
 *
 * Typical use:
 *
 *     static void task_led (void) { LED_TOGGLE(); }
 *     static void task_print(void) { uart_puts("hello\r\n"); }
 *
 *     static coop_task_t s_tasks[] = {
 *         { task_led,   200, 0, 0 },   // every 200 ms
 *         { task_print, 1000, 0, 0 },  // every 1 s
 *     };
 *
 *     coop_init(s_tasks, sizeof(s_tasks)/sizeof(s_tasks[0]));
 *     // ... your SysTick must call coop_tick() once per ms ...
 *     coop_run();    // never returns
 */
#ifndef _COOP_SCHED_H
#define _COOP_SCHED_H

#include <stdint.h>

typedef void (*coop_func_t)(void);

typedef struct {
    coop_func_t func;          /* what to run */
    uint32_t    period_ms;     /* how often (0 = run every tick) */
    /* --- runtime fields, leave 0 at init --- */
    uint32_t    next_run_ms;
    uint32_t    run_count;     /* for debug; comment out if tight on RAM */
} coop_task_t;

/* Bind a static task array. Must be called once before coop_run(). */
void coop_init(coop_task_t *tasks, uint8_t ntasks);

/* Called from SysTick ISR exactly once per ms. */
void coop_tick(void);

/* Returns current ms-since-boot (32-bit, wraps after ~49 days). */
uint32_t coop_now_ms(void);

/* Main loop. Never returns. Sleeps via WFI between ticks. */
void coop_run(void);

/* Optional debug: percentage of CPU used in the last 1 s window (0-100). */
uint8_t coop_get_cpu_load(void);

#endif
