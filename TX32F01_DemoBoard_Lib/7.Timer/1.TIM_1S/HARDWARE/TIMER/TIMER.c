#include "TX32F01_periph.h"
#include "timer.h"


void TIM0_Int_1s(void)
{
    TIM_InitTypeDef TIM_InitStructure;

    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_TIM0,ENABLE);//打开TIM0外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

    TIM_DeInit(TIM0);//UART寄存器恢复默认值

    //TIM0初始化
    TIM_InitStructure.TIM_Mode = TIM_Mode_CNT;//计数模式
    TIM_InitStructure.TIM_Prescaler = GetSystemClock()/1000-1;//经过分频后，1ms计数一次
    TIM_InitStructure.TIM_Period =1000-1;//1ms计数一次，计数1000次，即1秒
    TIM_Init(TIM0,&TIM_InitStructure);//初始化TIM

	TIM_ITConfig(TIM0,TIM_IER_CNTIE,ENABLE);//开启计数器溢出中断
	
    NVIC_EnableIRQ(TIM0_IRQn);//打开TIM0中断
    NVIC_SetPriority(TIM0_IRQn, 0x0);//中断优先级设置

    TIM_Cmd(TIM0,ENABLE);//计数器使能
}

BOOL EnterIntFlag=FALSE;//中断标志
void TIMER0_Handler(void)
{
    if (TIM_GetFlagStatus(TIM0,TIM_IFR_CNTIF))// 检查CNTIF标志位
    {
        EnterIntFlag=TRUE;//1秒计时时间到
		TIM_ClearFlag(TIM0,TIM_IFR_CNTIF);//清除标志位
    }
}
