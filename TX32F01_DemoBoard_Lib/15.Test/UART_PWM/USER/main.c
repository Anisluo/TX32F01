#include "TX32F01_periph.h"
#include "systick.h"
#include "UART.h"
#include "LED.h"
#include "pwm.h"
#include "scpi_pwm.h"

void SCU_Init(void)
{

    SCU_Unlock();			// 解锁SCU，可对其余SCU寄存器操作。

    SCU_SetSysClock(SysClock_24M);//系统时钟设置为24M

//  SCU_PeriphClockCmd(Periph_ALL,ENABLE);//打开所有外设时钟

    SCU_ResetPeriphClock(Periph_ALL);//复位所有外设模块

    SCU_SetBor(BOR_2P5V,ENABLE);//设置BOR2.5V

    SCU->RSR|=0x1;	//清除所有上电复位标志位

    SCU_Lock();			// 上锁SCU，不可对其余SCU寄存器操作。
}

int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率
    LED_Init();//LED P03初始化
		TIM0_PWM_Init(1000,50);//PWM初始化,使用1kHz占空比
	
		//注册指令系统
	// 注册各子系统命令
    SCPI_PWM_Register();
    while(1){
		__WFI();//任务到来时才启动
		}
}




