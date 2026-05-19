#include "TX32F01_periph.h"
#include "systick.h"
#include "uart.h"
#include "pwm.h"

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

//P35作为输入捕获，捕获P03的PWM频率
extern u16 FrequencyAve;//PWM捕获频率
int main(void)
{
    u32  PWMFre=1000;//设置初始频率
    SCU_Init();//系统时钟初始化
    delay_init();//使用systick作为延时
    UART_NVIC_Init(115200);//初始化串口，设置波特率
    printf("请连接管脚P35和P03！\r\n");

    TIM0_PWM_Init(PWMFre,50);//PWM初始化
	TIM1_Capture_Init();//初始化输入捕获
	delay_ms(5);
	
    while(1)
    {
        TIM0_PWM_Set_FreDuty(PWMFre++,50);//设置值占空比
        delay_ms(500);
        printf("P35捕获到的PWM频率为：%dHz！！！\r\n",FrequencyAve);
    }
}






