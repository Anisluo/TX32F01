/*
 * spinor_emu.h
 *
 * Top-level emulator: turns the raw byte-stream from bsp_spi_slave
 * into responses that look bit-for-bit like a W25Qxx SPI NOR.
 *
 * Threading: the SPI ISR calls snf_emu_byte() once per host-clocked
 * byte. The CS rising-edge IRQ calls snf_emu_frame_end(). Both are
 * O(1) and IRQ-safe. Long-running operations (page program, sector
 * erase, chip erase) are deferred to the main loop via internal
 * pending flags. The main loop calls snf_emu_tick() once per
 * iteration to drain those.
 *
 *   ISR path                     main loop
 *   --------                     ---------
 *   byte arrives  ----+
 *                     |
 *                     v
 *                 [state machine]
 *                  - JEDEC ID  -> immediate response
 *                  - READ      -> immediate stream from Flash
 *                  - PROGRAM   -> latch bytes, mark BUSY=1
 *                  - ERASE     -> mark BUSY=1
 *                     |
 *                     |  CS rising edge
 *                     v
 *                 [frame end]
 *                  - if PROGRAM with bytes pending:
 *                      set "do program" flag
 *                  - if ERASE with addr captured:
 *                      set "do erase"   flag
 *                     |
 *                     v
 *                                +--- snf_emu_tick()
 *                                |    sees flag set
 *                                |    runs Flash op (blocking, ms)
 *                                |    clears BUSY=1 when done
 *                                +
 *
 * That decoupling is what lets the host poll status while a write or
 * erase is in flight -- the SPI ISR keeps handling status-read
 * commands while the main loop is busy in Flash_EraseSector().
 */
#ifndef APP_SPINOR_SPINOR_EMU_H
#define APP_SPINOR_SPINOR_EMU_H

#include <stdint.h>

/* Boot init: configures storage, registers ISR callbacks, returns to
 * caller with the emulator ready to serve. */
void snf_emu_init(void);

/* Drain any pending deferred operations (program / erase). Call once
 * per main-loop iteration. Returns the number of operations processed
 * (useful for activity LEDs). */
unsigned snf_emu_tick(void);

/* ---- byte / frame callbacks wired to bsp_spi_slave at init time ---- */
uint8_t snf_emu_byte(uint8_t rx);
void    snf_emu_frame_end(void);

/* ---- introspection (used by main.c for the heartbeat printout) ---- */
uint32_t snf_emu_total_bytes_rx(void);
uint32_t snf_emu_total_cmds(void);
uint32_t snf_emu_total_programs(void);
uint32_t snf_emu_total_erases(void);
uint8_t  snf_emu_status_reg1(void);

#endif /* APP_SPINOR_SPINOR_EMU_H */
