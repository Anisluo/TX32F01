#ifndef APP_FOC_FOC_LOOP_H
#define APP_FOC_FOC_LOOP_H

#include "foc_config.h"
#include "foc_math.h"
#include <stdint.h>

typedef enum {
    FOC_MODE_IDLE = 0,    /* PWM at 50%, outputs OFF, safe */
    FOC_MODE_ALIGN,       /* park rotor at θ=0 by forcing Id */
    FOC_MODE_VF_OPEN,     /* open-loop V/F ramp from open-loop angle */
    FOC_MODE_FOC_CLOSED,  /* closed Id/Iq loop, angle from hall */
    FOC_MODE_FAULT,
} foc_mode_t;

typedef struct {
    /* Inputs */
    foc_mode_t mode;
    int16_t    id_ref_q15;
    int16_t    iq_ref_q15;
    uint16_t   vf_hz_elec;        /* open-loop V/F frequency */
    int16_t    speed_ref_rpm;     /* used in closed-loop top */

    /* Measurements (latest sample) */
    int16_t    id_meas, iq_meas;
    int16_t    ia_q15, ib_q15;
    uint16_t   vbus_raw;
    uint16_t   angle;
    uint8_t    hall_code;

    /* Outputs */
    int16_t    vd_q15, vq_q15;
    uint16_t   ta, tb, tc;

    /* Stats */
    uint32_t   loop_cnt;
    uint16_t   max_loop_cyc;      /* worst-case cycles measured by SysTick */
    uint16_t   fault_cnt;
} foc_state_t;

extern volatile foc_state_t g_foc;

void foc_loop_init(void);
void foc_loop_set_mode(foc_mode_t m);
void foc_loop_set_vf(uint16_t hz_elec);
void foc_loop_set_speed_ref(int16_t rpm);

#endif /* APP_FOC_FOC_LOOP_H */
