#include "HAL_SPI.h"
#include "HAL_SCU.h"
#include "TX32F01.h"

/********************************************************************************************************
**函数信息 ：SPI_DeInit(void)
**功能描述 ：将SPI寄存器恢复默认值
**输入参数 ：无
**输出参数 ：无
********************************************************************************************************/
void SPI_DeInit(void)
{
	SCU_Unlock();// 解锁SCU，可对其余SCU寄存器操作。
    SCU_ResetPeriphClock(Periph_SPI);//复位SPI
	SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。
}

/********************************************************************************************************
**函数信息 ：SPI_Init(SPI_InitTypeDef *SPI_InitStruct)
**功能描述 ：初始化UART
**输入参数 ：SPI_InitStruct：SPI初始化参数结构体
**输出参数 ：无
********************************************************************************************************/
void SPI_Init(SPI_InitTypeDef *SPI_InitStruct)
{
    SPI->CR=0;//清零
    SPI->DIVR=0;//清零

    //设置主从模式、数据长度、CPOL、CPHA、片选信号软硬件控制选择、优先发送数据选择
    SPI->CR|=SPI_InitStruct->SPI_Mode|SPI_InitStruct->SPI_DataWidth|SPI_InitStruct->SPI_CPOL
             |SPI_InitStruct->SPI_CPHA|SPI_InitStruct->SPI_NSS|SPI_InitStruct->SPI_FirstBit;
   
    SPI->DIVR=SPI_InitStruct->SPI_CLK_DIV;//分频系数选择
}

/********************************************************************************************************
**函数信息 ：SPI_Cmd(FunctionalState NewState)
**功能描述 ：SPI使能
**输入参数 ：NewState：ENABLE(使能SPI)/DISABLE(关闭SPI)
**输出参数 ：无
********************************************************************************************************/
void SPI_Cmd(FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        SPI->CR |=1;//使能SPI
    }
    else
    {
        SPI->CR &=~1;//关闭SPI
    }
}

/********************************************************************************************************
**函数信息 ：SPI_ITConfig(uint16_t SPI_IT, FunctionalState NewState)
**功能描述 ：SPI中断使能
**输入参数 ：SPI_IT：要开启的中断
						NewState：ENABLE(开启中断)/DISABLE(关闭中断)
**输出参数 ：无
********************************************************************************************************/
void SPI_ITConfig(uint16_t SPI_IT, FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        SPI->IER |= SPI_IT;//开启中断
    }
    else
    {
        SPI->IER &= ~SPI_IT;//关闭中断
    }
}

/********************************************************************************************************
**函数信息 ：SPI_GetFlagStatus(uint16_t SPI_FLAG)
**功能描述 ：得到标志位状态
**输入参数 ：SPI_FLAG：状态标志
**输出参数 ：无
********************************************************************************************************/
BOOL SPI_GetFlagStatus(uint32_t SPI_FLAG)
{
    if (SPI->SR & SPI_FLAG)
    {
        return  TRUE;
    }
    else
    {
        return  FALSE;
    }
}

/********************************************************************************************************
**函数信息 ：SPI_ClearFlag(uint16_t SPI_FLAG)
**功能描述 ：清除标志位状态
**输入参数 ：SPI_FLAG：状态标志
**输出参数 ：无
********************************************************************************************************/
void SPI_ClearFlag(uint32_t SPI_FLAG)
{
    SPI->SR &=~SPI_FLAG;//清除标志位
}

/********************************************************************************************************
**函数信息 ：SPI_SendData(uint16_t Data)
**功能描述 ：发送数据
**输入参数 ：Data：要发送的数据
**输出参数 ：无
********************************************************************************************************/
void SPI_SendData(uint16_t Data)
{
    SPI->DATAR = Data;//发送数据
}

/********************************************************************************************************
**函数信息 ：SPI_ReceiveData(void)
**功能描述 ：接收数据
**输入参数 ：无
**输出参数 ：UART接收到的数据
********************************************************************************************************/
uint16_t SPI_ReceiveData(void)
{
    return (uint16_t)(SPI->DATAR);//接收数据
}

/********************************************************************************************************
**函数信息 ：SPI_CSInternalSelected(uint16_t SPI_CSInternalSelected)
**功能描述 ：片选信号软件控制的时候，NSS输出有效/无效电平
**输入参数 ：SPI_CSInternalSelected：SPI_NSSInternalSoft_Low（输出低电平有效）
									 SPI_NSSInternalSoft_High（输出高电平有效）
**输出参数 ：无
********************************************************************************************************/
void SPI_CSInternalSelected(uint16_t SPI_CSInternalSelected)
{
    if(SPI_CSInternalSelected==SPI_NSSInternalSoft_High)
    {
        SPI->CR |=SPI_NSSInternalSoft_High;//输出高电平有效
    }
    else
    {
        SPI->CR &=~SPI_NSSInternalSoft_High;//输出低电平有效
    }
}

/********************************************************************************************************
**函数信息 ：SPI_NSSInternalSoftwareConfig(uint16_t SPI_NSSInternalSoft)
**功能描述 ：片选信号软件控制的时候，NSS输出有效/无效电平
**输入参数 ：SPI_NSSInternalSoft：SPI_NSSInternalSoft_Set（输出有效电平）
								  SPI_NSSInternalSoft_Reset（输出无效电平）
**输出参数 ：无
********************************************************************************************************/
void SPI_NSSInternalSoftwareConfig(uint16_t SPI_NSSInternalSoft)
{
    if(SPI_NSSInternalSoft==SPI_NSSInternalSoft_Set)
    {
        SPI->CR |=SPI_NSSInternalSoft_Set;//输出有效电平
    }
    else
    {
        SPI->CR &=~SPI_NSSInternalSoft_Set;//输出无效电平
    }
}



