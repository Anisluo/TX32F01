#include "TX32F01_periph.h"
#include "systick.h"
#include "uart.h"
#include "flash.h"
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

u8 TEXT_Buffer[]= {"SPI FLASH TEST Num:1!\r\n"};//往SPI FLASH要写入的数据
#define SIZE sizeof(TEXT_Buffer)//得到数组长度

u8 RX_Buffer[SIZE];//从SPI FLASH读出的数据

int main(void)
{
    u8 i=0,NUM=0x31;
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率
    KEY_Init();//LED P03初始化
    SPI_Flash_Init();

    printf("SPI FLASH读写实验!!!\r\n");

    while((SPI_Flash_ReadID()&W25QXX)!=W25QXX);//检测不到spi Falsh

    SPI_Flash_WAKEUP();//唤醒Flash，防止睡眠

    printf("请按下S2按键!!!\r\n");
    while(1)
    {
        if(!KEY_P12)//判断按键按下
        {
            delay_ms(30);//消抖
            if(!KEY_P12)//再次确认按键是否按下
            {
                SPI_Flash_Erase_Sector(0);//擦除扇区

                TEXT_Buffer[19]=NUM++;//修改数组数据
                SPI_Flash_Write_NoCheck((u8*)TEXT_Buffer,0,SIZE);//往0地址写入数据

                SPI_Flash_Read((u8*)RX_Buffer,0,SIZE);//读出写入的数据

                printf("Flash写入的数据为：");
                for(i=0; i<SIZE; i++)
                {
                    printf("%c",RX_Buffer[i]);
                }
                while(!KEY_P12);//等待按键释放
            }
        }
    }
}




