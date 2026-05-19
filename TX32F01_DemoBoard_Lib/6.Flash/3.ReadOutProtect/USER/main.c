#include "TX32F01_periph.h"
#include "systick.h"
#include "LED.h"


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

u32 code32[]= {0x11111111,0x2222};
u16 code16[]= {0x11,0x2222,0x33,0x4444};
u8 code8[]= {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99};

//操作方式：1.进入debug界面后，断点打到开启读保护，观察开启读保护后，程序数据是否还能正常读出
//2.退出debug模式后，芯片需要重新上电，观察LED是否先满闪3次，然后快闪5次，最终LED不再闪烁，说明解除读保护导致FLASH全片擦除
int main(void)
{
    u8 i=0;
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    Flash_Unlock();//解锁Flash
    Flash_Main_WriteEease_Enable();//使能main区擦写操作
	Flash_SCU24MHz_ClkCfg();//系统时钟设置为24MHz，必须要调用此函数
    LED_Init();//LED P03初始化
	
    for(i=0; i<6; i++)
    {
        LED_TOGGLE();//LED慢速翻转
        delay_ms(500);
    }

    ReadOutProtectEnable();//开启读保护，无法使用SWD读取程序区数据

    i=0;
    //退出debug模式后，可以发现继续闪灯，设置程序读保护不影响程序运行（设置读保护后，需要重新上电才能观测到下面程序的运行）
    while(1) 
    {
        LED_TOGGLE();//LED翻转
        delay_ms(100);
        i++;
        if(i>=6)
        {
            ReadOutProtectDisable();//解除读保护，注意，一旦解除读保护后，程序区会自动擦除，因此快速闪灯5次后不再闪灯。
        }
    }
}







