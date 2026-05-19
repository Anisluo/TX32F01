#include "TX32F01_periph.h"
#include "systick.h"
#include "uart.h"
#include "ADC.h"
#include "ntc.h"

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

u16 ADCVAL;//ADC采样值
u32  NTCRES;//ADC电阻值
float Getvol;//ADC采样电压
float VDDVAL=3.3;//实际VDD电压
float RealTemp;//实时温度

int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率
    printf("NTC温敏电阻测试\r\n");

    ADC_SingleChannel_Init(ADC_VREF_2V048,ADC_CH_1P3VDD);//设置参考电压和采样通道，使用内部2.048V作为ADC采样基准
    delay_ms(1);

    ADCVAL=Get_ADCVAL();//得到1/3VDD电压值ADC转换值
    VDDVAL=ADCVAL*2.048/4096.0*3;//得到精确的VDD电压值
	
    ADC_ChannelConfig(1,ADC_CH_AN3_P02);//将序列1转换通道改为P02，准备采样NTC电压
	
    while(1)
    {
        ADCVAL=Get_ADCVAL();//得到热敏电阻ADC采样值
        Getvol=ADCVAL*2.048/4096.0;//将ADC采样值转换成实际的电压值

        NTCRES=10000*Getvol/(VDDVAL-Getvol);//将电压值转换成热敏电阻3435实际电阻值
        RealTemp=Compare_tempres(NTCRES);//查表将NTC电阻值转换成实际温度值
			
        printf("当前环境温度为：%.1f℃\r\n",RealTemp);
        delay_ms(500);
    }

}




