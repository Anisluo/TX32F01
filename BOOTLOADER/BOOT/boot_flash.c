#include "boot_flash.h"

/* 工程在 24 MHz 跑，按 HAL 注释一次性写入 24MHz 时序魔数。
   注意：Flash_SCU24MHz_ClkCfg() 同时会做 Flash_Unlock，但不会调
   Flash_Main_WriteEease_Enable()，要单独再调。*/
void bflash_prepare(void)
{
    SCU_Unlock();
    Flash_SCU24MHz_ClkCfg();        /* 内部已调 PKEYR = 0xBADBEE */
    Flash_Unlock();                 /* 冗余保险 */
    Flash_Main_WriteEease_Enable(); /* PRGKEYR */
}

/* 只允许在 APP/META/FLAG 范围内擦，禁止误擦 BL 自己或 NVR */
static BOOL addr_in_app_region(uint32_t a, uint32_t len)
{
    if (len == 0) return FALSE;
    if (a < FLASH_APP_BASE) return FALSE;
    if (a + len - 1U > FLASH_TOTAL_END) return FALSE;
    return TRUE;
}

BOOL bflash_erase_app_range(uint32_t addr, uint32_t len)
{
    if (!addr_in_app_region(addr, len)) return FALSE;
    /* 对齐到扇区下边界 */
    uint32_t s = addr & ~(FLASH_SECTOR_SZ - 1U);
    uint32_t e = (addr + len + FLASH_SECTOR_SZ - 1U) & ~(FLASH_SECTOR_SZ - 1U);
    while (s < e) {
        if (Flash_EraseSector(s) != TRUE) return FALSE;
        s += FLASH_SECTOR_SZ;
    }
    return TRUE;
}

/* 写一段任意长度数据。如果起始地址 / 长度 4 字节对齐，走 word 模式；
   否则全部按 byte 模式（更慢，但兼容 YMODEM 末尾不足 4 B 的情况）。*/
BOOL bflash_program(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (!addr_in_app_region(addr, len)) return FALSE;
    uint32_t i = 0;
    /* 头部不对齐字节 */
    while ((addr & 0x3U) && i < len) {
        if (FLASH_ProgramOneByte(addr, data[i]) != TRUE) return FALSE;
        addr++; i++;
    }
    /* 整字部分 */
    while (i + 4U <= len) {
        uint32_t w = (uint32_t)data[i]
                   | ((uint32_t)data[i+1] << 8)
                   | ((uint32_t)data[i+2] << 16)
                   | ((uint32_t)data[i+3] << 24);
        if (FLASH_ProgramOneWord(addr, w) != TRUE) return FALSE;
        addr += 4U; i += 4U;
    }
    /* 尾部不足 4 B */
    while (i < len) {
        if (FLASH_ProgramOneByte(addr, data[i]) != TRUE) return FALSE;
        addr++; i++;
    }
    return TRUE;
}

uint16_t bflash_crc16(uint32_t start_addr, uint32_t end_addr_inclusive)
{
    /* Flash_CRC 失败时返回 0，外面校验时通过比较 meta.crc16 也能识别 */
    return Flash_CRC(start_addr, end_addr_inclusive);
}

BOOL bflash_write_meta(const app_meta_t *m)
{
    if (Flash_EraseSector(FLASH_META_BASE) != TRUE) return FALSE;
    /* meta 结构是 12 字节，按 word 写 */
    if (FLASH_ProgramOneWord(FLASH_META_BASE + 0,  m->magic)    != TRUE) return FALSE;
    if (FLASH_ProgramOneWord(FLASH_META_BASE + 4,  m->app_size) != TRUE) return FALSE;
    uint32_t crc_word = (uint32_t)m->crc16 | ((uint32_t)m->reserved << 16);
    if (FLASH_ProgramOneWord(FLASH_META_BASE + 8,  crc_word)    != TRUE) return FALSE;
    return TRUE;
}

BOOL bflash_clear_bootflag(void)
{
    return Flash_EraseSector(FLASH_BOOTFLAG_BASE);
}

uint32_t bflash_read_bootflag(void)
{
    return *(volatile uint32_t *)FLASH_BOOTFLAG_BASE;
}
