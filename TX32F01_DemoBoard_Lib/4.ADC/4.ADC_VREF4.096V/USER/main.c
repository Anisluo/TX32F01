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

//注意，如果要使用内部4.096V为ADC基准，芯片VDD电压必须大于4.096V。若VDD为3.3V，会计算不准。
u16   ADCVAL;
float Getvol;
float VDDVAL;//实际VDD电压

int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率
    ADC_SingleChannel_Init(ADC_VREF_2V048,ADC_CH_1P3VDD);//先使用内部2.048V基准测量VDD电压
    delay_ms(1);

    ADCVAL=Get_ADCVAL();//得到1/3VDD电压值ADC转换值
    VDDVAL=ADCVAL*2.048/4096.0*3;//得到精确的VDD电压值
	printf("VDD电压为：%.3fV\r\n",VDDVAL);
	
    if(VDDVAL>4.6)
    {
        ADC_SingleChannel_Init(ADC_VREF_4V096,ADC_CH_AN8_P13);//如果VDD电压大于4.6V，则可以使用内部4.096V基准
        delay_ms(1);
    }
    else
    {
        printf("VDD电压需要5V，ADC基准才可使用4.096V！！！\r\n");
		while(1);
    }

    while(1)
    {
        ADCVAL=Get_ADCVAL();//获取ADC采样值
        Getvol=ADCVAL*4.096/4096.0;//转换成电压值

        printf("P13管脚ADC采样电压为：%.3fV\r\n",Getvol);
        delay_ms(500);

    }
}




