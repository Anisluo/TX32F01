/*
 * spinor_storage.c -- see spinor_storage.h
 */
#include "spinor_storage.h"
#include "TX32F01_periph.h"

/* The HAL Flash driver wants addresses inside the chip's flash window
 * (0x0100xxxx). All host-supplied addresses arrive 0-based, so we just
 * add SNF_BACKING_BASE before any HAL call. */
static inline uint32_t to_phys(uint32_t host_addr)
{
    return SNF_BACKING_BASE + (host_addr & SNF_ADDR_MASK);
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */
void snf_storage_init(void)
{
    /* The vendor HAL needs the flash timing recalibrated whenever the
     * core runs above 16 MHz. We're at 24 MHz, so this is required. */
    SCU_Unlock();
    Flash_SCU24MHz_ClkCfg();
    SCU_Lock();
}

/* ------------------------------------------------------------------ */
/*  Read                                                              */
/* ------------------------------------------------------------------ */
void snf_storage_read(uint32_t addr, uint8_t *dst, uint32_t len)
{
    /* Flash is memory-mapped on this part — read is just a pointer
     * dereference. Loop manually instead of memcpy() to stay clear of
     * any library dependency that would balloon code size. */
    while (len--) {
        uint32_t off = addr & SNF_ADDR_MASK;
        if (off < SNF_TOTAL_SIZE) {
            *dst = *(volatile uint8_t *)(SNF_BACKING_BASE + off);
        } else {
            *dst = 0xFFU;
        }
        ++dst;
        ++addr;
    }
}

uint8_t snf_storage_read_byte(uint32_t addr)
{
    uint32_t off = addr & SNF_ADDR_MASK;
    if (off >= SNF_TOTAL_SIZE) return 0xFFU;
    return *(volatile uint8_t *)(SNF_BACKING_BASE + off);
}

/* ------------------------------------------------------------------ */
/*  Program                                                           */
/* ------------------------------------------------------------------ */
/* Real SPI NOR programs 1-byte at a time at the bit level (1 -> 0 only).
 * The HAL writes one 32-bit word at a time, and only to an erased
 * word. We work around both:
 *
 *   - For each destination 4-byte word, we read current contents,
 *     overlay the bytes the caller wants to write at their byte
 *     positions (preserving the others), and only write if the new
 *     value differs from current AND the write is valid (each bit must
 *     go from 1 -> 0 or stay; a bit going from 0 -> 1 would need an
 *     erase, which the host hasn't issued). If invalid, we silently
 *     program what bits we can and leave 0->1 transitions for the
 *     host to discover via a verify read — same behaviour as a real
 *     NOR that just refuses to flip 0 -> 1 without an erase.
 */
static int program_word_aligned(uint32_t phys_addr, uint32_t word_off,
                                const uint8_t *src, uint32_t nbytes)
{
    uint32_t cur = *(volatile uint32_t *)(phys_addr - word_off);
    uint32_t neu = cur;

    for (uint32_t i = 0; i < nbytes; i++) {
        uint32_t shift = (word_off + i) * 8U;
        uint8_t  s     = src[i];
        uint8_t  c     = (uint8_t)(cur >> shift);
        /* AND models a NOR program: bits can drop from 1 to 0 only. */
        uint8_t  p     = c & s;
        neu = (neu & ~(0xFFu << shift)) | ((uint32_t)p << shift);
    }

    if (neu == cur) return 0;   /* no bit needs to change */

    SCU_Unlock();
    Flash_Unlock();
    Flash_Main_WriteEease_Enable();
    int ok = (int)FLASH_ProgramOneWord(phys_addr - word_off, neu);
    SCU_Lock();
    return ok ? 0 : -1;
}

int snf_storage_program(uint32_t addr, const uint8_t *src, uint32_t len)
{
    while (len) {
        uint32_t off       = addr & SNF_ADDR_MASK;
        if (off >= SNF_TOTAL_SIZE) {
            /* address past end of emulated chip: real NOR wraps inside
             * the page; we just drop the byte silently here. */
            addr++;
            src++;
            len--;
            continue;
        }
        uint32_t phys      = SNF_BACKING_BASE + off;
        uint32_t word_off  = phys & 0x3U;
        uint32_t chunk     = 4U - word_off;
        if (chunk > len) chunk = len;

        if (program_word_aligned(phys, word_off, src, chunk) != 0) return -1;

        addr += chunk;
        src  += chunk;
        len  -= chunk;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Erase                                                             */
/* ------------------------------------------------------------------ */
/* Host-visible sector is 4 KB == 8 internal 512-B sectors. */
#define INTERNAL_SECTORS_PER_EMU_SECTOR  (SNF_SECTOR_SIZE / 512U)

int snf_storage_erase_4k(uint32_t addr)
{
    uint32_t sector_base_off = (addr & SNF_ADDR_MASK) & ~(SNF_SECTOR_SIZE - 1U);
    if (sector_base_off >= SNF_TOTAL_SIZE) return 0;        /* out-of-range erase is a no-op */

    uint32_t phys_base = SNF_BACKING_BASE + sector_base_off;

    SCU_Unlock();
    Flash_Unlock();
    Flash_Main_WriteEease_Enable();

    for (uint32_t i = 0; i < INTERNAL_SECTORS_PER_EMU_SECTOR; i++) {
        if (!Flash_EraseSector(phys_base + i * 512U)) {
            SCU_Lock();
            return -1;
        }
    }

    SCU_Lock();
    return 0;
}

int snf_storage_chip_erase(void)
{
    for (uint32_t s = 0; s < SNF_NUM_SECTORS; s++) {
        if (snf_storage_erase_4k(s * SNF_SECTOR_SIZE) != 0) return -1;
    }
    return 0;
}
