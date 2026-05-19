#include "HAL_GPIO.h"
#include "HAL_SCU.h"
#include "TX32F01.h"

/********************************************************************************************************
**函数信息 ：GPIO_DeInit(GPIO_Type* GPIOx)
**功能描述 ：将GPIO寄存器恢复默认值
**输入参数 ：GPIOx：选择GPIO
**输出参数 ：无
********************************************************************************************************/
void GPIO_DeInit(GPIO_Type* GPIOx)
{
	SCU_Unlock();// 解锁SCU，可对其余SCU寄存器操作。
    if(GPIOx==GPIO0)
    {
        SCU_ResetPeriphClock(Periph_GPIO0);//复位GPIO
    }
    else	if(GPIOx==GPIO1)
    {
        SCU_ResetPeriphClock(Periph_GPIO1);//复位GPI1
    }
    else	if(GPIOx==GPIO2)
    {
        SCU_ResetPeriphClock(Periph_GPIO2);//复位GPI2
    }
    else	if(GPIOx==GPIO3)
    {
        SCU_ResetPeriphClock(Periph_GPIO3);//复位GPI3
    }
	SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。
}

/********************************************************************************************************
**函数信息 ：GPIO_Init(GPIO_Type* GPIOx, u8 GPIO_Pin, u32 mode)
**功能描述 ：单个GPIO初始化，可以配置为浮空输入、上拉输入、下拉输入、推挽输出、开漏输出、复用功能、模拟功能
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
						 mode：GPIO模式选择,可以配置为GPIO_MODE_INPUT_FLOAT、GPIO_MODE_INPUT_PU等等
**输出参数 ：无
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
void GPIO_Init(GPIO_Type* GPIOx, u8 GPIO_Pin, u32 mode)
{
    GPIOx->MDR&=~((0x3)<<(GPIO_Pin*2));//清除当前寄存器状态
    GPIOx->PUR&=~((0x1)<<GPIO_Pin);//清除当前寄存器状态
    GPIOx->PDR&=~((0x1)<<GPIO_Pin);//清除当前寄存器状态
    GPIOx->OTR&=~((0x1)<<GPIO_Pin);//清除当前寄存器状态
    GPIOx->IER&=~((0x1)<<GPIO_Pin);//清除当前寄存器状态
    GPIOx->DSR&=~((0x1)<<GPIO_Pin);//清除当前寄存器状态
    GPIOx->SRR&=~((0x1)<<GPIO_Pin);//清除当前寄存器状态

    switch(mode)
    {
		case GPIO_MODE_INPUT_FLOAT://浮空输入
			GPIOx->IER|=0x1<<GPIO_Pin;
			break;

		case GPIO_MODE_INPUT_PU://上拉输入
			GPIOx->PUR|=0x1<<GPIO_Pin;
			GPIOx->IER|=0x1<<GPIO_Pin;
			break;

		case GPIO_MODE_INPUT_PD://下拉输入
			GPIOx->PDR|=0x1<<GPIO_Pin;
			GPIOx->IER|=0x1<<GPIO_Pin;
			break;

		case GPIO_MODE_OUTPUT_PP://推挽输出
			GPIOx->MDR|=0x1<<(GPIO_Pin*2);
			break;

		case GPIO_MODE_OUTPUT_OD://开漏输出
			GPIOx->MDR|=0x1<<(GPIO_Pin*2);
			GPIOx->OTR|=0x1<<GPIO_Pin;
			break;

		case GPIO_MODE_AF://复用功能
			GPIOx->MDR|=0x2<<(GPIO_Pin*2);
			break;

		case GPIO_MODE_ANALOG://模拟功能
			GPIOx->MDR|=0x3<<(GPIO_Pin*2);
			break;

		default :
			break;

    }
}


/********************************************************************************************************
**函数信息 ：GPIO_PinRemapConfig(GPIO_Type* GPIOx, u8 GPIO_Pin, u8 GPIO_AFReg)
**功能描述 ：GPIO复用功能选择
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
						 GPIO_AFReg：复用选择，可以配置为GPIO_AF_UART_RX、GPIO_AF_UART_TX等等
**输出参数 ：无
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
void GPIO_PinRemapConfig(GPIO_Type* GPIOx, u8 GPIO_Pin, u8 GPIO_AFReg)
{
    GPIOx->MDR&=~((0x3)<<(GPIO_Pin*2));//清除当前寄存器状态
    GPIOx->MDR|=0x2<<(GPIO_Pin*2);//设置复用模式

    GPIOx->AFR&=~((0xf)<<(GPIO_Pin*4));//清除当前寄存器状态
    GPIOx->AFR|=GPIO_AFReg<<(GPIO_Pin*4);//配置复用功能
}


/********************************************************************************************************
**函数信息 ：GPIO_PullUpConfig(GPIO_Type* GPIOx, u8 GPIO_Pin)
**功能描述 ：GPIO配置上拉
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
**输出参数 ：无
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
void GPIO_PullUpConfig(GPIO_Type* GPIOx, u8 GPIO_Pin)
{
    GPIOx->PDR&=~((0x1)<<GPIO_Pin);//清除当前寄存器状态
    GPIOx->PUR|=0x1<<GPIO_Pin;//配置上拉
}


/********************************************************************************************************
**函数信息 ：GPIO_PullDownConfig(GPIO_Type* GPIOx, u8 GPIO_Pin)
**功能描述 ：GPIO配置下拉
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
**输出参数 ：无
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
void GPIO_PullDownConfig(GPIO_Type* GPIOx, u8 GPIO_Pin)
{
	GPIOx->PUR&=~((0x1)<<GPIO_Pin);//清除当前寄存器状态
    GPIOx->PDR|=0x1<<GPIO_Pin;//配置下拉
}


/********************************************************************************************************
**函数信息 ：GPIO_SetSpeed(GPIO_Type* GPIOx, u8 GPIO_Pin,u8 Speed)
**功能描述 ：GPIO速率选择
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
						 Speed：可以配置为GPIO_Speed_High或者GPIO_Speed_Low
**输出参数 ：无
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
void GPIO_SetSpeed(GPIO_Type* GPIOx, u8 GPIO_Pin,u8 Speed)
{
    if(Speed==GPIO_Speed_High)
    {
        GPIOx->DSR&=~((0x1)<<GPIO_Pin);//配置低驱动
        GPIOx->SRR&=~((0x1)<<GPIO_Pin);//配置低速率
    }
    else if(Speed==GPIO_Speed_Low)
    {
        GPIOx->DSR|=0x1<<GPIO_Pin;//配置高驱动
        GPIOx->SRR|=0x1<<GPIO_Pin;//配置高速率
    }
}


/********************************************************************************************************
**函数信息 ：GPIO_SetBits(GPIO_Type* GPIOx, u8 GPIO_Pin)
**功能描述 ：GPIO输出高电平
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
**输出参数 ：无
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
void GPIO_SetBits(GPIO_Type* GPIOx, u8 GPIO_Pin)
{
    GPIOx->BSR |=0x1<<GPIO_Pin;//GPIO输出高电平
}


/********************************************************************************************************
**函数信息 ：GPIO_ResetBits(GPIO_Type* GPIOx, u8 GPIO_Pin)
**功能描述 ：GPIO输出低电平
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
**输出参数 ：无
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
void GPIO_ResetBits(GPIO_Type* GPIOx, u8 GPIO_Pin)
{
    GPIOx->BRR |=0x1<<GPIO_Pin;//GPIO输出低电平
}


/********************************************************************************************************
**函数信息 ：GPIO_Toggle(GPIO_Type* GPIOx, u8 GPIO_Pin)
**功能描述 ：GPIO电平输出翻转
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
**输出参数 ：无
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
void GPIO_Toggle(GPIO_Type* GPIOx, u8 GPIO_Pin)
{
    GPIOx->BFR |=0x1<<GPIO_Pin;//GPIO电平输出翻转
}

/********************************************************************************************************
**函数信息 ：GPIO_Write(GPIO_Type* GPIOx, u8 PortVal)
**功能描述 ：同时对PIN00~PIN07配置输出电平
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 PortVal：GPIOx所有GPIO输出电平状态
**输出参数 ：无
********************************************************************************************************/
void GPIO_Write(GPIO_Type* GPIOx, u8 PortVal)
{
    GPIOx->ODR=PortVal;//配置输出电平
}


/********************************************************************************************************
**函数信息 ：GPIO_ReadOutputDataBit(GPIO_Type* GPIOx, u8 GPIO_Pin)
**功能描述 ：当GPIO配置为输出模式时，读取GPIO电平状态
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
**输出参数 ：0：该端口为低电平
						 1：该端口为高电平
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
uint8_t GPIO_ReadOutputDataBit(GPIO_Type* GPIOx, u8 GPIO_Pin)
{
    GPIOx->IER|=0x1<<GPIO_Pin;//读取GPIO电平状态
    __NOP();//延时
    __NOP();
    __NOP();

    return ((GPIOx->IDR>>GPIO_Pin)&0x1);//返回GPIO电平状态
}


/********************************************************************************************************
**函数信息 ：GPIO_ReadInputDataBit(GPIO_Type* GPIOx, u8 GPIO_Pin)
**功能描述 ：GPIO读取输入电平状态
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
**输出参数 ：0：该端口为低电平
						 1：该端口为高电平
注意：每次调用该函数只能配置一个GPIO，不能同时配置多个GPIO
********************************************************************************************************/
uint8_t GPIO_ReadInputDataBit(GPIO_Type* GPIOx, u8 GPIO_Pin)
{
    return ((GPIOx->IDR>>GPIO_Pin)&0x1);//GPIO读取输入电平状态
}


/********************************************************************************************************
**函数信息 ：GPIO_ReadInputData(GPIO_Type* GPIOx)
**功能描述 ：GPIOx读取所有IO电平状态
**输入参数 ：GPIOx：GPIO选择，可以配置为GPIO0~GPIO3
					 	 GPIO_Pin：GPIO端口选择，可以配置为PIN00~PIN07
**输出参数 ：GPIOx->IDR
********************************************************************************************************/
uint8_t GPIO_ReadInputData(GPIO_Type* GPIOx)
{
    return GPIOx->IDR;//返回所有IO电平状态
}












