#ifndef __PWM_H
#define __PWM_H
#include "TX32F01_periph.h"

u8 TIM0_PWM_Init(u32 clk,u8 duty);
u8 TIM0_PWM_Set_FreDuty(u32 clk,u8 duty);
u8 TIM1_Capture_Init(void);


#endif
