#include "TX32F01_periph.h"
#include "ADC.h"


void ADC_SingleChannel_Init(u16 ADCVref,u8 Channel)
{
    ADC_InitTypeDef ADC_InitStructure;

    SCU_Unlock();// 解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_ADC,ENABLE);//打开ADC外设时钟
    SCU_PeriphClockCmd(Periph_GPIO1,ENABLE);//打开GPIO1外设时钟
    SCU_Lock();// 上锁SCU，不可对其余SCU寄存器操作。

    GPIO_Init(GPIO1,PIN03,GPIO_MODE_ANALOG);//选择P13为采样通道

    ADC_DeInit();//ADC寄存器恢复默认值

    ADC_InitStructure.ADC_Vref=ADCVref;//选择ADC采样电压基准
    ADC_InitStructure.ADC_DataAlign=ADC_ALIGN_Right;//选择数据右对齐
    ADC_InitStructure.ADC_Sequence_Lenth=ADC_Sequence_Lenth_1;//转换序列长度选择1
    ADC_InitStructure.ADC_SAMP_CLK=ADC_SAMP_CLK_28;//采样时间为28个周期
    ADC_InitStructure.ADC_CLK_DIV=ADC_CLK_DIV_8;//ADC时钟选择系统时钟8分频
    ADC_Init(&ADC_InitStructure);//ADC初始化

    ADC_ChannelConfig(1,Channel);//通道1采样通道(通道个数与结构体中的转换序列长度要匹配)

    ADC_Cmd(ENABLE);//使能ADC
}

//ADC采样一次，返回采样值
u16 Get_ADCVAL(void)
{
    ADC_ClearFlag();//清除标志位
    ADC_StartConversion();//开始ADC转换
    while(!ADC_GetFlagStatus());//等待标志位置位
    return ADC_GetConversionValue(1);//得到通道序列号1的采样数据
}

