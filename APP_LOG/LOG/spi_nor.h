/*
 * spi_nor.h — minimal driver for W25Q-family SPI NOR (W25Q16 default).
 *
 * Pin map (matches vendor SPI flash demo):
 *   CS   = GPIO2.04 (manual)        CLK  = GPIO2.05 (AF)
 *   MOSI = GPIO3.00 (AF)            MISO = GPIO3.01 (AF)
 *
 * Address space is byte-addressed (24-bit). Erase granularity 4 KB.
 * Program granularity is 256 B page; programming must stay within one page.
 *
 * Key difference vs the vendor demo: the erase/program completion is
 * exposed as a poll (spi_nor_busy) instead of a busy-wait, so the
 * cooperative scheduler can tick during the 100-150 ms sector erase.
 */
#ifndef _SPI_NOR_H
#define _SPI_NOR_H

#include <stdint.h>

#define SPI_NOR_PAGE_SIZE       256U
#define SPI_NOR_SECTOR_SIZE     4096U
#define SPI_NOR_JEDEC_W25Q16    0xEF4015UL

void     spi_nor_init(void);
uint32_t spi_nor_read_jedec(void);          /* expect 0xEF4015 for W25Q16 */
void     spi_nor_wakeup(void);
void     spi_nor_powerdown(void);

/* Synchronous read — fast enough (24MHz/8 = 3MHz, ~300KB/s). */
void     spi_nor_read(uint32_t addr, void *buf, uint32_t len);

/* Page program (caller guarantees [addr, addr+len) lies in one 256B page
   AND the page was erased since last program). Returns immediately;
   poll spi_nor_busy() before the next op. */
void     spi_nor_program_page(uint32_t addr, const void *buf, uint32_t len);

/* Issue 4KB sector erase command and return. Poll spi_nor_busy(). */
void     spi_nor_erase_sector_start(uint32_t sector_addr);

/* Issue whole-chip erase and return. Typ 25-100s on W25Q16. Poll spi_nor_busy(). */
void     spi_nor_erase_chip_start(void);

/* Non-blocking status: 1 if a previous program/erase is still in flight. */
uint8_t  spi_nor_busy(void);

#endif
