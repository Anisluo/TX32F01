#include "TX32F01_periph.h"
#include "pwm.h"

//sysclk=24M
/******************************************************
输入参数：clk：PWM输出频率
				 duty：占空比，范围0~100，对应0%~100%
输出参数：0：PASS
				 1:占空比超出范围
******************************************************/
u8 TIM0_PWMBDTR_Init(u32 clk,u8 duty)
{
    if(duty>100)
    return 1;

    TIM_InitTypeDef TIM_InitStructure;
	TIM_BDTRInitTypeDef TIM_BDTRInitStructure;
		
    u32 SYSCLK=GetSystemClock();//得到当前系统时钟

    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_TIM0,ENABLE);//打开TIM0外设时钟
    SCU_PeriphClockCmd(Periph_GPIO0,ENABLE);//打开GPIO00外设时钟
    SCU_PeriphClockCmd(Periph_GPIO1,ENABLE);//打开GPIO01外设时钟
    SCU_PeriphClockCmd(Periph_GPIO2,ENABLE);//打开GPIO2外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

    GPIO_Init(GPIO2,PIN02,GPIO_MODE_AF);//模拟DAC
    GPIO_Init(GPIO0,PIN03,GPIO_MODE_AF);//LED
    GPIO_Init(GPIO1,PIN02,GPIO_MODE_AF);//KEY 刹车
    GPIO_PinRemapConfig(GPIO2,PIN02,GPIO_AF_T0CH);//OCx
    GPIO_PinRemapConfig(GPIO0,PIN03,GPIO_AF_T0CHN);//0CxN
    GPIO_PinRemapConfig(GPIO1,PIN02,GPIO_AF_TIMER_BRK_IN);//刹车
		
		
    TIM_DeInit(TIM0);//TIM0寄存器恢复默认值

    //TIM0 PWM初始化
    TIM_InitStructure.TIM_Mode = TIM_Mode_CompareOut;//比较输出模式（PWM输出）
    TIM_InitStructure.TIM_Prescaler = 3-1;//TIM时钟为系统时钟3分频
    TIM_InitStructure.TIM_Period = SYSCLK/(2+1)/clk-1;//设置预分配系数，即PWM输出频率
    TIM_Init(TIM0,&TIM_InitStructure);//初始化TIM

    TIM_SetCompare(TIM0,(TIM0->ARR+1)*(100-duty)/100-1);//设置比较值，即调节占空比

    TIM_SetOCx_Polarity(TIM0,TIM_OCx_High);//设置OCx初始电平高
    TIM_SetOCxN_Polarity(TIM0,TIM_OCxN_Low);//设置OCxN初始电平低
		
	TIM_SetDeadTime(TIM0,20);//设置死区时间

    TIM_OCxOutEnable(TIM0,ENABLE);//OCx输出使能
    TIM_OCxNOutEnable(TIM0,ENABLE);//OCxN输出使能
		
    //刹车配置
    GPIO_PullUpConfig(GPIO1,PIN02);//按键配置为上拉，因为刹车下降沿触发

    TIM_BDTRInitStructure.TIM_BreakPolarity=TIM_Breaklarity_Falling;//下降沿触发刹车
    TIM_BDTRInitStructure.TIM_OCxBreakState=TIM_OCxBreakState_Low;//刹车后，OCx输出低电平
    TIM_BDTRInitStructure.TIM_OCxNBreakState=TIM_OCxNBreakState_Low;//刹车后，OCxN输出低电平
    TIM_BDTRInitStructure.TIM_BreakFilterValue=6;//刹车输入滤波
    TIM_BDTRInitStructure.TIM_AutomaticOutput=TIM_AutomaticOutput_Enable;//自动恢复使能
    TIM_BDTRConfig(TIM0,&TIM_BDTRInitStructure);//初始化TIM刹车
    
    TIM_BDTRCmd(TIM0,ENABLE);//使能刹车
    TIM_Cmd(TIM0,ENABLE);//计数器使能

    return 0;
}

/******************************************************
输入参数：duty：占空比，范围0~100，对应0%~100%
输出参数：无
******************************************************/
u8  TIM0_PWM_Set_Duty(u8 duty)
{
    if(duty>100)
    return FALSE;

    u16 CCRVAL=(TIM0->ARR+1)*(100-duty)/100-1;

    TIM_SetCompare(TIM0,CCRVAL);//设置比较输出模式比较值
		
    return TRUE;
}



