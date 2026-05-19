#include "TX32F01_periph.h"
#include "systick.h"
#include "stop.h"
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

//LED慢闪3次后进入STOP模式，按下按键S2唤醒，唤醒后LED快闪。
//用户可以取下子板后，直接在子板上测量功耗
//STOP模式低功耗电压大约为5~8uA。
int main(void)
{
    u8 i=0;
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    EXTI_P12_Init();//外部中断唤醒STOP模式
    LED_Init();//LED P03初始化

    //按下S2按键，唤醒

    while(1)
    {
        LED_TOGGLE();//LED翻转
        delay_ms(100);
        i++;
        if(i>20)
        {
            LED(0);
            PWR_EnterSTOPMode_WFI();//进入STOP模式，按下S2按键可以唤醒
            i=0;
        }
    }
}




