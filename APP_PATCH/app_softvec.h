#ifndef _APP_SOFTVEC_H
#define _APP_SOFTVEC_H

/*
 *  App 端封装：把自己的中断函数注册到 BL 的软向量表里。
 *  原理：CM0 没有 VTOR，App 跑起来后中断仍然会派发到 BL 在 0x01000000
 *  的向量表，BL 的所有 *_Handler 都是跳板，会查 SRAM 0x20000000 起的
 *  软向量表然后调用 App 设置的函数指针。
 *
 *  典型用法：
 *      #include "app_softvec.h"
 *      void my_uart_isr(void) { ... }
 *      ...
 *      app_softvec_register_irq(UART_IRQn, my_uart_isr);
 *      NVIC_EnableIRQ(UART_IRQn);
 *
 *  SysTick：
 *      app_softvec_register_systick(my_systick_handler);
 *      SysTick_Config(24000);
 *
 *  注意：App 工程的 startup_TX32F01.s 里的弱符号 *_Handler 不要再被
 *  其它 .c 文件强符号覆盖，否则就不是"通过 BL 跳板"了。
 */

#include "TX32F01_periph.h"

#define APP_SOFT_VECTOR_BASE   0x20000000UL
#define APP_SVEC_NMI           0
#define APP_SVEC_HARDFAULT     1
#define APP_SVEC_SVC           2
#define APP_SVEC_PENDSV        3
#define APP_SVEC_SYSTICK       4
#define APP_SVEC_IRQ_BASE      6

typedef void (*app_isr_t)(void);

static __inline void app_softvec_register_irq(IRQn_Type irqn, app_isr_t fn)
{
    volatile app_isr_t *tbl = (volatile app_isr_t *)APP_SOFT_VECTOR_BASE;
    tbl[APP_SVEC_IRQ_BASE + (int)irqn] = fn;
}

static __inline void app_softvec_register_systick(app_isr_t fn)
{
    volatile app_isr_t *tbl = (volatile app_isr_t *)APP_SOFT_VECTOR_BASE;
    tbl[APP_SVEC_SYSTICK] = fn;
}

static __inline void app_softvec_register_sys(int idx, app_isr_t fn)
{
    volatile app_isr_t *tbl = (volatile app_isr_t *)APP_SOFT_VECTOR_BASE;
    tbl[idx] = fn;
}

/* App 想触发"下次上电进入升级模式"调这个；
   写完会软复位。 */
void app_request_bootloader_update(void);

#endif
