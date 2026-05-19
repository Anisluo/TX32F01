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

u32 code32[]= {0x11111111,0x2222,0x33333333};
u16 code16[]= {0x11,0x2222,0x33,0x4444};
u8 code8[]= {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99};

//操作方式：进入debug界面后，单步执行，观察扇区数据是否删除及写入
//注意写入地址的对齐问题，例如，以Word方式写数据，地址格式只能是0x100xxx0、能是0x100xxx4、能是0x100xxx8、能是0x100xxxc。

int main(void)
{
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    Flash_Unlock();//解锁Flash
    Flash_Main_WriteEease_Enable();//使能main区擦写操作
    Flash_SCU24MHz_ClkCfg();//系统时钟设置为24MHz，必须要调用此函数
	
    //一个扇区512字节
    delay_ms(2000);

    Flash_EraseSector(0x1002000);//擦除扇区
    FLASH_ProgramWord(0x1002000,code32,3);//以32位数据格式往Flash写数据
    FLASH_ProgramOneWord(0x100200C,0x5A5A5A5A);//往地址0x100200C写入0x5A5A5A5A
		
    Flash_EraseSector(0x1003000);//擦除扇区
    FLASH_ProgramHalfWord(0x1003000,code16,4);//以16位数据格式往Flash写数据
    FLASH_ProgramOneHalfWord(0x1003008,0xA5A5);//往地址0x1003008写入0x5A5A5A5A
		
		
    Flash_EraseSector(0x1004000);//擦除扇区
    FLASH_ProgramByte(0x1004000,code8,9);//以8位数据格式往Flash写数据
    FLASH_ProgramOneByte(0x1004009,0xA5);//往地址0x1004009写入0x5A5A5A5A

    while(1);
}







