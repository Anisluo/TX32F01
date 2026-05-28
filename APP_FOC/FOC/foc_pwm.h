#ifndef APP_FOC_FOC_PWM_H
#define APP_FOC_FOC_PWM_H

#include "foc_config.h"
#include <stdint.h>

/*
 * 3-phase complementary PWM via TIM0/TIM1/TIM2.  Each timer drives one
 * half-bridge — TX32F01 only has one OCx + OCxN pair per timer, so we
 * burn all three.  Periods are identical and started in lock-step;
 * residual phase skew between timers is in the single-cycle range
 * because the start sequence is back-to-back CR writes.
 *
 * Polarity convention (matches the existing PWM examples):
 *   OCx  initial = HIGH  (high-side gate)
 *   OCxN initial = LOW   (low-side  gate)
 *   At CCR match, both flip.  Dead-time is inserted by hardware DTG.
 *
 *   "High side ON" time per period  =  arr_top + 1 - CCR    (in tim ticks)
 *
 *   ⇒ commit:    CCR = arr_top + 1 - t_high_on
 */

/* Hardware tries to drive all three outputs to known levels on break.
 * Returns 0 on success. */
int  foc_pwm_init(void);

/* Apply the three high-side ON-times (already SVPWM-shaped) for the
 * next period.  Pre-load on the timers makes the new values latch on
 * the next update event, so transitions are atomic. */
void foc_pwm_commit(uint16_t t_a, uint16_t t_b, uint16_t t_c);

/* Master enable.  All three timers are armed together with their
 * CR.CEN bits set in successive store instructions.  Slight (~3 cyc)
 * skew between channels is acceptable for sensored FOC. */
void foc_pwm_start(void);
void foc_pwm_stop(void);

/* Force outputs low immediately (uses BDTR break-clear path).  Safer
 * than just stopping the counters because pins go to OCxBreakState. */
void foc_pwm_force_safe(void);

/* PWM ARR.  Equal to FOC_ARR_TOP and exposed for the math layer. */
static __inline uint16_t foc_pwm_arr(void) { return (uint16_t)FOC_ARR_TOP; }

#endif /* APP_FOC_FOC_PWM_H */
