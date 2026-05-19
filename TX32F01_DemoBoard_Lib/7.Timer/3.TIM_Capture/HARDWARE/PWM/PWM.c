#include "TX32F01_periph.h"
#include "pwm.h"

//sysclk=24M
/******************************************************
输入参数：clk：PWM输出频率
				 duty：占空比，范围0~100，对应0%~100%
输出参数：0：PASS
				 1:占空比超出范围
******************************************************/
u8 TIM0_PWM_Init(u32 clk,u8 duty)
{
    if(duty>100)
    return 1;

    TIM_InitTypeDef TIM_InitStructure;

    u32 SYSCLK=GetSystemClock();//得到当前系统时钟

    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_TIM0,ENABLE);//打开TIM0外设时钟
    SCU_PeriphClockCmd(Periph_GPIO0,ENABLE);//打开GPIO00外设时钟
    SCU_PeriphClockCmd(Periph_GPIO2,ENABLE);//打开GPIO2外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

    GPIO_Init(GPIO2,PIN02,GPIO_MODE_AF);//模拟DAC
    GPIO_Init(GPIO0,PIN03,GPIO_MODE_AF);//LED
    GPIO_PinRemapConfig(GPIO2,PIN02,GPIO_AF_T0CH);//OCx
    GPIO_PinRemapConfig(GPIO0,PIN03,GPIO_AF_T0CHN);//0CxN

    TIM_DeInit(TIM0);//TIM0寄存器恢复默认值

    //TIM0初始化
    TIM_InitStructure.TIM_Mode = TIM_Mode_CompareOut;//比较输出模式（PWM输出）
    TIM_InitStructure.TIM_Prescaler = 3-1;//TIM时钟为系统时钟3分频
    TIM_InitStructure.TIM_Period = SYSCLK/(2+1)/clk-1;//设置预分配系数，即PWM输出频率
    TIM_Init(TIM0,&TIM_InitStructure);//初始化TIM

    TIM_SetCompare(TIM0,(TIM0->ARR+1)*(100-duty)/100-1);//设置比较值，即调节占空比

    TIM_SetOCx_Polarity(TIM0,TIM_OCx_High);//设置OCx初始电平高
    TIM_SetOCxN_Polarity(TIM0,TIM_OCxN_Low);//设置OCxN初始电平低

    TIM_OCxOutEnable(TIM0,ENABLE);//OCx输出使能
    TIM_OCxNOutEnable(TIM0,ENABLE);//OCxN输出使能

    TIM_Cmd(TIM0,ENABLE);//计数器使能

    return 0;
}

/******************************************************
输入参数：duty：占空比，范围0~100，对应0%~100%
输出参数：无
******************************************************/
u8  TIM0_PWM_Set_FreDuty(u32 clk,u8 duty)
{
    if(duty>100)
    return FALSE;

    u32 SYSCLK=GetSystemClock();//得到当前系统时钟

    TIM_SetAutoreload(TIM0,SYSCLK/(TIM0->DIV+1)/clk-1);//设置PWM输出频率
    TIM_SetCompare(TIM0,(TIM0->ARR+1)*(100-duty)/100-1);//设置占空比

    return TRUE;
}

u8 TIM1_Capture_Init(void)
{
    TIM_InitTypeDef TIM_InitStructure;

    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_GPIO3,ENABLE);//打开GPIO3外设时钟
    SCU_PeriphClockCmd(Periph_TIM1,ENABLE);//打开GPIO3外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

    GPIO_Init(GPIO3,PIN05,GPIO_MODE_AF);//P35作为输入捕获
    GPIO_PinRemapConfig(GPIO3,PIN05,GPIO_AF_T1CH);//复用为T1CH

    TIM_DeInit(TIM1);//TIM1寄存器恢复默认值

    //TIM0初始化
    TIM_InitStructure.TIM_Mode = TIM_Mode_CaptureIn;//比较输出模式（PWM输出）
    TIM_InitStructure.TIM_Prescaler = 2-1;//TIM时钟为系统时钟2分频
    TIM_InitStructure.TIM_Period = 0xFFFF-1;//设置预分配系数，即PWM输出频率
    TIM_Init(TIM1,&TIM_InitStructure);//初始化TIM

	TIM_OCPolarityConfig(TIM1,TIM_ICPolarity_Rising);//配置为上升沿捕获
	TIM_CaptureInFilter(TIM1,6);//输入滤波
	
    TIM_ITConfig(TIM1,TIM_IER_CCIE,ENABLE);//使能捕获中断

    NVIC_EnableIRQ(TIM1_IRQn);// 打开中断
    NVIC_SetPriority(TIM1_IRQn,0);//中断优先级设置
		
    TIM_Cmd(TIM1,ENABLE);//计数器使能
    return 0;
}

u16 OLD;//上一次得到的计数值
u8 NUM_FRE=0;
u16 FrequencyAve;//PWM捕获频率
void TIMER1_Handler(void)
{
    if (TIM_GetFlagStatus(TIM1,TIM_IFR_CCIF))
    {
        if(NUM_FRE==0)
        {
            OLD=TIM1->CCR;//第一次得到捕获值
            NUM_FRE=1;
        }
        else
        {
            FrequencyAve=(24000000/2)/(TIM1->CCR-OLD); //两次捕获比较之差即为PWM频率
            NUM_FRE=0;
            TIM1->CNT=0;//清除计数器
        }
    }
		TIM_ClearFlag(TIM1,TIM_IFR_CCIF);//清除标志位
}


