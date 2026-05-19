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

u16   ADCVAL;
float Getvol;

int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率
    printf("VDD作为ADC采样基准\r\n");
    ADC_SingleChannel_Init(ADC_VREF_VDD,ADC_CH_AN8_P13);//设置参考电压和采样通道
    delay_ms(1);//ADC初始化完成后需等待1毫秒
		
    while(1)
    {
        ADCVAL=Get_ADCVAL();//获取ADC采样值
		Getvol=ADCVAL*3.3/4096.0;//转换成电压值
			
		printf("P13管脚ADC采样电压为：%.3fV\r\n",Getvol);
		delay_ms(500);

    }
}




