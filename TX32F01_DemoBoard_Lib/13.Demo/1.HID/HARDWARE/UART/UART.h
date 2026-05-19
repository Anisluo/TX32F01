#ifndef __UART_H
#define __UART_H
#include "TX32F01_periph.h"

// ----------- Function prototype -----------------------
void UART_NVIC_Init(u32 bps);
void SendStr(unsigned char *dat);
void SendHex(unsigned char dat);


#endif
