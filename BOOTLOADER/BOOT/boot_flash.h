#ifndef _BOOT_FLASH_H
#define _BOOT_FLASH_H

#include "boot_layout.h"

void  bflash_prepare(void);                                   /* 解锁 + 24MHz 配置 */
BOOL  bflash_erase_app_range(uint32_t addr, uint32_t len);    /* 仅允许 APP/META/FLAG 区 */
BOOL  bflash_program(uint32_t addr, const uint8_t *data, uint32_t len);
uint16_t bflash_crc16(uint32_t start_addr, uint32_t end_addr_inclusive);

BOOL  bflash_write_meta(const app_meta_t *m);
BOOL  bflash_clear_bootflag(void);
uint32_t bflash_read_bootflag(void);

#endif
