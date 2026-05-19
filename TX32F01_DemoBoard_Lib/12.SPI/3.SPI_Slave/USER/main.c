#include "TX32F01_periph.h"
#include "systick.h"
#include "uart.h"
#include "SPI.h"
#include "key.h"


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

//需要注意，SPI从机，必须把CS配置为硬件
u8 SPI_RX[256];
u8 i;
int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    SPI_Flash_Init();//SPi从机初始化

    while(1)
    {
				for(i=0;i<=0xFF;i++)
			{
					SPI_RX[i]=SPI_ReadWriteByte(i);
				}
    }
}




