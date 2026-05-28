/*
 * eeprom_protocol.h
 *
 * 24Cxx-class I2C EEPROM protocol model + sizing + special-region map.
 *
 * The host sees a 16 KB EEPROM at I2C bus address 0x50 with two-byte
 * (16-bit) word-address. That matches 24LC128 ish (1Mbit) or smaller.
 * Internally we keep:
 *
 *   bytes 0x0000 .. 0x3EFF   plain backing (16 KB - 256 B - 4 B)
 *   bytes 0x3F00 .. 0x3FFB   encrypted region (256 B - 4 B = 252 B)
 *                              -- write/read transparently via AES-128-CTR;
 *                              -- key derived from chip Die ID; never leaves chip.
 *   bytes 0x3FFC .. 0x3FFF   monotonic counter (4 B, big-endian)
 *                              -- read: returns current counter value
 *                              -- write: payload ignored, counter += 1
 *
 * The split is documented for the user; the host's EEPROM driver does
 * not need to know -- writes/reads still look like normal I2C EEPROM
 * accesses. The special semantics kick in transparently per address.
 *
 * NB: 24Cxx page sizes vary by part (8/16/32/64 bytes). We pick 64 B
 * which is the largest commonly seen, so almost any host driver works.
 */
#ifndef APP_I2CEEPROM_EEPROM_PROTOCOL_H
#define APP_I2CEEPROM_EEPROM_PROTOCOL_H

#include <stdint.h>

/* -------- bus identity -------- */
#define EE_BUS_ADDR_7BIT          0x50U          /* same as 24LC256 default */

/* -------- sizing -------- */
#define EE_TOTAL_SIZE             (16U * 1024U)  /* 16 KB host-visible */
#define EE_PAGE_SIZE              64U            /* matches 24LC128/256 */
#define EE_ADDR_MASK              (EE_TOTAL_SIZE - 1U)
#define EE_ADDR_BYTES             2U             /* 16-bit word address */

/* -------- special regions -------- */
/* Encrypted region: last full page minus the counter at the very end. */
#define EE_ENC_REGION_SIZE        252U
#define EE_ENC_REGION_BASE        (EE_TOTAL_SIZE - 256U)        /* 0x3F00 */
#define EE_ENC_REGION_END         (EE_ENC_REGION_BASE + EE_ENC_REGION_SIZE) /* 0x3FFC */

/* Monotonic counter: 4 bytes at the very top, big-endian. */
#define EE_COUNTER_SIZE           4U
#define EE_COUNTER_BASE           (EE_TOTAL_SIZE - EE_COUNTER_SIZE)         /* 0x3FFC */
#define EE_COUNTER_END            EE_TOTAL_SIZE                              /* 0x4000 */

/* Cheap predicates -- inlined where used so we keep the hot path tight. */
static __inline int ee_addr_is_counter(uint16_t a)
{
    return (a >= EE_COUNTER_BASE) && (a < EE_COUNTER_END);
}
static __inline int ee_addr_is_encrypted(uint16_t a)
{
    return (a >= EE_ENC_REGION_BASE) && (a < EE_ENC_REGION_END);
}

/* -------- timing limits the host must respect (informational) -------- */
/* Real 24LC256 page-write tWC = 5 ms typ / 10 ms max. We mirror that
 * by stretching SCL while the internal Flash sector erase + program
 * runs. Quoted here so the user can size their wait loops. */
#define EE_TWC_MS                 10U

#endif /* APP_I2CEEPROM_EEPROM_PROTOCOL_H */
