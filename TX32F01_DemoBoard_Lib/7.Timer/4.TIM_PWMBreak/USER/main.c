#include "TX32F01_periph.h"
#include "systick.h"
#include "uart.h"
#include "pwm.h"

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

int main(void)
{
    u8 i;
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率
    printf("按下S2按键，PWM刹车，LED灭！\r\n");
    printf("松开S2按键，刹车恢复！\r\n");
	
    TIM0_PWMBDTR_Init(10000,50);//PWM初始化
	delay_ms(5);
    while(1)
    {
        for(i=1; i<100; i++)
        {
            TIM0_PWM_Set_Duty(100-i);//更改占空比
            delay_ms(20);
        }
        for(i=1; i<100; i++)
        {
            TIM0_PWM_Set_Duty(i);//更改占空比
            delay_ms(20);
        }
    }
}






