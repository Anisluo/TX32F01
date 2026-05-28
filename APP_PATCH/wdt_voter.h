/*
 * wdt_voter.h — multi-module software watchdog voter for TX32F01.
 *
 * Why a voter on top of IWDT
 * --------------------------
 *   The hardware IWDT resets the chip if nobody feeds it. That's a
 *   coarse, last-resort safety net — it tells you "something is dead"
 *   but not WHICH something. In a real firmware multiple subsystems
 *   need to be making forward progress (FOC loop, shell, telemetry,
 *   logger…); if any one of them stalls, the chip should restart and
 *   the post-mortem should name the guilty module.
 *
 *   This module sits in between:
 *
 *     module A ──checkin─┐
 *     module B ──checkin─┼─► wdt_voter ─(all alive?)─► IWDT feed
 *     module C ──checkin─┘                         │
 *                                                  └─(no, stalled)
 *                                                     │
 *                                            tag fault_dump, panic
 *
 *   The voter only feeds the IWDT when every registered module has
 *   checked in within its declared deadline. If any module exceeds its
 *   deadline, the voter stamps the offending module name into
 *   fault_dump and triggers a captured HardFault, so the next boot's
 *   dump tells you exactly who stalled and at what PC.
 *
 * Typical lifecycle
 * -----------------
 *   wdt_voter_init(2000);                              // 2 s IWDT hard bound
 *   wdt_token_t foc_tok    = wdt_register("foc",  20); // FOC must check in every 20 ms
 *   wdt_token_t shell_tok  = wdt_register("shell",500);
 *   wdt_token_t logger_tok = wdt_register("log",  100);
 *
 *   // in SysTick / 1 ms tick:
 *   wdt_voter_tick(1);
 *
 *   // wherever the FOC outer loop runs:
 *   wdt_checkin(foc_tok);
 *
 * Failure modes covered
 * ---------------------
 *   - One task deadlocked: that task fails to checkin → voter panics
 *     with its name → reset → next boot dumps "module = wdt:foc".
 *   - Whole system hung (including wdt_voter_tick itself): IWDT fires
 *     on its own. Slower path but always reaches reset.
 *   - wdt_voter_tick called too often: harmless, just inflates the
 *     checkin "freshness" check.
 *
 * Failure modes NOT covered
 * -------------------------
 *   - Live-lock where a task IS checking in but spinning on garbage —
 *     the voter only sees the checkin pulse, not the work. Couple this
 *     with task-level invariant checks for full coverage.
 *   - Reentrancy from an ISR that calls a registered task's code path.
 *     wdt_checkin is IRQ-safe (single store) but the higher-level
 *     "is this work actually progress" question isn't.
 */
#ifndef APP_PATCH_WDT_VOTER_H
#define APP_PATCH_WDT_VOTER_H

#include <stdint.h>

#define WDT_MAX_MODULES    8

typedef int8_t wdt_token_t;     /* −1 = registration failed */

/* Initialize the voter and arm the hardware IWDT.
 *
 *   iwdt_timeout_ms — outer hardware bound. Must be > max(module deadlines)
 *                     + slack for the voter tick rate. 2× max deadline is
 *                     a good starting point.
 *
 *   The IWDT clock on TX32F01 is the LSI (≈ 32 kHz). We pick a prescaler
 *   and reload value to get as close to iwdt_timeout_ms as possible.
 *
 *   Idempotent: safe to call once at boot. Don't call again later — the
 *   IWDT can only be configured before enable.
 */
void wdt_voter_init(uint16_t iwdt_timeout_ms);

/* Register one module. Returns a token used by wdt_checkin, or −1 if
 * the table is full or the name is NULL. `name` must point to a string
 * with lifetime ≥ until reset (use a string literal). `deadline_ms` is
 * the maximum wall time between successive checkins. */
wdt_token_t wdt_register(const char *name, uint16_t deadline_ms);

/* Module heartbeat. O(1), IRQ-safe — does one volatile store.
 * Invalid token is silently ignored so it's safe to no-op a module by
 * stashing −1. */
void wdt_checkin(wdt_token_t tok);

/* Periodic voter pulse. Pass the wall-time elapsed since the last call,
 * in milliseconds. Typically called from SysTick (1 ms) so just pass 1.
 *
 * What it does:
 *   1. Adds `elapsed_ms` to every module's "ms since last checkin".
 *   2. If any module > its deadline → panic (see below).
 *   3. Otherwise feeds the hardware IWDT.
 *
 * Panic path:
 *   - Calls fault_dump_set_module("wdt:<name>") so the next boot's
 *     dump names the stalled module.
 *   - Triggers a HardFault via fault_dump_trigger_test().
 *   - fault_dump's asm trampoline records R4-R11, the C handler records
 *     the HW-stacked frame and resets.
 *
 *   The stack dump in the record points at the wdt_voter_tick context,
 *   NOT the stalled module. The module *name* is the actionable info.
 *   For deeper context, call wdt_set_breadcrumb() from the module at
 *   key progress points (see below).
 */
void wdt_voter_tick(uint16_t elapsed_ms);

/* Optional: record a 32-bit breadcrumb value at each checkin so the
 * dump can show what the module was last doing. Pure debug aid — the
 * voter never inspects this value. */
void wdt_set_breadcrumb(wdt_token_t tok, uint32_t crumb);

/* Read-only access for shell / telemetry. Returns 0 if the token is
 * invalid. ms_since may exceed deadline if you're sampling during a
 * stall (you'll see it just before the voter panics on the next tick). */
uint16_t wdt_ms_since(wdt_token_t tok);
uint32_t wdt_breadcrumb(wdt_token_t tok);
const char *wdt_name(wdt_token_t tok);
uint8_t  wdt_module_count(void);

#endif
