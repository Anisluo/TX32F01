#ifndef __SYSTICK_H
#define __SYSTICK_H
#include "TX32F01_periph.h"

void delay_init(void);
void delay_ms(__IO uint32_t nTime);
void delay_us(__IO uint32_t nTime);

#endif





























