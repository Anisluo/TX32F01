/*
 * bsp_uart.h
 *
 * Minimal blocking UART for diagnostic output only -- no shell, no RX
 * handling. The point of this module is to spit out periodic stats so
 * a human watching the serial line can see "the emulator received N
 * commands, responded with M programs, etc." while a host SPI master
 * is exercising it.
 *
 *   115200 8N1 on GPIO3.PIN06 (RX, unused) / GPIO3.PIN07 (TX)
 *
 * The TX is blocking polling -- no IRQ, no buffer. That's safe because
 * we only call it from the main loop, never from the SPI ISR.
 */
#ifndef APP_SPINOR_BSP_UART_H
#define APP_SPINOR_BSP_UART_H

#include <stdint.h>

void bsp_uart_init_115200(void);

void bsp_uart_putc(char c);
void bsp_uart_puts(const char *s);
void bsp_uart_put_u32(uint32_t v);
void bsp_uart_put_hex(uint32_t v);

#endif
