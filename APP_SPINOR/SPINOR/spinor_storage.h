/*
 * spinor_storage.h
 *
 * Maps the host-visible 24 KB address space to a region of the chip's
 * own internal Flash, hiding the differences in sector size and write
 * semantics behind a clean read / program / erase API.
 *
 *   internal Flash layout (32 KB total, 512 B sectors):
 *
 *     0x01000000 .. 0x010017FF   code           (6 KB)
 *     0x01001800 .. 0x010077FF   emulated NOR   (24 KB, 48 internal sectors)
 *     0x01007800 .. 0x01007FFF   config / spare (2 KB)
 *
 *   host-visible NOR layout (24 KB, 4 KB sectors):
 *
 *     0x000000   .. 0x000FFF      sector 0
 *     0x001000   .. 0x001FFF      sector 1
 *     ...
 *     0x005000   .. 0x005FFF      sector 5
 *
 * One emulated 4 KB sector = 8 internal 512 B sectors. Erase is the
 * costly operation (a few ms per internal sector). The emulator
 * accumulates erase calls into a busy-flag window so the host driver
 * can poll status until done.
 *
 * Atomicity: this layer is NOT power-fail safe on its own. A program
 * call that is interrupted mid-write leaves part of the page in the
 * old state and part in the new state, just like a real SPI NOR. The
 * host's filesystem / journaling layer is expected to handle that
 * (typical for any NOR-backed storage).
 */
#ifndef APP_SPINOR_SPINOR_STORAGE_H
#define APP_SPINOR_SPINOR_STORAGE_H

#include <stdint.h>
#include "spinor_protocol.h"

/* Physical address of byte 0 of the host-visible NOR. */
#define SNF_BACKING_BASE          0x01001800UL
#define SNF_BACKING_END           (SNF_BACKING_BASE + SNF_TOTAL_SIZE)

/* ------------------------------------------------------------------ */
/*  Read                                                              */
/* ------------------------------------------------------------------ */

/* Read up to `len` bytes starting at host address `addr` into `dst`.
 * Reads past SNF_TOTAL_SIZE are served as 0xFF (matches an erased real
 * SPI NOR that wraps at its capacity). No alignment requirement. */
void snf_storage_read(uint32_t addr, uint8_t *dst, uint32_t len);

/* Read one byte; convenient for the byte-at-a-time SPI ISR path. */
uint8_t snf_storage_read_byte(uint32_t addr);

/* ------------------------------------------------------------------ */
/*  Program                                                           */
/* ------------------------------------------------------------------ */

/* Program `len` bytes starting at host address `addr` from `src`.
 *
 * Caller must:
 *   - own WEL=1 (we don't enforce; protocol layer does)
 *   - keep addr within a single 256-B page (the host's responsibility,
 *     matching real SPI NOR semantics — programs that cross a page
 *     boundary wrap inside the same page)
 *
 * Semantics: bits can only go 1 -> 0 in a program operation. Writing
 * 0xFF anywhere is a NOP. Writing the same value as currently in Flash
 * is a NOP. This matches real NOR and lets the host issue redundant
 * programs without consuming erase cycles.
 *
 * Returns 0 on success, -1 if any internal write failed. */
int snf_storage_program(uint32_t addr, const uint8_t *src, uint32_t len);

/* ------------------------------------------------------------------ */
/*  Erase                                                             */
/* ------------------------------------------------------------------ */

/* Erase one 4 KB sector containing host address `addr`. addr is masked
 * to the sector base internally. Returns 0 on success.
 *
 * Implementation: erases 8 consecutive internal 512-B sectors. Each
 * one takes ~5 ms on this part, so a 4 KB sector erase blocks for
 * ~40 ms. The protocol layer raises SR1.BUSY for that window. */
int snf_storage_erase_4k(uint32_t addr);

/* Erase the entire emulated chip (all SNF_NUM_SECTORS sectors). */
int snf_storage_chip_erase(void);

/* ------------------------------------------------------------------ */
/*  One-time init: unlock SCU, configure flash clock                  */
/* ------------------------------------------------------------------ */
void snf_storage_init(void);

#endif /* APP_SPINOR_SPINOR_STORAGE_H */
