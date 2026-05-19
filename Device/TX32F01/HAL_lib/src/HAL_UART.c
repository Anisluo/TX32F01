#include "HAL_UART.h"
#include "HAL_SCU.h"
#include "TX32F01.h"

/********************************************************************************************************
**函数信息 ：UART_DeInit(void)
**功能描述 ：将UART寄存器恢复默认值
**输入参数 ：无
**输出参数 ：无
********************************************************************************************************/
void UART_DeInit(void)
{
    SCU_Unlock();// 解锁SCU，可对其余SCU寄存器操作。
    SCU_ResetPeriphClock(Periph_UART);//复位UART
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。
}

/********************************************************************************************************
**函数信息 ：UART_Init(UART_InitTypeDef *UART_InitStruct)
**功能描述 ：初始化UART
**输入参数 ：UART_InitStruct：UART初始化参数结构体
**输出参数 ：无
********************************************************************************************************/
void UART_Init(UART_InitTypeDef *UART_InitStruct)
{
    uint32_t SYSCLK=GetSystemClock();//得到当前系统时钟频率

    UART->IER	= 0;//禁止UART中断
    UART->CR 	= 0;//清零
    UART->CER = 0;//清零

    //选择数据长度、停止位以及奇偶校验位
    UART->CR |=UART_InitStruct->UART_WordLength|UART_InitStruct->UART_StopBits|UART_InitStruct->UART_Parity;
    
    UART->BRR	= BAUDRATE_DIV(SYSCLK,UART_InitStruct->UART_BaudRate);// 设置波特率
   
    UART->CER |= UART_InitStruct->UART_Mode;//使能UART和收发
}

/********************************************************************************************************
**函数信息 ：UART_Cmd(FunctionalState NewState)
**功能描述 ：UART使能
**输入参数 ：NewState：ENABLE(使能UART)/DISABLE(关闭UART)
**输出参数 ：无
********************************************************************************************************/
void UART_Cmd(FunctionalState NewState)
{
    if (NewState != DISABLE)
    {
        UART->CER|=UART_UE;//使能UART
    }
    else
    {
        UART->CER&=~UART_UE;//关闭UART
    }
}

/********************************************************************************************************
**函数信息 ：UART_ITConfig(uint16_t UART_IT, FunctionalState NewState)
**功能描述 ：UART中断使能
**输入参数 ：UART_IT：要开启的中断
						NewState：ENABLE(开启中断)/DISABLE(关闭中断)
**输出参数 ：无
********************************************************************************************************/
void UART_ITConfig(uint16_t UART_IT, FunctionalState NewState)
{
    if (NewState != DISABLE)
    {        
        UART->IER |= UART_IT;//开启中断
    }
    else
    {        
        UART->IER &= ~UART_IT;//关闭中断
    }
}

/********************************************************************************************************
**函数信息 ：UART_GetFlagStatus(uint16_t UART_FLAG)
**功能描述 ：得到标志位状态
**输入参数 ：UART_FLAG：状态标志
**输出参数 ：无
********************************************************************************************************/
BOOL UART_GetFlagStatus(uint16_t UART_FLAG)
{
    if (UART->ISR & UART_FLAG)
    {
        return  TRUE;
    }
    else
    {
        return  FALSE;
    }
}

/********************************************************************************************************
**函数信息 ：UART_ClearFlag(uint16_t UART_FLAG)
**功能描述 ：清除标志位状态
**输入参数 ：UART_FLAG：状态标志
**输出参数 ：无
********************************************************************************************************/
void UART_ClearFlag(uint16_t UART_FLAG)
{   
    UART->ISR&=~UART_FLAG;//清除标志位
}

/********************************************************************************************************
**函数信息 ：UART_SendData(uint16_t Data)
**功能描述 ：发送数据
**输入参数 ：Data：要发送的数据
**输出参数 ：无
********************************************************************************************************/
void UART_SendData(uint16_t Data)
{   
    UART->TDR = (Data & (uint16_t)0x01FF);//发送数据
}

/********************************************************************************************************
**函数信息 ：UART_ReceiveData(void)
**功能描述 ：接收数据
**输入参数 ：无
**输出参数 ：UART接收到的数据
********************************************************************************************************/
uint16_t UART_ReceiveData(void)
{   
    return (uint16_t)(UART->RDR & (uint16_t)0x01FF);//接收数据
}




