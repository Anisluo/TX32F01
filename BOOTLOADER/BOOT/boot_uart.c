#include "boot_uart.h"

static volatile uint32_t s_ms_tick;

void buart_tick_1ms(void)        { s_ms_tick++; }
uint32_t buart_now_ms(void)      { return s_ms_tick; }

void buart_init(uint32_t bps)
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
    u.UART_BaudRate   = bps;
    u.UART_WordLength = UART_8DATABIT;
    u.UART_StopBits   = UART_1STOPBIT;
    u.UART_Parity     = UART_Pority_None;
    u.UART_Mode       = UART_Mode_Rx | UART_Mode_Tx;
    UART_Init(&u);

    /* 全程轮询，不开 UART 中断（避免和软向量打架）*/
    UART_Cmd(ENABLE);
}

void buart_send_byte(uint8_t c)
{
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}

void buart_send(const uint8_t *p, uint32_t n)
{
    while (n--) buart_send_byte(*p++);
}

BOOL buart_recv_byte(uint8_t *out, uint32_t timeout_ms)
{
    uint32_t t0 = s_ms_tick;
    for (;;) {
        if (UART_GetFlagStatus(UART_RDNEIF)) {
            *out = (uint8_t)(UART_ReceiveData() & 0xFF);
            UART_ClearFlag(UART_RDNEIF);
            return TRUE;
        }
        if (timeout_ms == 0) return FALSE;
        if ((uint32_t)(s_ms_tick - t0) >= timeout_ms) return FALSE;
    }
}
