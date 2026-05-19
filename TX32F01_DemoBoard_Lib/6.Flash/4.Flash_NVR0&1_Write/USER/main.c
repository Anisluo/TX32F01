#include "TX32F01_periph.h"
#include "systick.h"


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

//操作方式：进入debug界面后，单步执行，观察扇区数据是否删除及写入
int main(void)
{
    u32 i;
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    Flash_Unlock();//解锁Flash
    Flash_Main_WriteEease_Enable();//使能main区擦写操作
	Flash_SCU24MHz_ClkCfg();//系统时钟设置为24MHz，必须要调用此函数

    //一个扇区512字节
	delay_ms(2000);
    Flash_EraseSector(0x1008000);//擦除NVR0扇区
    Flash_EraseSector(0x1008200);//擦除NVR1扇区
	
    for(i=0x1008000; i<0x10083FF; i+=4)
    {
        FLASH_ProgramOneWord(i,0x5A5A5A5A);//以32位数据格式往Flash写数据
    }


    while(1);
}







