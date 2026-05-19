#ifndef _BOOT_LAYOUT_H
#define _BOOT_LAYOUT_H

#include "TX32F01_periph.h"

/* ============== Flash 分区 ============== */
#define FLASH_BL_BASE          0x01000000UL
#define FLASH_BL_SIZE          0x00002000UL   /* 8KB bootloader */

#define FLASH_APP_BASE         0x01002000UL
#define FLASH_APP_SIZE         0x00005800UL   /* 22KB app */

#define FLASH_META_BASE        0x01007800UL
#define FLASH_META_SIZE        0x00000400UL   /* 1KB (2 sectors) */

#define FLASH_BOOTFLAG_BASE    0x01007C00UL
#define FLASH_BOOTFLAG_SIZE    0x00000400UL   /* 1KB (2 sectors) */

#define FLASH_SECTOR_SZ        512U

#define FLASH_APP_END          (FLASH_APP_BASE + FLASH_APP_SIZE - 1U)
#define FLASH_TOTAL_END        0x01007FFFUL

/* ============== Magic ============== */
#define BOOT_REQ_UPDATE        0xA55AF00DUL   /* 写入 BOOTFLAG_BASE 即停在 BL */
#define APP_META_MAGIC         0x424C4150UL   /* ASCII 'BLAP' */

typedef struct {
    uint32_t magic;
    uint32_t app_size;     /* bytes，必须 4 字节对齐 */
    uint16_t crc16;        /* Flash 硬件 CRC */
    uint16_t reserved;
} app_meta_t;

/* ============== SRAM 软向量表 ============== */
/* 18 个外部 IRQ (IWDT..SPI) + 5 个系统异常 (NMI/HF/SVC/PendSV/SysTick)。
   为对齐方便统一开 24 项。 */
#define SOFT_VECTOR_COUNT      24U
#define SOFT_VECTOR_BASE       0x20000000UL   /* SRAM 头 96 B */
#define SOFT_VECTOR_END        (SOFT_VECTOR_BASE + SOFT_VECTOR_COUNT*4U)

typedef void (*isr_t)(void);

/* 索引约定（>=0 为外设号；负数为系统异常） */
#define SVEC_NMI               0
#define SVEC_HARDFAULT         1
#define SVEC_SVC               2
#define SVEC_PENDSV            3
#define SVEC_SYSTICK           4
/* 5 预留 */
#define SVEC_IRQ_BASE          6        /* 从这里开始等于 IRQn (IWDT=0 → 6) */
#define SVEC_OF(irqn)          (SVEC_IRQ_BASE + (irqn))

#endif
