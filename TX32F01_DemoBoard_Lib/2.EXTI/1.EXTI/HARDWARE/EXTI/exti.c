#include "TX32F01_periph.h"
#include "exti.h"
#include "led.h"
#include "systick.h"


void EXTI_P12_Init(void)
{
    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_GPIO1,ENABLE);//打开外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

    GPIO_Init(GPIO1,PIN02,GPIO_MODE_INPUT_PU);//P12配置成上拉输入
    EXTI_GPIO_Config(EXTI_GPIO1,EXTI_LINE_2);//按键中断配置

    EXTI_FALEDGE_TRIG_ENABLE(EXTI_LINE_2);//下降沿触发
//		EXTI_FALEDGE_TRIG_ENABLE(EXTI_LINE_2);//上降沿触发
    EXTI_IT_ENABLE(EXTI_LINE_2);//中断使能
    EXTI_ITF_CLEAR(EXTI_LINE_2);//清除标志位
    
    NVIC_EnableIRQ(EXTI2_IRQn);
		NVIC_SetPriority(EXTI2_IRQn, 0x0);//中断优先级设置
}

void EXTI2_Handler(void)
{
    delay_us(5000); //消抖

    if(!KEY_P12)
    {
        LED_TOGGLE();//LED翻转
    }
    EXTI_ITF_CLEAR(EXTI_LINE_2);
}


