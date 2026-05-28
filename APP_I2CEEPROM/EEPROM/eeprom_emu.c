/*
 * eeprom_emu.c
 *
 * Two-phase per-frame state machine. State is just a few bytes:
 *
 *   addr_high_seen  : we've consumed the first byte of the 16-bit address
 *   addr_seen       : both address bytes consumed; next bytes are data
 *   page_buf[64]    : pending writes since last STOP
 *   page_buf_len    : how many bytes are pending
 *   addr_ptr        : the internal address pointer the host has set
 *
 *  Phase A (write):
 *     SLA+W  -> reset addr_high_seen / addr_seen / page_buf_len
 *     byte 0 -> upper address byte
 *     byte 1 -> lower address byte (commit addr_ptr)
 *     byte 2..N -> latch into page_buf
 *     STOP   -> if page_buf_len > 0, do storage page_program;
 *               otherwise this was just a "set address pointer" frame
 *
 *  Phase B (read):
 *     SLA+R  -> ship storage byte @ addr_ptr++, host ACKs for more
 *     each subsequent byte does the same; address auto-increments and
 *     wraps inside the 16 KB host region.
 *
 *  The protocol layer touches Flash only at STOP, which means the
 *  slow ~5-40 ms Flash work happens between frames -- clock-stretching
 *  is implicit because the I2C peripheral stretches SCL whenever SI is
 *  pending. We further ensure no host bus contention by completing the
 *  Flash write fully before returning from the STOP callback.
 */
#include "eeprom_emu.h"
#include "eeprom_protocol.h"
#include "eeprom_storage.h"
#include "bsp_i2c_slave.h"

static struct {
    uint8_t  addr_high_seen;
    uint8_t  addr_seen;
    uint16_t addr_ptr;

    uint8_t  page_buf[EE_PAGE_SIZE];
    uint8_t  page_buf_len;

    /* stats */
    uint32_t n_writes;
    uint32_t n_reads;
    uint32_t n_bytes;
    uint16_t last_addr;
} s;

/* ------------------------------------------------------------------ */
/*  Callbacks invoked from bsp_i2c_slave ISR                          */
/* ------------------------------------------------------------------ */
static void on_start_write(void)
{
    s.addr_high_seen = 0;
    s.addr_seen      = 0;
    s.page_buf_len   = 0;
}

static int on_rx_byte(uint8_t b)
{
    s.n_bytes++;

    if (!s.addr_high_seen) {
        s.addr_ptr       = (uint16_t)b << 8;
        s.addr_high_seen = 1;
        return 0;
    }
    if (!s.addr_seen) {
        s.addr_ptr   |= b;
        s.addr_seen   = 1;
        s.last_addr   = s.addr_ptr;
        return 0;
    }

    /* data byte: latch into page buffer, wrap at page boundary */
    if (s.page_buf_len < EE_PAGE_SIZE) {
        s.page_buf[s.page_buf_len++] = b;
    } else {
        /* page wrap: overwrite within the same page */
        uint16_t wrap_off =
            (uint16_t)((s.addr_ptr + s.page_buf_len) & (EE_PAGE_SIZE - 1U));
        s.page_buf[wrap_off] = b;
    }
    return 0;
}

static uint8_t on_start_read(void)
{
    s.n_reads++;
    uint8_t out = 0xFFU;
    ee_storage_read(s.addr_ptr, &out, 1);
    s.addr_ptr++;
    return out;
}

static uint8_t on_tx_byte(void)
{
    s.n_bytes++;
    uint8_t out = 0xFFU;
    ee_storage_read(s.addr_ptr, &out, 1);
    s.addr_ptr++;
    return out;
}

static void on_stop(void)
{
    /* If we accumulated data bytes during the write phase, commit them
     * as a page program. Otherwise STOP just marks "host set address
     * pointer, will read next time". */
    if (s.addr_seen && s.page_buf_len > 0) {
        s.n_writes++;
        (void)ee_storage_page_program(s.addr_ptr, s.page_buf, s.page_buf_len);
        s.page_buf_len = 0;
    }
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */
void ee_emu_init(void)
{
    ee_storage_init();

    static const i2c_slave_cb_t cb = {
        .start_write = on_start_write,
        .rx_byte     = on_rx_byte,
        .start_read  = on_start_read,
        .tx_byte     = on_tx_byte,
        .stop        = on_stop,
    };
    bsp_i2c_slave_init(EE_BUS_ADDR_7BIT, &cb);
}

/* ------------------------------------------------------------------ */
/*  Introspection                                                     */
/* ------------------------------------------------------------------ */
uint32_t ee_emu_writes(void)    { return s.n_writes; }
uint32_t ee_emu_reads(void)     { return s.n_reads; }
uint32_t ee_emu_bytes(void)     { return s.n_bytes; }
uint16_t ee_emu_last_addr(void) { return s.last_addr; }
