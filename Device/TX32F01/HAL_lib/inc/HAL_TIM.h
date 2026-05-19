#ifndef _HAL_TIM_H
#define _HAL_TIM_H

#include "fpdefine.h"
#include "TX32F01.h"


typedef struct {
    uint8_t TIM_Mode;//模式选择
    uint16_t TIM_Prescaler;//预分频系数
    uint16_t TIM_Period;//自动重装载值
} TIM_InitTypeDef;


typedef struct {
    uint16_t TIM_BreakPolarity;//刹车上升沿/下降沿控制
    uint16_t TIM_OCxBreakState;//刹车后，0Cx通道输出电平
    uint16_t TIM_OCxNBreakState;//刹车后，0CxN通道输出电平
    uint8_t  TIM_BreakFilterValue;//刹车信号滤波
    uint16_t TIM_AutomaticOutput;//刹车自动恢复使能
} TIM_BDTRInitTypeDef;

/* TIM_CR	*/
#define TIM_Mode_CNT         0
#define TIM_Mode_CaptureIn   (1<<2)
#define TIM_Mode_CompareOut  (2<<2)
#define TIM_Mode_OnePulse    (3<<2)


/* TIM_CCCR	*/
#define TIM_OCxN_Low          (0)
#define TIM_OCxN_High         (1<<15)

#define TIM_OCx_Low           (0)
#define TIM_OCx_High          (1<<14)

#define TIM_ICPolarity_Rising    (0x0000)
#define TIM_ICPolarity_Falling   (1<<8)
#define TIM_ICPolarity_BothEdge  (2<<8)


/* TIM_IER	*/
#define TIM_IER_TIE           (1<<3)
#define TIM_IER_BIT           (1<<2)
#define TIM_IER_CCIE          (1<<1)
#define TIM_IER_CNTIE         (1)


/* TIM_IFR	*/
#define TIM_IFR_TIF           (1<<3)
#define TIM_IFR_BIF           (1<<2)
#define TIM_IFR_CCIF          (1<<1)
#define TIM_IFR_CNTIF         (1)

/* TIM_BCR	*/
#define TIM_AutomaticOutput_Disable        (0)
#define TIM_AutomaticOutput_Enable         (1<<5)

#define TIM_OCxNBreakState_Low             (0)
#define TIM_OCxNBreakState_High            (1<<3)

#define TIM_OCxBreakState_Low              (0)
#define TIM_OCxBreakState_High             (1<<2)

#define TIM_Breaklarity_Rising             (0)
#define TIM_Breaklarity_Falling            (1<<1)



void TIM_DeInit(TIM_Type* TIMx);
void TIM_Init(TIM_Type* TIMx,TIM_InitTypeDef *TIM_InitStruct);
void TIM_BDTRConfig(TIM_Type* TIMx,TIM_BDTRInitTypeDef *TIM_BDTRInitStruct);
void TIM_BDTRCmd(TIM_Type* TIMx,FunctionalState NewState);
void TIM_SetAutoreload(TIM_Type *TIMx, uint16_t Autoreload);
void TIM_SetCompare(TIM_Type* TIMx, uint16_t Compare);
void TIM_SetOCx_Polarity(TIM_Type* TIMx,uint16_t OCxPolarity);
void TIM_SetOCxN_Polarity(TIM_Type* TIMx,uint16_t OCxNPolarity);
void TIM_OCxOutEnable(TIM_Type* TIMx,FunctionalState NewState);
void TIM_OCxNOutEnable(TIM_Type* TIMx,FunctionalState NewState);
void TIM_SetDeadTime(TIM_Type* TIMx,uint16_t DeadTime);
void TIM_OCPolarityConfig(TIM_Type* TIMx, uint16_t TIM_OCPolarity);
void TIM_CaptureInFilter(TIM_Type* TIMx, uint8_t FilterValue);
void TIM_Cmd(TIM_Type* TIMx,FunctionalState NewState);
void TIM_ITConfig(TIM_Type* TIMx, uint16_t TIM_IT, FunctionalState NewState);
BOOL TIM_GetFlagStatus(TIM_Type* TIMx,uint16_t TIM_FLAG);
void TIM_ClearFlag(TIM_Type* TIMx,uint16_t TIM_FLAG);


#endif




