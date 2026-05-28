#ifndef APP_FOC_FOC_ADC_H
#define APP_FOC_FOC_ADC_H

#include "foc_config.h"
#include <stdint.h>

/*
 * 3-channel ADC sequence (Ia, Ib, Vbus), triggered by TIM0 master mode.
 * EOC IRQ runs the FOC loop — the latest samples land in the static
 * fields below before the ISR you register via foc_adc_set_cb() is
 * invoked.  No DMA on this part, so we eat the EOC IRQ at PWM rate.
 *
 *   PWM update  ── TIM0 trigger ── ADC sequence ──┐
 *                                                  │ EOC IRQ
 *                                                  ▼
 *                                            foc_loop_isr()
 */

typedef struct {
    uint16_t ia;      /* raw counts, 12-bit right-aligned */
    uint16_t ib;
    uint16_t vbus;
} foc_adc_sample_t;

typedef void (*foc_adc_cb_t)(const foc_adc_sample_t *s);

int  foc_adc_init(foc_adc_cb_t cb);

/* Run once with PWM at 50% but disabled output to estimate the Ia/Ib
 * zero-current ADC code.  Stored in static state; call before align. */
void foc_adc_calibrate_offset(void);

/* For diagnostics from the shell. */
void foc_adc_get_offsets(uint16_t *ia0, uint16_t *ib0);

#endif /* APP_FOC_FOC_ADC_H */
