/*
 * spinor_protocol.h
 *
 * Command codes, status-register bits, sizing, and JEDEC ID for the
 * emulated W25Qxx-compatible SPI NOR device.
 *
 * Why this file exists: keeping all the host-visible protocol constants
 * in one place separates "what the protocol says" from "how we
 * implement it". When we eventually expand the emulator to advertise a
 * different size or vendor ID, only this header changes.
 *
 * Reference: Winbond W25Q16JV datasheet, sections 7.1 (commands) and
 * 7.2 (status register). The W25X series is functionally a subset.
 */
#ifndef APP_SPINOR_SPINOR_PROTOCOL_H
#define APP_SPINOR_SPINOR_PROTOCOL_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Device sizing                                                     */
/* ------------------------------------------------------------------ */
/* We back 24 KB of the host-visible address space with internal Flash.
 * Reads beyond return 0xFF (== erased state). The host typically only
 * relies on JEDEC ID to know what it's talking to, so we report a size
 * that matches our actual backing rather than lying. */
#define SNF_TOTAL_SIZE            (24U * 1024U)             /* 24 KB */
#define SNF_PAGE_SIZE             256U                       /* fixed by SPI NOR convention */
#define SNF_SECTOR_SIZE           (4U  * 1024U)              /* 4 KB sector */
#define SNF_BLOCK32_SIZE          (32U * 1024U)              /* 32 KB block (we have less, treated as chip erase) */
#define SNF_BLOCK64_SIZE          (64U * 1024U)              /* 64 KB block (same as above) */
#define SNF_NUM_PAGES             (SNF_TOTAL_SIZE / SNF_PAGE_SIZE)     /* 96 pages */
#define SNF_NUM_SECTORS           (SNF_TOTAL_SIZE / SNF_SECTOR_SIZE)   /* 6 sectors */
#define SNF_ADDR_MASK             (SNF_TOTAL_SIZE - 1U)

/* ------------------------------------------------------------------ */
/*  JEDEC identification                                              */
/* ------------------------------------------------------------------ */
/* 3-byte JEDEC ID reported by command 0x9F.
 *   [0] manufacturer  -> 0xEF (Winbond), the most widely recognised
 *   [1] memory type   -> 0x40 (W25Q-class)
 *   [2] capacity      -> log2(bytes); 0x0E -> 16 KB, 0x0F -> 32 KB.
 *                        We carry 24 KB but report 0x0F so hosts that
 *                        compute sectors = 1<<(cap-12) get 8 sectors
 *                        and may probe a couple past our backing —
 *                        which we serve as 0xFF.
 */
#define SNF_JEDEC_MFR             0xEFU
#define SNF_JEDEC_TYPE            0x40U
#define SNF_JEDEC_CAPACITY        0x0FU
#define SNF_LEGACY_MFR            0xEFU       /* cmd 0x90/0xAB returns this */
#define SNF_LEGACY_DEVICE_ID      0x13U       /* legacy device ID byte */
#define SNF_UNIQUE_ID_LEN         8U          /* cmd 0x4B (8 bytes); we synthesise this from chip ID */

/* ------------------------------------------------------------------ */
/*  Command opcodes (W25Q-compatible subset)                          */
/* ------------------------------------------------------------------ */
typedef enum {
    /* ---- write enable / disable ---- */
    SNF_CMD_WRITE_ENABLE      = 0x06,
    SNF_CMD_WRITE_DISABLE     = 0x04,

    /* ---- status register ---- */
    SNF_CMD_READ_STATUS_1     = 0x05,
    SNF_CMD_READ_STATUS_2     = 0x35,
    SNF_CMD_READ_STATUS_3     = 0x15,
    SNF_CMD_WRITE_STATUS_1    = 0x01,    /* writes byte 1, optionally byte 2 */
    SNF_CMD_WRITE_STATUS_2    = 0x31,
    SNF_CMD_WRITE_STATUS_3    = 0x11,

    /* ---- read ---- */
    SNF_CMD_READ              = 0x03,    /* 3-byte addr, then N data */
    SNF_CMD_FAST_READ         = 0x0B,    /* 3-byte addr + 1 dummy, then N data */

    /* ---- program ---- */
    SNF_CMD_PAGE_PROGRAM      = 0x02,    /* 3-byte addr + 1..256 data bytes */

    /* ---- erase ---- */
    SNF_CMD_SECTOR_ERASE      = 0x20,    /* 4 KB; 3-byte addr */
    SNF_CMD_BLOCK_ERASE_32K   = 0x52,    /* 32 KB; 3-byte addr */
    SNF_CMD_BLOCK_ERASE_64K   = 0xD8,    /* 64 KB; 3-byte addr */
    SNF_CMD_CHIP_ERASE        = 0xC7,    /* alternate is 0x60 */
    SNF_CMD_CHIP_ERASE_ALT    = 0x60,

    /* ---- identification ---- */
    SNF_CMD_JEDEC_ID          = 0x9F,    /* returns 3 bytes */
    SNF_CMD_READ_MFR_DEV      = 0x90,    /* 3-byte addr (=0x000000), returns mfr+devid */
    SNF_CMD_READ_UNIQUE_ID    = 0x4B,    /* 4 dummy, then 8 bytes */
    SNF_CMD_RELEASE_PWRDN_ID  = 0xAB,    /* 3 dummy bytes, then 1 ID byte */

    /* ---- power / misc ---- */
    SNF_CMD_POWER_DOWN        = 0xB9,
    SNF_CMD_ENABLE_RESET      = 0x66,
    SNF_CMD_RESET             = 0x99
} snf_cmd_t;

/* ------------------------------------------------------------------ */
/*  Status register 1 layout                                          */
/* ------------------------------------------------------------------ */
/* Most W25Q-class hosts only look at bit 0 (BUSY) and bit 1 (WEL).
 * The block-protect bits (BP/TB/SEC) are kept persistent across power
 * cycles by storing them at the end of our backing region in a tiny
 * "config" sector that the host can't address.
 */
#define SNF_SR1_BUSY              (1U << 0)     /* 1 = program/erase in progress */
#define SNF_SR1_WEL               (1U << 1)     /* write enable latch */
#define SNF_SR1_BP0               (1U << 2)
#define SNF_SR1_BP1               (1U << 3)
#define SNF_SR1_BP2               (1U << 4)
#define SNF_SR1_TB                (1U << 5)     /* top/bottom protect */
#define SNF_SR1_SEC               (1U << 6)     /* sector/block protect */
#define SNF_SR1_SRP0              (1U << 7)     /* status register protect */

#define SNF_SR1_PERSISTENT_MASK   (SNF_SR1_BP0 | SNF_SR1_BP1 | SNF_SR1_BP2 | \
                                   SNF_SR1_TB  | SNF_SR1_SEC | SNF_SR1_SRP0)

#endif /* APP_SPINOR_SPINOR_PROTOCOL_H */
