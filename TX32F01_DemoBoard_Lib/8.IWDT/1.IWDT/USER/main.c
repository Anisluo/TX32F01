#include "TX32F01_periph.h"
#include "systick.h"
#include "uart.h"
#include "IWDT.h"
#include "KEY.h"


void SCU_Init(void)
{

    SCU_Unlock();			// 解锁SCU，可对其余SCU寄存器操作。

    SCU_SetSysClock(SysClock_24M);//系统时钟设置为32M

//	SCU_PeriphClockCmd(Periph_ALL,ENABLE);//打开外设时钟

    SCU_ResetPeriphClock(Periph_ALL);//复位所有外设模块

    SCU_SetBor(BOR_2P5V,ENABLE);//设置BOR2.5V

    SCU->RSR|=0x1;	//清除所有上电复位标志位

    SCU_Lock();			// 上锁SCU，不可对其余SCU寄存器操作。

}

//测试方式：下载程序后，需要不断按下S2按键来进行喂狗，否则超时后会发生看门狗复位
int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    KEY_Init();//LED P03初始化
    UART_NVIC_Init(115200);//初始化串口，设置波特率

    printf("芯片发生复位！！！\r\n");
    printf("按下S2按键可以喂狗！！！\r\n");

    IWDT_ON();//开启看门狗

    while(1)
    {
        if(!KEY_P12)//判断按键按下
        {
            delay_ms(30);//消抖
            if(!KEY_P12)//再次确认按键是否按下
            {
                Feed_Dog();//喂狗
            }
        }
    }
}




