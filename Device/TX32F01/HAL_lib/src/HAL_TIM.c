#include "HAL_TIM.h"
#include "HAL_SCU.h"
#include "TX32F01.h"


/********************************************************************************************************
**函数信息 ：TIM_DeInit(void)
**功能描述 ：将TIM寄存器恢复默认值
**输入参数 ：TIMx：TIM0~TIM2
**输出参数 ：无
********************************************************************************************************/
void TIM_DeInit(TIM_Type* TIMx)
{
	SCU_Unlock();// 解锁SCU，可对其余SCU寄存器操作。
    if(TIMx==TIM0)
    {
        SCU_ResetPeriphClock(Periph_TIM0);//复位TIM0
    }
    else if(TIMx==TIM1)
    {
        SCU_ResetPeriphClock(Periph_TIM1);//复位TIM1
    }
    else if(TIMx==TIM2)
    {
        SCU_ResetPeriphClock(Periph_TIM2);//复位TIM2
    }
	SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。
}

/********************************************************************************************************
**函数信息 ：TIM_TimeBaseInit(TIM_Type* TIMx,TIM_TimeBaseInitTypeDef *TIM_TimeBaseInitStruct)
**功能描述 ：初始化TIM
**输入参数 ：TIMx：TIM0~TIM2
						TIM_InitStruct：TIM初始化参数结构体
**输出参数 ：无
********************************************************************************************************/
void TIM_Init(TIM_Type* TIMx,TIM_InitTypeDef *TIM_InitStruct)
{
    TIMx->CR=0;//清零
    TIMx->DIV=0;//清零
    
    TIMx->CR|=TIM_InitStruct->TIM_Mode;//模式选择
   
    TIMx->DIV|=TIM_InitStruct->TIM_Prescaler;//预分频系数

    //ARR寄存器不能写入0x0和0xFFFF
    if(TIM_InitStruct->TIM_Period==0) TIM_InitStruct->TIM_Period=1;
    if(TIM_InitStruct->TIM_Period==0xFFFF) TIM_InitStruct->TIM_Period=0xFFFE;
    
    TIMx->ARR=TIM_InitStruct->TIM_Period;//配置自动重装载值
}

/********************************************************************************************************
**函数信息 ：TIM_BDTRConfig(TIM_Type* TIMx,TIM_BDTRInitTypeDef *TIM_BDTRInitStruct)
**功能描述 ：TIMx刹车初始化
**输入参数 ：TIMx：TIM0~TIM2
						TIM_BDTRInitStruct：TIM刹车初始化参数结构体
**输出参数 ：无
********************************************************************************************************/
void TIM_BDTRConfig(TIM_Type* TIMx,TIM_BDTRInitTypeDef *TIM_BDTRInitStruct)
{
    TIMx->BCR=0;//清零

    //刹车触发边沿选择、刹车后，0Cx、0CxN通道输出电平、刹车信号滤波、刹车自动恢复使能
    TIMx->BCR |=TIM_BDTRInitStruct->TIM_BreakPolarity|TIM_BDTRInitStruct->TIM_OCxBreakState|
                TIM_BDTRInitStruct->TIM_OCxNBreakState|(TIM_BDTRInitStruct->TIM_BreakFilterValue<<8)|
                TIM_BDTRInitStruct->TIM_AutomaticOutput;
   
    TIMx->BCR|=1<<4;//PWM输出使能
}

/********************************************************************************************************
**函数信息 ：TIM_BDTRCmd(TIM_Type* TIMx,FunctionalState NewState)
**功能描述 ：TIM刹车使能
**输入参数 ：NewState：ENABLE(使能TIM刹车)/DISABLE(关闭TIM刹车)
**输出参数 ：无
********************************************************************************************************/
void TIM_BDTRCmd(TIM_Type* TIMx,FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        TIMx->BCR|=1;//使能TIM刹车
    }
    else
    {
        TIMx->BCR&=~1;//关闭TIM刹车
    }
}

/********************************************************************************************************
**函数信息 ：TIM_SetAutoreload(TIM_Type *TIMx, uint16_t Autoreload)
**功能描述 ：设置自动重装值
**输入参数 ：TIMx：TIM0~TIM2
						Autoreload：自动重装值
**输出参数 ：无
********************************************************************************************************/
void TIM_SetAutoreload(TIM_Type *TIMx, uint16_t Autoreload)
{
    if(Autoreload==0) 
	{
		Autoreload=1;
	}
    else if(Autoreload==0xFFFF) 
	{
		Autoreload=0xFFFE;
	}

    TIMx->ARR = Autoreload;//设置自动重装值
}

/********************************************************************************************************
**函数信息 ：TIM_SetCompare(TIM_Type* TIMx, uint16_t Compare)
**功能描述 ：比较输出模式设置比较值
**输入参数 ：TIMx：TIM0~TIM2
						Compare：比较输出值
**输出参数 ：无
********************************************************************************************************/
void TIM_SetCompare(TIM_Type* TIMx, uint16_t Compare)
{
    if(Compare==0) 
	{
		Compare=1;
	}
    else if(Compare==0xFFFF) 
	{
		Compare=0xFFFE;
	}

    TIMx->CCR = Compare;//设置比较值
}

/********************************************************************************************************
**函数信息 ：TIM_SetOCx_Polarity(TIM_Type* TIMx,uint16_t OCxPolarity)
**功能描述 ：比较输出模式下，OCx设置PWM初始电平极性
**输入参数 ：TIMx：TIM0~TIM2
						OCxPolarity：OCx初始电平极性
**输出参数 ：无
********************************************************************************************************/
void TIM_SetOCx_Polarity(TIM_Type* TIMx,uint16_t OCxPolarity)
{
    if(OCxPolarity==TIM_OCx_Low)
    {
        TIMx->CCCR&=~TIM_OCx_High;//OCx初始电平为低
    }
    else if(OCxPolarity==TIM_OCx_High)
    {
        TIMx->CCCR|=OCxPolarity;//OCx初始电平为高
    }
}

/********************************************************************************************************
**函数信息 ：TIM_SetOCxN_Polarity(TIM_Type* TIMx,uint16_t OCxNPolarity)
**功能描述 ：比较输出模式下，OCxN设置PWM初始电平极性
**输入参数 ：TIMx：TIM0~TIM2
						OCxNPolarity：OCxN初始电平极性
**输出参数 ：无
********************************************************************************************************/
void TIM_SetOCxN_Polarity(TIM_Type* TIMx,uint16_t OCxNPolarity)
{
    if(OCxNPolarity==TIM_OCxN_Low)
    {
        TIMx->CCCR&=~TIM_OCxN_High;//OCxN初始电平为低
    }
    else if(OCxNPolarity==TIM_OCxN_High)
    {
        TIMx->CCCR|=OCxNPolarity;//OCxN初始电平为高
    }
}

/********************************************************************************************************
**函数信息 ：TIM_OCxOutEnable(TIM_Type* TIMx,FunctionalState NewState)
**功能描述 ：比较输出模式下，OCx允许输出控制
**输入参数 ：TIMx：TIM0~TIM2
						NewState：ENABLE(使能OCx输出)/DISABLE(关闭OCx输出)
**输出参数 ：无
********************************************************************************************************/
void TIM_OCxOutEnable(TIM_Type* TIMx,FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        TIMx->CCCR|=(1<<12);//使能OCx输出
    }
    else
    {
        TIMx->CCCR&=~(1<<12);//关闭OCx输出
    }
}

/********************************************************************************************************
**函数信息 ：TIM_OCxNOutEnable(TIM_Type* TIMx,FunctionalState NewState)
**功能描述 ：比较输出模式下，OCxN允许输出控制
**输入参数 ：TIMx：TIM0~TIM2
						NewState：ENABLE(使能OCxN输出)/DISABLE(关闭OCxN输出)
**输出参数 ：无
********************************************************************************************************/
void TIM_OCxNOutEnable(TIM_Type* TIMx,FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        TIMx->CCCR|=(1<<13);//使能OCxN输出
    }
    else
    {
        TIMx->CCCR&=~(1<<13);//关闭OCxN输出
    }
}

/********************************************************************************************************
**函数信息 ：TIM_SetDeadTime(TIM_Type* TIMx,uint16_t DeadTime)
**功能描述 ：设置死区时间
**输入参数 ：TIMx：TIM0~TIM2
						DeadTime：死区时间
**输出参数 ：无
********************************************************************************************************/
void TIM_SetDeadTime(TIM_Type* TIMx,uint16_t DeadTime)
{   
    TIMx->DTG = DeadTime;//设置死区时间
}

/********************************************************************************************************
**函数信息 ：TIM_OCPolarityConfig(TIM_Type* TIMx, uint16_t TIM_OCPolarity)
**功能描述 ：输入捕获模式下，输入捕获边沿选择
**输入参数 ：TIMx：TIM0~TIM2
						TIM_OCPolarity：边沿选择
**输出参数 ：无
********************************************************************************************************/
void TIM_OCPolarityConfig(TIM_Type* TIMx, uint16_t TIM_OCPolarity)
{
    TIMx->CCCR &=~ (3<<8);//清除状态
    TIMx->CCCR |= TIM_OCPolarity;//输入捕获边沿选择
}

/********************************************************************************************************
**函数信息 ：TIM_CaptureInFilter(TIM_Type* TIMx, uint8_t FilterValue)
**功能描述 ：输入捕获滤波
**输入参数 ：TIMx：TIM0~TIM2
						FilterValue：滤波值
**输出参数 ：无
********************************************************************************************************/
void TIM_CaptureInFilter(TIM_Type* TIMx, uint8_t FilterValue)
{
    TIMx->CCCR &=~0xFF;//清除状态
    TIMx->CCCR |= FilterValue;//设置滤波值
}

/********************************************************************************************************
**函数信息 ：TIM_Cmd(TIM_Type* TIMx,FunctionalState NewState)
**功能描述 ：TIM计数器使能
**输入参数 ：NewState：ENABLE(使能TIM计数器)/DISABLE(关闭TIM计数器)
**输出参数 ：无
********************************************************************************************************/
void TIM_Cmd(TIM_Type* TIMx,FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        TIMx->CR|=1;//使能TIM计数器
    }
    else
    {
        TIMx->CR&=~1;//关闭TIM计数器
    }
}

/********************************************************************************************************
**函数信息 ：TIM_ITConfig(TIM_Type* TIMx, uint16_t TIM_IT, FunctionalState NewState)
**功能描述 ：TIM中断使能
**输入参数 ：TIM_IT：要开启的中断
						NewState：ENABLE(开启中断)/DISABLE(关闭中断)
**输出参数 ：无
********************************************************************************************************/
void TIM_ITConfig(TIM_Type* TIMx, uint16_t TIM_IT, FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        TIMx->IER|= TIM_IT;//开启中断
    }
    else
    {
        TIMx->IER&= ~TIM_IT; //关闭中断
    }
}

/********************************************************************************************************
**函数信息 ：TIM_GetFlagStatus(TIM_Type* TIMx,uint16_t TIM_FLAG)
**功能描述 ：得到标志位状态
**输入参数 ：TIM_FLAG：状态标志
**输出参数 ：无
********************************************************************************************************/
BOOL TIM_GetFlagStatus(TIM_Type* TIMx,uint16_t TIM_FLAG)
{
    if (TIMx->IFR & TIM_FLAG)
    {
        return  TRUE;
    }
    else
    {
        return  FALSE;
    }
}

/********************************************************************************************************
**函数信息 ：TIM_ClearFlag(TIM_Type* TIMx,uint16_t TIM_FLAG)
**功能描述 ：清除标志位状态
**输入参数 ：TIM_FLAG：状态标志
**输出参数 ：无
********************************************************************************************************/
void TIM_ClearFlag(TIM_Type* TIMx,uint16_t TIM_FLAG)
{   
    TIMx->IFR &= ~TIM_FLAG;//清除标志位
}



