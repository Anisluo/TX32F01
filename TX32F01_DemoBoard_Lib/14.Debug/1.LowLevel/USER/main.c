#include  "TX32F01_periph.h"
#include  "systick.h"
#include  "LED.h"


void SCU_Init(void)
{

    SCU_Unlock();			// 解锁SCU，可对其余SCU寄存器操作。

    SCU_SetSysClock(SysClock_24M);//系统时钟设置为32M

//    SCU_PeriphClockCmd(Periph_ALL,ENABLE);//打开外设时钟

    SCU_ResetPeriphClock(Periph_ALL);//复位所有外设模块

    SCU_SetBor(BOR_2P5V,ENABLE);//设置BOR2.5V

    SCU_ClearPWR_Flag();;	//清除所有上电复位标志位

    SCU_Lock();			// 上锁SCU，不可对其余SCU寄存器操作。

}

const u32 aaa[4];//为了增大代码量，用户可以改变数组大小来改变程序大小
int main(void)
{
    u8 i;
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    LED_Init();//LED P03初始化

    if(aaa[3]==1) i=3;

    while(1)
    {
        LED(1);//P03 输出高
        delay_ms(600);
        LED(0);//P03 输出低
        delay_ms(600);
    }
}



