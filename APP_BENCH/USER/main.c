/*
 * APP_BENCH/USER/main.c — entry point for CPU benchmarking.
 *
 * What runs
 * ---------
 *   1) Built-in bench_core suite (q15, memcpy, sort, CRC, FOC inner).
 *      Always runs — useful as a daily sanity check.
 *   2) Optional Coremark — only compiled in if COREMARK_ENABLED is
 *      defined at build time AND the Coremark sources have been added
 *      to APP_BENCH/COREMARK/.
 */
#include "TX32F01_periph.h"
#include "../../APP_PATCH/app_softvec.h"
#include "../BENCH/bench_core.h"

#ifdef COREMARK_ENABLED
extern int coremark_main(void);     /* renamed Coremark entry */
#endif

static void scu_init_24mhz(void)
{
    SCU_Unlock();
    SCU_SetSysClock(SysClock_24M);
    SCU_ResetPeriphClock(Periph_ALL);
    SCU_SetBor(BOR_2P5V, ENABLE);
    SCU_ClearPWR_Flag();
    SCU_Lock();
}

static void uart_init_115200(void)
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
    u.UART_BaudRate = 115200; u.UART_WordLength = UART_8DATABIT;
    u.UART_StopBits = UART_1STOPBIT; u.UART_Parity = UART_Pority_None;
    u.UART_Mode     = UART_Mode_Rx | UART_Mode_Tx;
    UART_Init(&u);
    UART_Cmd(ENABLE);
}

static void putc_uart(char c)
{
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}

static void puts_uart(const char *s) { while (*s) putc_uart(*s++); }

int main(void)
{
    scu_init_24mhz();
    uart_init_115200();

    puts_uart("\r\n[BENCH] APP_BENCH up @ 24 MHz\r\n");

    /* Always: in-house baseline */
    bench_run_all(putc_uart);

#ifdef COREMARK_ENABLED
    puts_uart("[BENCH] starting EEMBC Coremark...\r\n");
    coremark_main();
#else
    puts_uart("[BENCH] Coremark not built. See APP_BENCH/README.md to enable.\r\n");
#endif

    puts_uart("[BENCH] done. spinning.\r\n");
    for (;;) { }
}
