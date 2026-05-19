/*
 * APP_RTOS bare-metal demo
 * - Direct flash to 0x01000000 via SWD (no bootloader)
 * - FreeRTOS V10.6.2 CM0 port, static allocation only
 * - 2 tasks:
 *     led_task : 200 ms blink on GPIO0/PIN03
 *     uart_task: 1 s "[RTOS] tick=N" on GPIO3 P07/P06 @115200
 * - Demonstrates real preemptive scheduling via PendSV
 */
#include "TX32F01_periph.h"
#include "FreeRTOS.h"
#include "task.h"

/* -------- BSP -------- */
static void scu_init_24mhz(void)
{
    SCU_Unlock();
    SCU_SetSysClock(SysClock_24M);
    SCU_ResetPeriphClock(Periph_ALL);
    SCU_SetBor(BOR_2P5V, ENABLE);
    SCU_ClearPWR_Flag();
    SCU_Lock();
}

static void led_init(void)
{
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO0, ENABLE);
    SCU_Lock();
    GPIO_Init(GPIO0, PIN03, GPIO_MODE_OUTPUT_PP);
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
    u.UART_BaudRate   = 115200;
    u.UART_WordLength = UART_8DATABIT;
    u.UART_StopBits   = UART_1STOPBIT;
    u.UART_Parity     = UART_Pority_None;
    u.UART_Mode       = UART_Mode_Rx | UART_Mode_Tx;
    UART_Init(&u);
    UART_Cmd(ENABLE);
}

static void uart_putc(char c) {
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_put_u32(uint32_t v) {
    char b[11]; int n = 0;
    if (!v) { uart_putc('0'); return; }
    while (v && n < 10) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) uart_putc(b[n]);
}

/* -------- Tasks -------- */
static StaticTask_t s_led_tcb;
static StackType_t  s_led_stack[configMINIMAL_STACK_SIZE];

static StaticTask_t s_uart_tcb;
static StackType_t  s_uart_stack[configMINIMAL_STACK_SIZE];

static void led_task(void *arg)
{
    (void)arg;
    for (;;) {
        GPIO_Toggle(GPIO0, PIN03);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void uart_task(void *arg)
{
    (void)arg;
    uint32_t n = 0;
    uart_puts("\r\n[RTOS] scheduler running, 2 tasks\r\n");
    for (;;) {
        uart_puts("[RTOS] uart_task tick=");
        uart_put_u32(n++);
        uart_puts("\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* -------- FreeRTOS static allocation callbacks --------
 * Required because configSUPPORT_STATIC_ALLOCATION=1 and
 * configSUPPORT_DYNAMIC_ALLOCATION=0: the kernel asks us where to
 * put the idle task's TCB+stack.
 */
static StaticTask_t s_idle_tcb;
static StackType_t  s_idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &s_idle_tcb;
    *ppxIdleTaskStackBuffer = s_idle_stack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

/* -------- main -------- */
int main(void)
{
    scu_init_24mhz();
    led_init();
    uart_init_115200();

    uart_puts("\r\n[RTOS] boot, creating tasks...\r\n");

    xTaskCreateStatic(led_task,  "led",  configMINIMAL_STACK_SIZE,
                      NULL, 1, s_led_stack,  &s_led_tcb);

    xTaskCreateStatic(uart_task, "uart", configMINIMAL_STACK_SIZE,
                      NULL, 1, s_uart_stack, &s_uart_tcb);

    vTaskStartScheduler();      /* never returns */

    uart_puts("[RTOS] scheduler returned (FATAL)\r\n");
    for (;;) { }
}
