#ifndef __PWM_H
#define __PWM_H
#include "TX32F01_periph.h"

u8 TIM0_PWMBDTR_Init(u32 clk,u8 duty);
u8  TIM0_PWM_Set_Duty(u8 duty);


#endif
