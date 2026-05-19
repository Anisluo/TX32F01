#include "TX32F01_periph.h"
#include "systick.h"
#include "exti.h"
#include "LED.h"
#include "UART.h"
#include "CH372.h"
#include "usb.h"
#include "CH372.h"
#include "CH375INC.h"

void SCU_Init(void)
{

    SCU_Unlock();			// 解锁SCU，可对其余SCU寄存器操作。

    SCU_SetSysClock(SysClock_24M);//系统时钟设置为32M

    SCU_PeriphClockCmd(Periph_ALL,ENABLE);//打开外设时钟

    SCU_ResetPeriphClock(Periph_ALL);//复位所有外设模块

    SCU_SetBor(BOR_2P5V,ENABLE);//设置BOR2.5V

    SCU->RSR|=0x1;	//清除所有上电复位标志位

    SCU_Lock();			// 上锁SCU，不可对其余SCU寄存器操作。

}

//程序测试方法：按下按键S2，可以看到LED亮灭
int main(void)
{

    SCU_Init();//系统时钟初始化
	  CH375_GPIOInit();
	  UART_NVIC_Init(256000);

	  LED_Init();
		LED(0);
 
  	delay50ms( );	/* 延时等待CH375初始化完成,如果单片机由CH375提供复位信号则不必延时 */
   	CH375_Init( );  /* 初始化CH375 */
		delay50ms( );		
		while(1){
		}
}



