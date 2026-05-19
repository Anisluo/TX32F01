#ifndef __ADC_H
#define __ADC_H
#include "TX32F01_periph.h"


void ADC_SingleChannel_Init(u16 ADCVref,u8 Channel);
u16 Get_ADCVAL(void);

#endif
