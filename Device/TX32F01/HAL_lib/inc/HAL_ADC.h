#ifndef _HAL_ADC_H
#define _HAL_ADC_H

#include "fpdefine.h"
#include "TX32F01.h"


typedef struct {
	
	  uint16_t ADC_Vref;//ADC基准电压
	  uint16_t ADC_DataAlign;//ADC对齐方式
	  uint16_t ADC_Sequence_Lenth;//ADC转换序列长度
	  uint16_t ADC_CLK_DIV;//ADC时钟分频
	  uint16_t ADC_SAMP_CLK;//ADC采样时间
	
} ADC_InitTypeDef;


/* UART_CFGR	*/
#define ADC_VREF_MASK 			  (~0x3<<10)
#define ADC_VREF_VDD   				(0x2<<10)
#define ADC_VREF_2V048  			(0x0<<10)
#define ADC_VREF_4V096  			(0x1<<10)

#define ADC_ALIGN_Right  			(0x0<<9)
#define ADC_ALIGN_Left  			(0x1<<9)

#define ADC_SAMP_CLK_MASK 		(~0x7<<4)
#define ADC_SAMP_CLK_7    		(0x0<<4)
#define ADC_SAMP_CLK_13  		  (0x1<<4)
#define ADC_SAMP_CLK_28   		(0x2<<4)
#define ADC_SAMP_CLK_41   		(0x3<<4)
#define ADC_SAMP_CLK_55  		  (0x4<<4)
#define ADC_SAMP_CLK_71   		(0x5<<4)
#define ADC_SAMP_CLK_160  		(0x6<<4)
#define ADC_SAMP_CLK_239  		(0x7<<4)

#define ADC_CLK_DIV_MASK  		(~0xF))
#define ADC_CLK_DIV_1     		(0)
#define ADC_CLK_DIV_2     		(1)
#define ADC_CLK_DIV_4     		(2)
#define ADC_CLK_DIV_6     		(3)
#define ADC_CLK_DIV_8     		(4)
#define ADC_CLK_DIV_10    		(5)
#define ADC_CLK_DIV_12    		(6)
#define ADC_CLK_DIV_16    		(7)
#define ADC_CLK_DIV_32    		(8)
#define ADC_CLK_DIV_64    		(9)
#define ADC_CLK_DIV_128   		(10)
#define ADC_CLK_DIV_256   		(11)


/* UART_SQR1*/
#define ADC_Sequence_Lenth_1  (0<<12)
#define ADC_Sequence_Lenth_2  (1<<12)
#define ADC_Sequence_Lenth_3  (2<<12)
#define ADC_Sequence_Lenth_4  (3<<12)
#define ADC_Sequence_Lenth_5  (4<<12)
#define ADC_Sequence_Lenth_6  (5<<12)
#define ADC_Sequence_Lenth_7  (6<<12)
#define ADC_Sequence_Lenth_8  (7<<12)


/* UART_SQR2*/
#define ADC_CH_AN1_P00    		(0x01)
#define ADC_CH_AN2_P01    		(0x02)
#define ADC_CH_AN3_P02   		  (0x03)
#define ADC_CH_AN4_P03    		(0x04)
#define ADC_CH_AN5_P10    		(0x05)
#define ADC_CH_AN6_P11    		(0x06)
#define ADC_CH_AN7_P12    		(0x07)
#define ADC_CH_AN8_P13    		(0x08)
#define ADC_CH_AN9_P14    		(0x09)
#define ADC_CH_AN10_P15   		(0x0A)
#define ADC_CH_AN11_P16   		(0x0B)
#define ADC_CH_AN12_P17   		(0x0C)
#define ADC_CH_AN13_P20  		  (0x0D)
#define ADC_CH_AN14_P21   		(0x0E)
#define ADC_CH_AN15_P22   		(0x0F)
#define ADC_CH_1P3VDD     		(0x10)
#define ADC_CH_GND        		(0x11)

void ADC_DeInit(void);
void ADC_Init(ADC_InitTypeDef *ADC_InitStruct);
void ADC_ChannelConfig(uint8_t Rank,uint8_t Channel);
void ADC_Cmd(FunctionalState NewState);
void ADC_StartConversion(void);
BOOL ADC_GetFlagStatus(void);
void ADC_ClearFlag(void);
void ADC_ITConfig(FunctionalState NewState);
uint16_t ADC_GetConversionValue(uint8_t Rank);


#endif




