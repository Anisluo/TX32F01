#ifndef __PWM_H
#define __PWM_H


#include "TX32F01_periph.h"



uint8_t TIM0_PWM_Init(uint32_t clk,uint8_t duty);
uint8_t TIM0_PWM_Set_Duty(uint8_t duty);


void     PWM_SetDuty(uint8_t duty); // 0..100 %
void     PWM_Set_Ch_Duty(uint8_t ch,uint8_t duty);//配置通道数，和占空比数据
uint8_t  PWM_GetDuty(void);
void     PWM_SetFreq(uint32_t hz);
uint32_t PWM_GetFreq(void);

void  PWM_Set_Ch_Duty_cnt(uint8_t chs,uint8_t duty);

void 	   PWM_Disable();//关闭PWM输出
void     PWM_Enable();//使能PWM输出


#endif
