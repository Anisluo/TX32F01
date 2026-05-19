#include "boot_jump.h"
#include "boot_flash.h"

/* 软向量表"指针的指针"：它本身放在 BL 的 RW 段，但指向 SRAM 头 */
volatile isr_t * const g_soft_vec = (isr_t *)SOFT_VECTOR_BASE;

/* 由 BL 自己的所有中断入口共用的跳板 —— 见 startup_TX32F01.s
   里的所有 *_Handler 都改为 BL_Trampoline_<name> 然后在这里实现 */
#define DEFAULT_TRAMP(name, idx)                            \
    void name(void) {                                       \
        if (g_soft_vec[idx]) g_soft_vec[idx]();             \
    }

/* 系统异常 */
void NMI_Handler(void)        { if (g_soft_vec[SVEC_NMI])       g_soft_vec[SVEC_NMI](); }
void HardFault_Handler(void)  { if (g_soft_vec[SVEC_HARDFAULT]) g_soft_vec[SVEC_HARDFAULT](); else for(;;){} }
void SVC_Handler(void)        { if (g_soft_vec[SVEC_SVC])       g_soft_vec[SVEC_SVC](); }
void PendSV_Handler(void)     { if (g_soft_vec[SVEC_PENDSV])    g_soft_vec[SVEC_PENDSV](); }

/* SysTick 在 BL 阶段被自己用来计 ms（boot_uart 用），跳到 App 之前我们：
   1. 关 SysTick
   2. 让跳板转发给 App 注册的 ISR
   App 自己 SysTick_Config() 时会再开。 */
extern void buart_tick_1ms(void);
void SysTick_Handler(void) {
    if (g_soft_vec[SVEC_SYSTICK]) g_soft_vec[SVEC_SYSTICK]();
    else buart_tick_1ms();
}

/* 18 个外设中断，全部跳板 */
DEFAULT_TRAMP(EXTI9_IWDT_Handler, SVEC_OF(IWDT_IRQn))
DEFAULT_TRAMP(PVD_Handler,        SVEC_OF(PVD_IRQn))
DEFAULT_TRAMP(FLASH_Handler,      SVEC_OF(FLASH_IRQn))
DEFAULT_TRAMP(EXTI0_Handler,      SVEC_OF(EXTI0_IRQn))
DEFAULT_TRAMP(EXTI1_Handler,      SVEC_OF(EXTI1_IRQn))
DEFAULT_TRAMP(EXTI2_Handler,      SVEC_OF(EXTI2_IRQn))
DEFAULT_TRAMP(EXTI3_Handler,      SVEC_OF(EXTI3_IRQn))
DEFAULT_TRAMP(EXTI4_Handler,      SVEC_OF(EXTI4_IRQn))
DEFAULT_TRAMP(EXTI5_Handler,      SVEC_OF(EXTI5_IRQn))
DEFAULT_TRAMP(EXTI6_Handler,      SVEC_OF(EXTI6_IRQn))
DEFAULT_TRAMP(EXTI7_Handler,      SVEC_OF(EXTI7_IRQn))
DEFAULT_TRAMP(ADC_Handler,        SVEC_OF(ADC_IRQn))
DEFAULT_TRAMP(TIMER0_Handler,     SVEC_OF(TIM0_IRQn))
DEFAULT_TRAMP(TIMER1_Handler,     SVEC_OF(TIM1_IRQn))
DEFAULT_TRAMP(TIMER2_Handler,     SVEC_OF(TIM2_IRQn))
DEFAULT_TRAMP(UART_Handler,       SVEC_OF(UART_IRQn))
DEFAULT_TRAMP(I2C_Handler,        SVEC_OF(I2C_IRQn))
DEFAULT_TRAMP(SPI_Handler,        SVEC_OF(SPI_IRQn))

/* ============== 校验 + 跳转 ============== */

BOOL bl_app_meta_valid(void)
{
    const app_meta_t *m = (const app_meta_t *)FLASH_META_BASE;

    if (m->magic != APP_META_MAGIC) return FALSE;
    if (m->app_size < 0x100 || m->app_size > FLASH_APP_SIZE) return FALSE;
    if (m->app_size & 0x3U) return FALSE;

    uint32_t sp  = *(volatile uint32_t *)(FLASH_APP_BASE + 0);
    uint32_t pc  = *(volatile uint32_t *)(FLASH_APP_BASE + 4);
    if (sp < 0x20000000UL || sp > 0x20001000UL) return FALSE;
    if (pc < FLASH_APP_BASE || pc > FLASH_APP_END) return FALSE;
    if ((pc & 0x1U) == 0) return FALSE;   /* Thumb bit */

    /* Flash 硬件 CRC 校验 */
    uint16_t got = bflash_crc16(FLASH_APP_BASE, FLASH_APP_BASE + m->app_size - 1U);
    if (got != m->crc16) return FALSE;
    return TRUE;
}

/* CM0 上没有 VTOR：App 中断仍走 BL 向量表，由我们的跳板转发。
   跳转时只需把 App 的 SP / PC 设好。 */
typedef void (*app_entry_t)(void);

void bl_jump_to_app(void)
{
    /* 1) 关中断、关 SysTick，避免跳过去前还在 1ms 抖动 */
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 2) 清掉所有 NVIC pending（BL 阶段可能开过 UART/Systick）*/
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[0] = 0xFFFFFFFFU;
        NVIC->ICPR[0] = 0xFFFFFFFFU;
    }

    /* 3) 软向量表清零，App 自己会逐个注册 */
    for (uint32_t i = 0; i < SOFT_VECTOR_COUNT; i++) g_soft_vec[i] = 0;

    /* 4) 取 App 的 SP/PC 并跳 */
    uint32_t app_sp = *(volatile uint32_t *)(FLASH_APP_BASE + 0);
    uint32_t app_pc = *(volatile uint32_t *)(FLASH_APP_BASE + 4);
    app_entry_t entry = (app_entry_t)app_pc;

    __set_MSP(app_sp);
    __enable_irq();
    entry();
    for (;;) { }    /* 不会到 */
}
