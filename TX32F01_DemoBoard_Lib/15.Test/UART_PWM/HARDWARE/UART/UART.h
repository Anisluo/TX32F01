#ifndef __UART_H
#define __UART_H
#include "TX32F01_periph.h"

// ----------- Function prototype -----------------------
void UART_NVIC_Init(u32 bps);
#define UART_RX_LINE_MAX 128

#endif
