/*
 * eeprom_emu.h
 *
 * Glue layer between bsp_i2c_slave callbacks and eeprom_storage.
 * Holds the address pointer that the host conceptually owns:
 *
 *   write phase:   first two bytes -> new address pointer (BE),
 *                  subsequent bytes -> page buffer, then STOP commits
 *   read phase:    next byte to send comes from storage at the current
 *                  address pointer, which auto-increments
 *
 * That is exactly how 24LC256 and friends behave.
 */
#ifndef APP_I2CEEPROM_EEPROM_EMU_H
#define APP_I2CEEPROM_EEPROM_EMU_H

#include <stdint.h>

void ee_emu_init(void);

/* Stats for the diagnostic UART line. */
uint32_t ee_emu_writes(void);
uint32_t ee_emu_reads(void);
uint32_t ee_emu_bytes(void);
uint16_t ee_emu_last_addr(void);

#endif /* APP_I2CEEPROM_EEPROM_EMU_H */
