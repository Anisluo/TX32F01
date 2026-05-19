#include "TX32F01_periph.h"
#include "ADC.h"


void ADC_Continuous_Init(u16 ADCVref,u8 Channel)
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
    ADC_InitStructure.ADC_Sequence_Lenth=ADC_Sequence_Lenth_8;//转换序列长度选择8
    ADC_InitStructure.ADC_SAMP_CLK=ADC_SAMP_CLK_28;//采样时间为28个周期
    ADC_InitStructure.ADC_CLK_DIV=ADC_CLK_DIV_8;//ADC时钟选择系统时钟8分频
    ADC_Init(&ADC_InitStructure);//ADC初始化

	//8个序列号全都使用相同的采样通道
    ADC_ChannelConfig(1,Channel);//第1个要转换的序列号
    ADC_ChannelConfig(2,Channel);//第2个要转换的序列号
    ADC_ChannelConfig(3,Channel);//第3个要转换的序列号
    ADC_ChannelConfig(4,Channel);//第4个要转换的序列号
    ADC_ChannelConfig(5,Channel);//第5个要转换的序列号
    ADC_ChannelConfig(6,Channel);//第6个要转换的序列号
    ADC_ChannelConfig(7,Channel);//第7个要转换的序列号
    ADC_ChannelConfig(8,Channel);//第8个要转换的序列号

    ADC_Cmd(ENABLE);//使能ADC
}

//连续采样8个通道，得到平均值
u16 Get_AverageADCVAL(void)
{
    u32 ADCVAL=0;
    u8 i;
	
    ADC_ClearFlag();//清除标志位
    ADC_StartConversion();//开始ADC转换
    while(!ADC_GetFlagStatus());//等待标志位置位
    for(i=1; i<=8; i++)
    {
        ADCVAL+=ADC_GetConversionValue(i);//得到通道序列号1~8的采样数据
    }
    return (u16)(ADCVAL>>3);//得到平均值
}






