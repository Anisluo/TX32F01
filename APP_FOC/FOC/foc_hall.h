#ifndef APP_FOC_FOC_HALL_H
#define APP_FOC_FOC_HALL_H

#include "foc_config.h"
#include <stdint.h>

/*
 * Sensored position estimator for a 120° Hall configuration.  Maps the
 * 6 valid Hall states to one of 6 sectors of the electrical revolution,
 * places the rotor at the sector center, and integrates the latest
 * Hall-edge interval to estimate ω.  Good to ~5° at speed; at standstill
 * it just reports the last-known sector center.
 *
 *   Hall code  CBA   sector  center θ (deg)
 *   ──────────────  ──────  ───────────────
 *    101  ( 5 )       0          30
 *    001  ( 1 )       1          90
 *    011  ( 3 )       2         150
 *    010  ( 2 )       3         210
 *    110  ( 6 )       4         270
 *    100  ( 4 )       5         330
 *
 *   (000 and 111 are invalid and indicate a wiring / sensor fault.)
 */

int      foc_hall_init(void);

/* Current rotor electrical angle, 0..65535 ≡ 0..2π. */
uint16_t foc_hall_angle(void);

/* Filtered electrical rad/s in Q15-per-(2π Hz).  Convert to RPM via
 *   rpm = ω_q15 * f_pwm * 60 / pole_pairs / 32768
 * if you need a print value. */
int32_t  foc_hall_omega_q15(void);

/* Raw 3-bit hall code, for diagnostics. */
uint8_t  foc_hall_code(void);

/* Called once per PWM period from the ADC ISR.  Drives the
 * Hall-edge-interval timebase used by foc_hall_omega_q15(). */
void     foc_hall_tick(void);

#endif /* APP_FOC_FOC_HALL_H */
