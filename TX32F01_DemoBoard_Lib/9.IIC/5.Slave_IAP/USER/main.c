#include "TX32F01_periph.h"
#include "systick.h"
#include "flash.h"
#include "i2c.h"

void SCU_Init(void)
{

    SCU_Unlock();			// 解锁SCU，可对其余SCU寄存器操作。

    SCU_SetSysClock(SysClock_4M);//系统时钟设置为24M

  	SCU_PeriphClockCmd(Periph_I2C,ENABLE);//打开外设时钟
  	SCU_PeriphClockCmd(Periph_GPIO1,ENABLE);//打开外设时钟

    SCU_ResetPeriphClock(Periph_ALL);//复位所有外设模块

    SCU_SetBor(BOR_2P5V,ENABLE);//设置BOR2.5V

    SCU->RSR|=0x1;	//清除所有上电复位标志位

    SCU_Lock();			// 上锁SCU，不可对其余SCU寄存器操作。
}

#define blSyscfgRemapMemorySram()         {SCU->PKR = 0xBADBEE; \
											SCU->BSR = 1; \
											SCU->PKR = 0; \
                                          }

//中断向量映射复制地址，在APP工程中需要设置RAM起始地址,前160个字节用来保护中断向量
//APP中需调用，BootLoader不需要
void fpSetVectorTableSRAM(void)
{
    u8 i;
    for(i = 0; i < 40; i++)
    {
        *((u32*)(0x20000000+(i<<2))) = *(__IO uint32_t*)(appAddr + (i<<2));
    }
    blSyscfgRemapMemorySram();
}

typedef  void (*pFunction)(void);
pFunction Jump_To_Application;

u8 GotoAppEntry(void)
{
    u32 JumpAddress;

    if (((*(__IO u32*)appAddr) & 0x2FFE0000 ) == 0x20000000)
    {
        /*用户添加程序：关闭程序使用的所有中断，如 NVIC_DisableIRQ(TIM0_IRQn);*/
        SCU_Init();//复位所有外设模块

        fpSetVectorTableSRAM();//复制中断向量到SRAM

        /* Jump to user application */
        JumpAddress = *(__IO u32*) (appAddr + 4);
        Jump_To_Application = (pFunction) JumpAddress;

        /* Initialize user application's Stack Pointer */
        __set_MSP(*(__IO u32*) appAddr);

        /* Jump to application */
        Jump_To_Application();
    }
    return truefp;
}

//初始化看门狗
void IWDG_Init()
{
    IWDT->KR = 0x5555;
    IWDT->PR = 0x07;
    IWDT->RLR = 0xfff;
    CLEAR_WATCH_DOG();
}

u8 CloseIIC=falsefp;

void BOOT_APP_CHOOSE(void)
{
    u16 EnterAPPFlag;

    EnterAPPFlag=*((u32*)(Address_EnterAPPFlag))&0xffff;//能否进入APP的标志位，由Address_EnterAPPFlag地址数据决定

    if(EnterAPPFlag==0x5555)//若Flash读到0x5555，则代表可以进入APP
    {
        gotoBootloaderFlag = falsefp;//可以进入APP
        IWDT->KR = 0x5555;
        if(IWDT->RLR != 0xffa&&IWDT->RLR != 0xfff)//有可能主机需要复位芯片读取芯片信息，因此利用看门狗一个寄存器来做一个标志位（看门狗寄存器复位不会改变）
        {
            CloseIIC=truefp;//等待期间关闭金手指上的IIC
        }
    }
    else
    {
        gotoBootloaderFlag = truefp;//程序需升级，不进入APP
        CloseIIC=falsefp;
    }
}

//这是一个Bootloader Demo
#define wait_1us  24
int main(void)
{
    u32 i=0;
    SCU_Init();//系统时钟初始化

    BOOT_APP_CHOOSE();//Boot选择
    IWDG_Init();//看门狗仅修改喂狗时间，不初始化，但喂狗
    if(CloseIIC==falsefp)//如果APP程序是好的，则不开启IIC0，防止金手指与其他主机产生握手
    {
        i2cSlaveCfg();//用户可以根据需求选择Bootloader的IIC管脚，P15 SCL ;   P16 SDA
    }
		
    while(1)
    {
        i++;
        CLEAR_WATCH_DOG();

        if(I2C->SR!=0xf8) 	  
        {
            I2C_Bootloader();
        }

        if(i > (10000*wait_1us/3))//上电后总共约等待100ms后进入APP
        {
            if(truefp != gotoBootloaderFlag)
            {
                {
                    GotoAppEntry();
                }
            }
        }
    }

}
