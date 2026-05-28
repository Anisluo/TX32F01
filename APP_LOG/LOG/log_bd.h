/*
 * log_bd.h — Thin block-device wrapper over spi_nor.
 *
 *   - Addresses by (sector, page) instead of raw bytes.
 *   - Owns the "is an async erase still pending?" bit so callers don't
 *     have to remember to poll spi_nor_busy() before every op.
 */
#ifndef _LOG_BD_H
#define _LOG_BD_H

#include <stdint.h>

#define LOG_BD_SECTOR_SIZE      4096U
#define LOG_BD_PAGE_SIZE        256U
#define LOG_BD_PAGES_PER_SECTOR (LOG_BD_SECTOR_SIZE / LOG_BD_PAGE_SIZE)

/* Returns 0 on success, -1 if JEDEC unknown / SPI dead. */
int      log_bd_init(void);

uint32_t log_bd_jedec(void);
uint32_t log_bd_sector_count(void);     /* probed from JEDEC */

void     log_bd_read(uint32_t sector, uint32_t offset, void *buf, uint32_t len);

/* All write-side ops return 0 on accepted, -1 if device still busy. */
int      log_bd_program_page (uint32_t sector, uint32_t page, const void *buf, uint32_t len);
int      log_bd_erase_sector (uint32_t sector);

uint8_t  log_bd_busy(void);

#endif
