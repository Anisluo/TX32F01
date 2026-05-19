#include "TX32F01_periph.h"
#include "systick.h"
#include "uart.h"
#include "24cxx.h"

void SCU_Init(void)
{

    SCU_Unlock();			// 解锁SCU，可对其余SCU寄存器操作。

    SCU_SetSysClock(SysClock_24M);//系统时钟设置为24M

    SCU_PeriphClockCmd(Periph_ALL,ENABLE);//打开外设时钟

    SCU_ResetPeriphClock(Periph_ALL);//复位所有外设模块

    SCU_SetBor(BOR_2P5V,ENABLE);//设置BOR2.5V

    SCU->RSR|=0x1;	//清除所有上电复位标志位

    SCU_Lock();			// 上锁SCU，不可对其余SCU寄存器操作。
}

const u8 TEXT_Buffer[]= {"IIC连续读写测试!!!\r\n"};
#define SIZE sizeof(TEXT_Buffer)
u8 RX_Buffer[SIZE];

int main(void)
{
    u8 i;
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率

    AT24CXX_Init(400000);//硬件IIC主机初始化，频率400K
    while(AT24CXX_Check());//检测AT24c02是否存在

    AT24CXX_Write(0,(u8*)TEXT_Buffer,SIZE);//从0地址开始写数据
	
    AT24CXX_Read(0,RX_Buffer,SIZE);//将数据读出来

    while(1)
    {
        printf("EEPROM写入的数据为：");
        for(i=0; i<SIZE; i++)
        {
            printf("%c",RX_Buffer[i]);
        }
        delay_ms(500);
    }
}




