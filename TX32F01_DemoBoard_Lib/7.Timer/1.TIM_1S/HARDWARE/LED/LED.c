#include "TX32F01_periph.h"
#include "LED.h"
#include "systick.h"


void LED_Init(void)
{
    SCU_Unlock();// 解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_GPIO0,ENABLE);//打开外设时钟
    SCU_Lock();// 上锁SCU，不可对其余SCU寄存器操作。

    GPIO_Init(GPIO0,PIN03,GPIO_MODE_OUTPUT_PP);//LED管脚配置成推挽输出
}
