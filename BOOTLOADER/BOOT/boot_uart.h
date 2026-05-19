#ifndef _BOOT_UART_H
#define _BOOT_UART_H

#include "boot_layout.h"

void buart_init(uint32_t bps);
void buart_send_byte(uint8_t c);
void buart_send(const uint8_t *p, uint32_t n);

/* timeout_ms: 0 = 立刻返回；>0 = 最多等这么久；ms 基准来自外部 1ms 计数 */
BOOL buart_recv_byte(uint8_t *out, uint32_t timeout_ms);

void buart_tick_1ms(void);   /* 由 SysTick_Handler 调用 */
uint32_t buart_now_ms(void);

#endif
