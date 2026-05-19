#include "TX32F01_periph.h"
#include "systick.h"
#include "uart.h"
#include "ADC.h"

void SCU_Init(void)
{

    SCU_Unlock();			// 解锁SCU，可对其余SCU寄存器操作。

    SCU_SetSysClock(SysClock_24M);//系统时钟设置为24M

//    SCU_PeriphClockCmd(Periph_ALL,ENABLE);//打开外设时钟

    SCU_ResetPeriphClock(Periph_ALL);//复位所有外设模块

    SCU_SetBor(BOR_2P5V,ENABLE);//设置BOR2.5V

    SCU->RSR|=0x1;	//清除所有上电复位标志位

    SCU_Lock();			// 上锁SCU，不可对其余SCU寄存器操作。
}

//注意，如果要使用内部2.048V为ADC基准，芯片VDD电压必须大于2.048V。
u16   ADCVAL;
float Getvol;

int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率
	printf("连续采样模式\r\n");
    ADC_Continuous_Init(ADC_VREF_2V048,ADC_CH_AN8_P13);//设置参考电压和采样通道
    delay_us(20);//ADC初始化完成后需要延时20us，否则第一次采样不准
		
    while(1)
    {
        ADCVAL=Get_AverageADCVAL();//获取ADC采样值
		Getvol=ADCVAL*2.048/4096.0;//转换成电压值
			
		printf("P13管脚ADC采样电压为：%.3fV\r\n",Getvol);
		delay_ms(500);
    }
}




