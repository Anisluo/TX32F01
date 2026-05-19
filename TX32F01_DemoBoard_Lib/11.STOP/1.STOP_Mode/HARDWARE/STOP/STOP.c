#include "TX32F01_periph.h"
#include "stop.h"
#include "systick.h"

#define SysCtrl_SLEEPONEXIT_Set     ((uint16_t)0x0002)
#define SysCtrl_SLEEPDEEP_Set  	    ((uint16_t)0x0004)


void PWR_EnterSTOPMode_WFI(void)
{
    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU->LPCR=0x10;//lash 进入省电模式
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

    SCB->SCR |= SysCtrl_SLEEPONEXIT_Set;//防止进入STOP模式前被其余中断打断
    SCB->SCR |= SysCtrl_SLEEPDEEP_Set;//进入STOP模式

    __WFI();//进入STOP，需要外部中断唤醒
}

void EXTI_P12_Init(void)
{
    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_GPIO1,ENABLE);//打开外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

    GPIO_Init(GPIO1,PIN02,GPIO_MODE_INPUT_PU);//P12配置成上拉输入
    EXTI_GPIO_Config(EXTI_GPIO1,EXTI_LINE_2);//配置中断

    EXTI_FALEDGE_TRIG_ENABLE(EXTI_LINE_2);//下降沿触发
//		EXTI_FALEDGE_TRIG_ENABLE(EXTI_LINE_2);//上降沿触发
    EXTI_IT_ENABLE(EXTI_LINE_2);//使能中断
    EXTI_ITF_CLEAR(EXTI_LINE_2);//清除标志位
    NVIC_EnableIRQ(EXTI2_IRQn);
    NVIC_SetPriority(EXTI2_IRQn, 0x0);//中断优先级设置

}

void EXTI2_Handler(void)
{
    delay_us(30000); //30ms消抖

    if(!KEY_P12)
    {
        SCB->SCR &= ~SysCtrl_SLEEPONEXIT_Set;//退出STOP模式，需要把该位清零
    }
		
    EXTI_ITF_CLEAR(EXTI_LINE_2);//清除标志位
}


