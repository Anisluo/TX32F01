/*
 * eeprom_storage.c -- see eeprom_storage.h
 *
 * Design notes that matter for correctness:
 *
 *   - The internal Flash sector is 512 B. We work at the host's
 *     64-B page granularity, so a single page program touches at
 *     most TWO internal sectors (if it straddles a boundary). To
 *     keep the code simple we always do read-modify-write at
 *     internal-sector granularity: snapshot the whole sector into
 *     SRAM, modify the relevant bytes, erase the sector, write back.
 *   - That's expensive (~10 ms per sector erase) but absolutely
 *     correct under any partial-page write pattern. Real 24LC256
 *     does ~5-10 ms anyway.
 *   - 512 B + handful of bytes overhead is tight on 4 KB SRAM but
 *     fits comfortably (~25%). Stack usage stays under 600 B.
 *
 *   - Encryption nonce: nonce[14..15] = block index within encrypted
 *     region. That makes each 16-byte block of plaintext deterministic
 *     (same address -> same keystream). We don't need IND-CPA security;
 *     the goal is "Flash dump is unreadable without the key".
 *
 *   - Counter: stored as 4 BE bytes inside the same internal sector
 *     that holds host bytes 0x3F00..0x3FFF. We treat its byte slot
 *     specially in the read/write paths but otherwise it lives in
 *     the same Flash region.
 */
#include "eeprom_storage.h"
#include "eeprom_crypto.h"
#include "TX32F01_periph.h"

#define INT_SECTOR_SIZE      512U
#define INT_SECTOR_MASK      (INT_SECTOR_SIZE - 1U)

/* ------------------------------------------------------------------ */
/*  Key derivation                                                     */
/* ------------------------------------------------------------------ */
/* Die ID lives in FLASH NVR registers DIEID0..DIEID3 (32-bit each).
 * Anyone who hasn't read this MCU sees a different "encrypted blob"
 * than what's stored on its Flash. */
static aes128_ctx_t s_aes;

static void mix_to_key(uint8_t key[16])
{
    /* Pull Die ID. FLASH is a peripheral pointer macro defined in the
     * vendor header (Device/TX32F01/Include/TX32F01.h). */
    uint32_t id0 = FLASH->DIEID0;
    uint32_t id1 = FLASH->DIEID1;
    uint32_t id2 = FLASH->DIEID2;
    uint32_t id3 = FLASH->DIEID3;

    /* Domain literal so this key is per-firmware. Even with the same
     * Die ID, a different firmware build gets a different key. */
    static const char dom[16] = "TX32F01:I2CEE:01";

    uint32_t mix[4] = { id0 ^ 0xA5A5A5A5u,
                        id1 ^ 0x5A5A5A5Au,
                        id2 ^ 0xDEADBEEFu,
                        id3 ^ 0xCAFEBABEu };

    /* Three rounds of xor + 32-bit rotate -- cheap mixer, not crypto-
     * grade, but the only attacker we care about is "someone with the
     * Flash dump and no MCU". */
    for (int r = 0; r < 3; r++) {
        for (int i = 0; i < 4; i++) {
            uint32_t v = mix[i] ^ mix[(i + 1) & 3];
            v = (v << 7) | (v >> 25);
            mix[i] = v ^ (uint32_t)dom[(i << 2) + r];
        }
    }

    /* Write out as 16 big-endian bytes. */
    for (int i = 0; i < 4; i++) {
        key[(i << 2) + 0] = (uint8_t)(mix[i] >> 24);
        key[(i << 2) + 1] = (uint8_t)(mix[i] >> 16);
        key[(i << 2) + 2] = (uint8_t)(mix[i] >> 8);
        key[(i << 2) + 3] = (uint8_t)(mix[i]);
    }
}

void ee_storage_init(void)
{
    SCU_Unlock();
    Flash_SCU24MHz_ClkCfg();
    SCU_Lock();

    uint8_t key[16];
    mix_to_key(key);
    aes128_keyexp(&s_aes, key);
}

/* ------------------------------------------------------------------ */
/*  Nonce builder for the encrypted region                            */
/* ------------------------------------------------------------------ */
/* The encrypted region is 252 B, but we encrypt at 16-B AES-CTR block
 * granularity. To get deterministic keystream per byte, the nonce is:
 *
 *   nonce[0..7]   = literal "EEPROM01"   (region tag)
 *   nonce[8..13]  = 0
 *   nonce[14..15] = block index BE (== (addr_within_region) / 16)
 *
 * For a byte read at addr A in the encrypted region:
 *   region_off  = A - EE_ENC_REGION_BASE
 *   block_idx   = region_off / 16
 *   byte_in_blk = region_off % 16
 *   stream byte = E_K(nonce(block_idx))[byte_in_blk]
 */
static void build_nonce(uint8_t n[16], uint16_t block_idx)
{
    static const uint8_t tag[8] = { 'E','E','P','R','O','M','0','1' };
    for (int i = 0; i < 8; i++) n[i] = tag[i];
    for (int i = 8; i < 14; i++) n[i] = 0;
    n[14] = (uint8_t)(block_idx >> 8);
    n[15] = (uint8_t)block_idx;
}

/* Stream `len` bytes through XOR with the keystream for the encrypted
 * region. Same call encrypts or decrypts (XOR is symmetric). */
static void enc_xor(uint16_t region_off, uint8_t *buf, uint32_t len)
{
    while (len) {
        uint16_t block_idx  = (uint16_t)(region_off >> 4);
        uint32_t byte_in    = region_off & 0xFu;
        uint32_t avail      = AES_BLOCKLEN - byte_in;
        uint32_t chunk      = (len < avail) ? len : avail;

        uint8_t ks[AES_BLOCKLEN];
        build_nonce(ks, block_idx);
        aes128_encrypt_block(&s_aes, ks);

        for (uint32_t i = 0; i < chunk; i++) buf[i] ^= ks[byte_in + i];

        buf        += chunk;
        len        -= chunk;
        region_off  = (uint16_t)(region_off + chunk);
    }
}

/* ------------------------------------------------------------------ */
/*  Direct Flash read (raw, no transformation)                        */
/* ------------------------------------------------------------------ */
static uint8_t flash_read_raw(uint16_t addr)
{
    return *(volatile uint8_t *)(EE_BACKING_BASE + (addr & EE_ADDR_MASK));
}

/* ------------------------------------------------------------------ */
/*  Public read: applies region-specific transformation               */
/* ------------------------------------------------------------------ */
void ee_storage_read(uint16_t addr, uint8_t *dst, uint32_t len)
{
    while (len--) {
        uint16_t a = addr & EE_ADDR_MASK;

        if (ee_addr_is_counter(a)) {
            /* counter region: synthesize 4 BE bytes from the stored value */
            uint32_t v   = ee_storage_counter_get();
            uint16_t off = a - EE_COUNTER_BASE;
            *dst = (uint8_t)(v >> ((3 - off) * 8));
        }
        else if (ee_addr_is_encrypted(a)) {
            uint8_t cipher = flash_read_raw(a);
            *dst = cipher;
            enc_xor((uint16_t)(a - EE_ENC_REGION_BASE), dst, 1);
        }
        else {
            *dst = flash_read_raw(a);
        }
        ++dst;
        ++addr;
    }
}

/* ------------------------------------------------------------------ */
/*  Counter implementation                                            */
/* ------------------------------------------------------------------ */
/* Counter lives in the LAST 4 bytes of the host region as a raw 32-bit
 * BE value -- on cold boot we may see 0xFFFFFFFF, which we interpret
 * as "uninitialized" and treat as 0. The bump path rewrites the entire
 * containing sector. */
static uint32_t counter_cache = 0xFFFFFFFFu;
static int      counter_cache_valid = 0;

uint32_t ee_storage_counter_get(void)
{
    if (!counter_cache_valid) {
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            v = (v << 8) | flash_read_raw((uint16_t)(EE_COUNTER_BASE + i));
        }
        if (v == 0xFFFFFFFFu) v = 0;
        counter_cache       = v;
        counter_cache_valid = 1;
    }
    return counter_cache;
}

/* ------------------------------------------------------------------ */
/*  Sector-erase + program-with-backup                                */
/* ------------------------------------------------------------------ */
/* Erase one 512-B internal sector and write back `buf` (which holds the
 * NEW content of that sector). Returns 0 on success.
 *
 * Words are written one at a time; we skip a word if its post-image is
 * equal to 0xFFFFFFFF (erased state -- saves one Flash write). */
static int rewrite_sector(uint32_t sector_phys, const uint32_t *buf)
{
    SCU_Unlock();
    Flash_Unlock();
    Flash_Main_WriteEease_Enable();

    if (!Flash_EraseSector(sector_phys)) { SCU_Lock(); return -1; }

    for (uint32_t i = 0; i < INT_SECTOR_SIZE / 4U; i++) {
        if (buf[i] != 0xFFFFFFFFu) {
            if (!FLASH_ProgramOneWord(sector_phys + i * 4U, buf[i])) {
                SCU_Lock(); return -1;
            }
        }
    }
    SCU_Lock();
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Public page-program                                               */
/* ------------------------------------------------------------------ */
/* The host's "page write" can span up to one 64-B page but may not
 * cross it. We respect that by wrapping inside the page, then for
 * each internal 512-B sector touched we do read-modify-erase-write. */
int ee_storage_page_program(uint16_t addr, const uint8_t *src, uint32_t len)
{
    /* First: apply special-region effects to the incoming buffer.
     * We need a mutable copy because encryption transforms it. */
    uint8_t tmp[EE_PAGE_SIZE];
    if (len > EE_PAGE_SIZE) len = EE_PAGE_SIZE;
    for (uint32_t i = 0; i < len; i++) tmp[i] = src[i];

    int counter_bump = 0;

    /* Step through each byte applying region semantics. */
    for (uint32_t i = 0; i < len; i++) {
        uint16_t a = (uint16_t)((addr & ~(EE_PAGE_SIZE - 1U)) |
                                ((addr + i) & (EE_PAGE_SIZE - 1U)));

        if (ee_addr_is_counter(a)) {
            counter_bump = 1;
            /* counter is rebuilt by bump(); zap the byte so the
             * sector write below doesn't fight it. */
            tmp[i] = 0xFFU;
        }
        else if (ee_addr_is_encrypted(a)) {
            uint16_t region_off = (uint16_t)(a - EE_ENC_REGION_BASE);
            enc_xor(region_off, &tmp[i], 1);
        }
    }

    /* Now write tmp[] into the backing. We do it sector-at-a-time. */
    uint16_t cur = addr;
    uint32_t left = len;
    while (left) {
        /* compute current page-wrapped host address */
        uint16_t page_base = addr & ~(EE_PAGE_SIZE - 1U);
        uint16_t page_off  = (addr + (len - left)) & (EE_PAGE_SIZE - 1U);
        uint16_t host_a    = page_base | page_off;
        uint32_t sec_off   = host_a & ~INT_SECTOR_MASK;
        uint32_t sec_phys  = EE_BACKING_BASE + sec_off;

        /* snapshot whole sector */
        uint32_t buf[INT_SECTOR_SIZE / 4U];
        for (uint32_t w = 0; w < INT_SECTOR_SIZE / 4U; w++) {
            buf[w] = *(volatile uint32_t *)(sec_phys + w * 4U);
        }
        uint8_t *bytes = (uint8_t *)buf;

        /* overlay our bytes for the portion that falls inside this sector */
        while (left) {
            uint16_t cur_host = (page_base | ((addr + (len - left)) & (EE_PAGE_SIZE - 1U)));
            uint32_t cur_sec  = cur_host & ~INT_SECTOR_MASK;
            if (cur_sec != sec_off) break;
            bytes[cur_host & INT_SECTOR_MASK] = tmp[len - left];
            left--;
            if (left == 0) break;
        }

        if (rewrite_sector(sec_phys, buf) != 0) return -1;
    }

    if (counter_bump) ee_storage_counter_bump();
    (void)cur;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Counter bump                                                      */
/* ------------------------------------------------------------------ */
void ee_storage_counter_bump(void)
{
    uint32_t v = ee_storage_counter_get() + 1U;

    /* Read the whole containing sector, patch the 4 counter bytes,
     * rewrite the sector. */
    uint32_t sec_off   = EE_COUNTER_BASE & ~INT_SECTOR_MASK;
    uint32_t sec_phys  = EE_BACKING_BASE + sec_off;
    uint32_t buf[INT_SECTOR_SIZE / 4U];

    for (uint32_t w = 0; w < INT_SECTOR_SIZE / 4U; w++) {
        buf[w] = *(volatile uint32_t *)(sec_phys + w * 4U);
    }

    uint8_t *bytes = (uint8_t *)buf;
    uint32_t off_in_sec = EE_COUNTER_BASE & INT_SECTOR_MASK;
    bytes[off_in_sec + 0] = (uint8_t)(v >> 24);
    bytes[off_in_sec + 1] = (uint8_t)(v >> 16);
    bytes[off_in_sec + 2] = (uint8_t)(v >> 8);
    bytes[off_in_sec + 3] = (uint8_t)v;

    if (rewrite_sector(sec_phys, buf) == 0) {
        counter_cache       = v;
        counter_cache_valid = 1;
    }
}
