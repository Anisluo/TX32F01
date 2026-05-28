#include "bsp_uart.h"
#include "TX32F01_periph.h"

void bsp_uart_init_115200(void)
{
    UART_InitTypeDef u;
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    SCU_PeriphClockCmd(Periph_UART,  ENABLE);
    SCU_Lock();

    GPIO_Init(GPIO3, PIN06, GPIO_MODE_AF);
    GPIO_Init(GPIO3, PIN07, GPIO_MODE_AF);
    GPIO_PinRemapConfig(GPIO3, PIN07, GPIO_AF_UART_TX);
    GPIO_PinRemapConfig(GPIO3, PIN06, GPIO_AF_UART_RX);

    UART_DeInit();
    u.UART_BaudRate   = 115200;
    u.UART_WordLength = UART_8DATABIT;
    u.UART_StopBits   = UART_1STOPBIT;
    u.UART_Parity     = UART_Pority_None;
    u.UART_Mode       = UART_Mode_Rx | UART_Mode_Tx;
    UART_Init(&u);
    UART_Cmd(ENABLE);
}

void bsp_uart_putc(char c)
{
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}

void bsp_uart_puts(const char *s) { while (*s) bsp_uart_putc(*s++); }

void bsp_uart_put_u32(uint32_t v)
{
    char b[11]; int n = 0;
    if (!v) { bsp_uart_putc('0'); return; }
    while (v && n < 10) { b[n++] = (char)('0' + v % 10U); v /= 10U; }
    while (n--) bsp_uart_putc(b[n]);
}

void bsp_uart_put_hex(uint32_t v)
{
    static const char H[] = "0123456789ABCDEF";
    int i;
    bsp_uart_putc('0'); bsp_uart_putc('x');
    for (i = 7; i >= 0; i--) bsp_uart_putc(H[(v >> (i * 4)) & 0xFU]);
}
