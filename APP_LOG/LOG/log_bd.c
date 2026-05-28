#include "log_bd.h"
#include "spi_nor.h"

static uint32_t s_jedec;
static uint32_t s_sectors;

int log_bd_init(void)
{
    spi_nor_init();
    s_jedec = spi_nor_read_jedec();

    /* Decode capacity from JEDEC byte[0]=mfr, byte[1]=mem_type, byte[2]=cap.
       Winbond convention: cap = log2(bytes), so cap byte directly gives size. */
    uint8_t cap = (uint8_t)(s_jedec & 0xFF);
    if (cap < 0x10 || cap > 0x1A) {
        /* unknown / no device on bus — bail out, caller handles */
        s_sectors = 0;
        return -1;
    }
    s_sectors = (1UL << cap) / LOG_BD_SECTOR_SIZE;
    return 0;
}

uint32_t log_bd_jedec(void)        { return s_jedec; }
uint32_t log_bd_sector_count(void) { return s_sectors; }
uint8_t  log_bd_busy(void)         { return spi_nor_busy(); }

void log_bd_read(uint32_t sector, uint32_t offset, void *buf, uint32_t len)
{
    /* Read tolerates an erase in flight on some parts, but to keep semantics
       simple we just spin briefly. Erases are short for callers that obey
       the busy poll, so this is dead code on the happy path. */
    while (spi_nor_busy()) { }
    spi_nor_read(sector * LOG_BD_SECTOR_SIZE + offset, buf, len);
}

int log_bd_program_page(uint32_t sector, uint32_t page, const void *buf, uint32_t len)
{
    if (spi_nor_busy()) return -1;
    spi_nor_program_page(sector * LOG_BD_SECTOR_SIZE + page * LOG_BD_PAGE_SIZE, buf, len);
    return 0;
}

int log_bd_erase_sector(uint32_t sector)
{
    if (spi_nor_busy()) return -1;
    spi_nor_erase_sector_start(sector * LOG_BD_SECTOR_SIZE);
    return 0;
}
