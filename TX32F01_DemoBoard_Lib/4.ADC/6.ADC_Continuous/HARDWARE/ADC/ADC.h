#ifndef __ADC_H
#define __ADC_H

#include "TX32F01_periph.h"

void ADC_Continuous_Init(u16 ADCVref,u8 Channel);
u16 Get_AverageADCVAL(void);

#endif
