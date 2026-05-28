/*
 * eeprom_storage.h
 *
 * Backing store for the I2C EEPROM emulator. Three regions, all
 * sharing the same 16 KB host-visible address space, distinguished
 * only by where the byte happens to live:
 *
 *   plain region          -> direct byte from internal Flash
 *   encrypted region      -> Flash byte after AES-128-CTR decrypt
 *   monotonic counter     -> last-stored counter value
 *
 * The host can't tell which is which from the bus; reads return
 * "plaintext" in all three cases. Writes to the encrypted region get
 * encrypted before Flash; writes to the counter address ignore the
 * payload and just bump the value.
 *
 *   internal Flash layout (32 KB total, 512 B sectors):
 *
 *     0x01000000 .. 0x010017FF   firmware code            (6 KB)
 *     0x01001800 .. 0x010057FF   host-visible region      (16 KB == 32 internal sectors)
 *     0x01005800 .. 0x01007FFF   reserved / future        (10 KB)
 *
 * The emulator only ever erases / programs inside the 16 KB host
 * region, so the code area is safe.
 */
#ifndef APP_I2CEEPROM_EEPROM_STORAGE_H
#define APP_I2CEEPROM_EEPROM_STORAGE_H

#include <stdint.h>
#include "eeprom_protocol.h"

#define EE_BACKING_BASE           0x01001800UL
#define EE_BACKING_END            (EE_BACKING_BASE + EE_TOTAL_SIZE)

/* One-time init: brings Flash to 24 MHz timing, derives AES key from
 * the chip Die ID, schedules the round keys. */
void ee_storage_init(void);

/* Read len bytes starting at host word-address addr. Handles the
 * three regions internally. addr is masked into the 16 KB space. */
void ee_storage_read(uint16_t addr, uint8_t *dst, uint32_t len);

/* Program a single host page (1..64 bytes within one 64-B page).
 * addr need not be page-aligned -- caller passes the absolute byte
 * address, this function honours real-EEPROM "wrap within page"
 * semantics. Returns 0 on success.
 *
 * Side effects:
 *   - if any byte falls in the encrypted region, that byte is XOR'd
 *     with AES-CTR keystream first
 *   - if any byte falls in the counter region, the counter is bumped
 *     once and the payload byte is discarded
 *
 * The whole thing runs in ~5-40 ms depending on how many internal
 * 512-B sectors get erased. Caller is responsible for holding the
 * I2C clock-stretched until this returns. */
int  ee_storage_page_program(uint16_t addr, const uint8_t *src, uint32_t len);

/* Counter helpers (the public read goes through ee_storage_read). */
uint32_t ee_storage_counter_get(void);
void     ee_storage_counter_bump(void);

#endif /* APP_I2CEEPROM_EEPROM_STORAGE_H */
