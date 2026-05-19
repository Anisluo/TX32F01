#include "TX32F01_periph.h"
#include "systick.h"
#include "exti.h"
#include "LED.h"


void SCU_Init(void)
{

    SCU_Unlock();			// 解锁SCU，可对其余SCU寄存器操作。

    SCU_SetSysClock(SysClock_24M);//系统时钟设置为32M

    //SCU_PeriphClockCmd(Periph_ALL,ENABLE);//打开外设时钟

    SCU_ResetPeriphClock(Periph_ALL);//复位所有外设模块

    SCU_SetBor(BOR_2P5V,ENABLE);//设置BOR2.5V

    SCU->RSR|=0x1;	//清除所有上电复位标志位

    SCU_Lock();			// 上锁SCU，不可对其余SCU寄存器操作。

}

//程序测试方法：按下按键S2，可以看到LED亮灭
int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    EXTI_P12_Init();//初始化P12按键中断
    LED_Init();//LED P03初始化
    LED(1);//初始化LED

    while(1);
}



