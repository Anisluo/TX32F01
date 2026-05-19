#include "TX32F01_periph.h"
#include "IWDT.h"



void IWDT_ON(void)
{
    IWDT_Init(IWDT_PR_256,240);//初始化IWDT,复位时间大约1.8秒
    IWDT_CmdEnable();//开启看门狗
}

void Feed_Dog(void)
{
    IWDT_ReloadCounter();//喂狗
}



