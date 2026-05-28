#ifndef APP_FOC_FOC_CONFIG_H
#define APP_FOC_FOC_CONFIG_H

#include "TX32F01_periph.h"

/* =====================================================================
 *  System / timing
 * =====================================================================*/
#define FOC_SYSCLK_HZ        24000000U
#define FOC_TIM_PRESC        3U                          /* DIV+1, gives 8 MHz tim clock */
#define FOC_TIM_CLK_HZ       (FOC_SYSCLK_HZ / FOC_TIM_PRESC)
#define FOC_PWM_HZ           10000U                      /* 10 kHz PWM = 100 us loop */
#define FOC_ARR_TOP          ((FOC_TIM_CLK_HZ / FOC_PWM_HZ) - 1U)   /* 799 */
#define FOC_DEADTIME_TCK     20U                         /* ~2.5 us — TUNE TO YOUR BOARD */

/* =====================================================================
 *  Motor parameters (used only for documentation / scaling — TUNE these
 *  before hooking up a real motor)
 * =====================================================================*/
#define FOC_MOTOR_POLE_PAIRS 4U      /* common hobby BLDC */
#define FOC_SHUNT_OHMS_mOHM  10U     /* 0.010 Ω shunt */
#define FOC_AMP_GAIN_X10     200U    /* 20.0 V/V op-amp */
#define FOC_VBUS_DIV_NUM     1U
#define FOC_VBUS_DIV_DEN     11U     /* 10:1 divider, ADC sees Vbus/11 */

/* =====================================================================
 *  Pin mapping — edit to your demo board.  Cross-check with HAL_GPIO.h.
 *  Defaults follow the existing PWM example for phase U, and reasonable
 *  next pins for V/W given the small AF table.
 * =====================================================================*/

/* Phase U — TIM0 ----------------------------------------------------- */
#define FOC_U_HI_PORT        GPIO2
#define FOC_U_HI_PIN         PIN02
#define FOC_U_HI_AF          GPIO_AF_T0CH
#define FOC_U_LO_PORT        GPIO0
#define FOC_U_LO_PIN         PIN03
#define FOC_U_LO_AF          GPIO_AF_T0CHN

/* Phase V — TIM1 -----------------------------------------------------
 *
 * ⚠️ DO NOT EVER USE GPIO0.PIN00 OR GPIO0.PIN01 ⚠️
 *
 *   Those two pins are the chip's SWDIO / SWCLK. Reconfiguring them
 *   to ANY function (output / AF / analog) kills the J-Link connection
 *   instantly. Every vendor demo in TX32F01_DemoBoard_Lib carefully
 *   avoids them — verify with `grep -r "GPIO0.*PIN0[01]" TX32F01_DemoBoard_Lib/`
 *   (returns nothing). Earlier revisions of this file had Phase V on
 *   GPIO0.PIN00/01 and shipped a brick: chip ran fine (LED blinked),
 *   but the debug pod could no longer find the device.
 *
 * Current assignment:
 *   V_HI = GPIO3.PIN05 (T1CH)  — confirmed safe by vendor demo
 *                                7.Timer/3.TIM_Capture/HARDWARE/PWM/PWM.c
 *   V_LO = GPIO1.PIN03 (T1CHN) — PLACEHOLDER. Vendor demos never wire
 *                                T1CHN anywhere, so the actual pin that
 *                                supports T1CHN must be read out of the
 *                                official TX32F01 datasheet (AF table).
 *                                Verify before driving a real motor.
 */
#define FOC_V_HI_PORT        GPIO3
#define FOC_V_HI_PIN         PIN05
#define FOC_V_HI_AF          GPIO_AF_T1CH
#define FOC_V_LO_PORT        GPIO1
#define FOC_V_LO_PIN         PIN03
#define FOC_V_LO_AF          GPIO_AF_T1CHN

/* Phase W — TIM2 ----------------------------------------------------- */
#define FOC_W_HI_PORT        GPIO1
#define FOC_W_HI_PIN         PIN05
#define FOC_W_HI_AF          GPIO_AF_T2CH
#define FOC_W_LO_PORT        GPIO1
#define FOC_W_LO_PIN         PIN04
#define FOC_W_LO_AF          GPIO_AF_T2CHN

/* Phase currents Ia/Ib + Vbus on ADC ---------------------------------- */
#define FOC_IA_PORT          GPIO1
#define FOC_IA_PIN           PIN00
#define FOC_IA_CH            ADC_CH_AN5_P10
#define FOC_IB_PORT          GPIO1
#define FOC_IB_PIN           PIN01
#define FOC_IB_CH            ADC_CH_AN6_P11
#define FOC_VBUS_PORT        GPIO1
#define FOC_VBUS_PIN         PIN02
#define FOC_VBUS_CH          ADC_CH_AN7_P12

/* Hall sensors A/B/C  ------------------------------------------------- */
#define FOC_HALL_A_PORT      GPIO2
#define FOC_HALL_A_PIN       PIN00
#define FOC_HALL_A_EXTI      0
#define FOC_HALL_A_IRQN      EXTI0_IRQn

#define FOC_HALL_B_PORT      GPIO2
#define FOC_HALL_B_PIN       PIN01
#define FOC_HALL_B_EXTI      1
#define FOC_HALL_B_IRQN      EXTI1_IRQn

/* Hall C uses GPIO3.PIN02 → EXTI line 2 (lines map 1:1 to pin numbers;
 * CFGR selects which port supplies each line). The previous (line=6,
 * IRQ=EXTI6) was a config bug — line 6 with port=GPIO3 means EXTI6
 * watches GPIO3.PIN06, which is our UART TX. That made every TX byte
 * fire spurious "hall" IRQs and corrupt the rotor angle. */
#define FOC_HALL_C_PORT      GPIO3
#define FOC_HALL_C_PIN       PIN02
#define FOC_HALL_C_EXTI      2
#define FOC_HALL_C_IRQN      EXTI2_IRQn

/* =====================================================================
 *  Control loop tunables.  Q15 unless noted.
 * =====================================================================*/

/* Sample current → Q15:  i_q15 = (adc - offset) * Q15_PER_AMP / 32768
 * We just store ADC counts and treat as signed Q15 around zero. */
#define FOC_ADC_FULLSCALE    4095U          /* 12-bit */
#define FOC_ADC_MIDSCALE     2048U          /* bipolar zero */

/* PI gains for Id/Iq (Q15·count → Q15 voltage) */
#define FOC_PI_KP_Q15        ((int16_t)(0.10f * 32768.0f))   /* 0.10 */
#define FOC_PI_KI_Q15        ((int16_t)(0.005f * 32768.0f))  /* 0.005 */
#define FOC_PI_OUT_MAX_Q15   ((int16_t) 28000)               /* leave headroom for IPark */
#define FOC_PI_OUT_MIN_Q15   ((int16_t)-28000)

/* Speed PI: error in electrical rpm → iq* in Q15 */
#define FOC_SPD_KP_Q15       ((int16_t)(0.02f * 32768.0f))
#define FOC_SPD_KI_Q15       ((int16_t)(0.001f * 32768.0f))
#define FOC_SPD_OUT_MAX_Q15  ((int16_t) 10000)
#define FOC_SPD_OUT_MIN_Q15  ((int16_t)-10000)

/* Alignment current (Q15 fraction of full-scale) */
#define FOC_ALIGN_ID_Q15     ((int16_t) 5000)
#define FOC_ALIGN_MS         200U

/* Open-loop V/F: V_q ∝ frequency, V_d = 0 */
#define FOC_VF_KV_Q15        ((int16_t) 100)              /* V_q per electrical Hz */

#endif /* APP_FOC_FOC_CONFIG_H */
