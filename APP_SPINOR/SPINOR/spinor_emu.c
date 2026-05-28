/*
 * spinor_emu.c -- see spinor_emu.h
 *
 * The state machine is intentionally flat -- one big switch on the
 * received command byte after the first byte of every frame. Inside
 * each command, we count how many bytes of the frame have come in and
 * react accordingly (address phase, dummy phase, data phase). The CS
 * rising-edge event is the only thing that ends a frame and triggers
 * deferred work.
 *
 * Memory: the only buffer this module allocates is a single 256-B
 * page-program staging area. Everything else is a handful of bytes of
 * state. Fits well within the 4 KB SRAM budget with the soft vector
 * table and stack also in there.
 */
#include "spinor_emu.h"
#include "spinor_protocol.h"
#include "spinor_storage.h"
#include "bsp_spi_slave.h"
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  State                                                             */
/* ------------------------------------------------------------------ */
typedef enum {
    STATE_IDLE = 0,
    STATE_CMD_DECODED,         /* first byte after CS-asserted received */
    STATE_ADDR1, STATE_ADDR2, STATE_ADDR3,
    STATE_DUMMY,
    STATE_READ_DATA,           /* streaming data out */
    STATE_PROGRAM_DATA,        /* latching incoming bytes */
    STATE_DONE                 /* ignore remaining bytes until CS rises */
} state_t;

/* All ISR-touched state in one struct so we can reason about access
 * patterns in one place. */
typedef struct {
    state_t   state;
    uint8_t   cmd;
    uint32_t  addr;
    uint8_t   dummy_left;

    /* page-program staging. pgm_addr is captured at the ADDR3 ->
     * PROGRAM_DATA transition because frame_end clears s_emu.addr. */
    uint8_t   pgm_buf[SNF_PAGE_SIZE];
    uint32_t  pgm_addr;
    uint16_t  pgm_len;
    uint8_t   pgm_pending;

    /* deferred erase */
    uint32_t  erase_addr;
    uint8_t   erase_pending;     /* 1=sector, 2=chip */

    /* status-register state */
    uint8_t   sr1;               /* SR1.BUSY and SR1.WEL live here */

    /* counters */
    uint32_t  total_bytes;
    uint32_t  total_cmds;
    uint32_t  total_programs;
    uint32_t  total_erases;
} emu_t;

static volatile emu_t s_emu;

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */
static inline void emu_reset_frame(void)
{
    s_emu.state      = STATE_IDLE;
    s_emu.cmd        = 0;
    s_emu.addr       = 0;
    s_emu.dummy_left = 0;
    /* keep pgm_buf / pgm_len untouched -- frame-end handler reads them */
}

static inline void set_busy(int busy)
{
    if (busy) s_emu.sr1 |=  SNF_SR1_BUSY;
    else      s_emu.sr1 &= ~SNF_SR1_BUSY;
}

static inline int wel(void)        { return (s_emu.sr1 & SNF_SR1_WEL)  != 0; }
static inline int busy(void)       { return (s_emu.sr1 & SNF_SR1_BUSY) != 0; }
static inline void set_wel(int on)
{
    if (on) s_emu.sr1 |=  SNF_SR1_WEL;
    else    s_emu.sr1 &= ~SNF_SR1_WEL;
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */
void snf_emu_init(void)
{
    snf_storage_init();

    s_emu.state          = STATE_IDLE;
    s_emu.sr1            = 0;
    s_emu.pgm_pending    = 0;
    s_emu.erase_pending  = 0;
    s_emu.total_bytes    = 0;
    s_emu.total_cmds     = 0;
    s_emu.total_programs = 0;
    s_emu.total_erases   = 0;

    bsp_spi_slave_init(snf_emu_byte, snf_emu_frame_end);
}

/* ------------------------------------------------------------------ */
/*  Byte callback -- the hot path                                     */
/* ------------------------------------------------------------------ */
/* Returns the byte to send on the *next* clocked transfer. The SPI
 * hardware has already shipped 0xFF (or whatever was in the TX FIFO)
 * by the time we get here. The return value goes into the FIFO so the
 * NEXT byte the host clocks gets a real response. */
uint8_t snf_emu_byte(uint8_t rx)
{
    s_emu.total_bytes++;

    /* The very first byte of any frame is the command opcode. */
    if (s_emu.state == STATE_IDLE) {
        s_emu.cmd        = rx;
        s_emu.total_cmds++;
        s_emu.state      = STATE_CMD_DECODED;
        s_emu.addr       = 0;
        s_emu.dummy_left = 0;
        s_emu.pgm_len    = 0;

        /* Commands that respond on the FIRST data clock after the
         * opcode need their reply pre-loaded here. */
        switch (s_emu.cmd) {
        case SNF_CMD_WRITE_ENABLE:
            set_wel(1);
            s_emu.state = STATE_DONE;
            break;
        case SNF_CMD_WRITE_DISABLE:
            set_wel(0);
            s_emu.state = STATE_DONE;
            break;
        case SNF_CMD_READ_STATUS_1:
            s_emu.state = STATE_READ_DATA;
            return s_emu.sr1;
        case SNF_CMD_READ_STATUS_2:
        case SNF_CMD_READ_STATUS_3:
            s_emu.state = STATE_READ_DATA;
            return 0x00U;
        case SNF_CMD_JEDEC_ID:
            s_emu.state = STATE_READ_DATA;
            s_emu.addr  = 0;   /* repurpose as response index */
            return SNF_JEDEC_MFR;
        case SNF_CMD_FAST_READ:
            s_emu.dummy_left = 1;
            /* falls through to address phase via state below */
            break;
        case SNF_CMD_CHIP_ERASE:
        case SNF_CMD_CHIP_ERASE_ALT:
            if (wel() && !busy()) {
                set_busy(1);
                s_emu.erase_pending = 2;
                s_emu.total_erases++;
            }
            s_emu.state = STATE_DONE;
            break;
        case SNF_CMD_RELEASE_PWRDN_ID:
            /* 3 dummy bytes then 1 device-ID byte */
            s_emu.dummy_left = 3;
            s_emu.state      = STATE_DUMMY;
            break;
        case SNF_CMD_POWER_DOWN:
        case SNF_CMD_ENABLE_RESET:
        case SNF_CMD_RESET:
            /* one-byte commands with no side effect we need to model */
            s_emu.state = STATE_DONE;
            break;
        default:
            /* anything not in the above list expects 3 address bytes
             * next, including READ / PAGE_PROGRAM / sector & block
             * erase / READ_MFR_DEV / WRITE_STATUS. We push to ADDR1. */
            break;
        }
        if (s_emu.state == STATE_CMD_DECODED) s_emu.state = STATE_ADDR1;
        return 0xFFU;
    }

    /* ---- everything from here is bytes 1..N of the frame ---- */
    switch (s_emu.state) {

    case STATE_ADDR1:
        s_emu.addr = ((uint32_t)rx) << 16;
        s_emu.state = STATE_ADDR2;
        return 0xFFU;

    case STATE_ADDR2:
        s_emu.addr |= ((uint32_t)rx) << 8;
        s_emu.state = STATE_ADDR3;
        return 0xFFU;

    case STATE_ADDR3:
        s_emu.addr |= rx;
        /* Address complete. Branch by command. */
        switch (s_emu.cmd) {
        case SNF_CMD_READ:
            s_emu.state = STATE_READ_DATA;
            return snf_storage_read_byte(s_emu.addr);

        case SNF_CMD_FAST_READ:
            /* one dummy byte THEN data */
            s_emu.state      = STATE_DUMMY;
            s_emu.dummy_left = 1;
            return 0xFFU;

        case SNF_CMD_PAGE_PROGRAM:
            if (wel() && !busy()) {
                s_emu.pgm_addr = s_emu.addr;   /* keep page base across frame reset */
                s_emu.pgm_len  = 0;
                s_emu.state    = STATE_PROGRAM_DATA;
            } else {
                s_emu.state = STATE_DONE;
            }
            return 0xFFU;

        case SNF_CMD_SECTOR_ERASE:
        case SNF_CMD_BLOCK_ERASE_32K:
        case SNF_CMD_BLOCK_ERASE_64K:
            if (wel() && !busy()) {
                s_emu.erase_addr    = s_emu.addr;
                s_emu.erase_pending = 1;
                set_busy(1);
                s_emu.total_erases++;
            }
            s_emu.state = STATE_DONE;
            return 0xFFU;

        case SNF_CMD_READ_MFR_DEV:
            /* mfr first byte, devid second */
            s_emu.state = STATE_READ_DATA;
            s_emu.addr  = 1;   /* response index: just shipped mfr next */
            return SNF_LEGACY_MFR;

        default:
            s_emu.state = STATE_DONE;
            return 0xFFU;
        }

    case STATE_DUMMY:
        if (--s_emu.dummy_left == 0) {
            switch (s_emu.cmd) {
            case SNF_CMD_FAST_READ:
                s_emu.state = STATE_READ_DATA;
                return snf_storage_read_byte(s_emu.addr);
            case SNF_CMD_RELEASE_PWRDN_ID:
                s_emu.state = STATE_DONE;
                return SNF_LEGACY_DEVICE_ID;
            default:
                s_emu.state = STATE_DONE;
                return 0xFFU;
            }
        }
        return 0xFFU;

    case STATE_READ_DATA:
        /* streaming response: advance per command */
        switch (s_emu.cmd) {
        case SNF_CMD_READ:
        case SNF_CMD_FAST_READ:
            s_emu.addr++;
            return snf_storage_read_byte(s_emu.addr);
        case SNF_CMD_READ_STATUS_1:
            return s_emu.sr1;
        case SNF_CMD_READ_STATUS_2:
        case SNF_CMD_READ_STATUS_3:
            return 0x00U;
        case SNF_CMD_JEDEC_ID: {
            uint32_t idx = ++s_emu.addr;
            if (idx == 1) return SNF_JEDEC_TYPE;
            if (idx == 2) return SNF_JEDEC_CAPACITY;
            return 0xFFU;   /* host kept clocking past the 3 ID bytes */
        }
        case SNF_CMD_READ_MFR_DEV: {
            uint32_t idx = ++s_emu.addr;
            if (idx == 1) return SNF_LEGACY_DEVICE_ID;
            return 0xFFU;
        }
        default:
            return 0xFFU;
        }

    case STATE_PROGRAM_DATA:
        /* SPI NOR semantics: a program that overruns 256 bytes wraps
         * inside the same page. We respect that exactly. */
        if (s_emu.pgm_len < SNF_PAGE_SIZE) {
            s_emu.pgm_buf[s_emu.pgm_len++] = rx;
        } else {
            /* overflow: wrap to start of page (same offset semantics
             * as Winbond datasheet) */
            uint32_t page_off = s_emu.pgm_len & (SNF_PAGE_SIZE - 1U);
            s_emu.pgm_buf[page_off] = rx;
        }
        return 0xFFU;

    case STATE_DONE:
    case STATE_IDLE:
    case STATE_CMD_DECODED:
    default:
        return 0xFFU;
    }
}

/* ------------------------------------------------------------------ */
/*  Frame-end callback                                                */
/* ------------------------------------------------------------------ */
void snf_emu_frame_end(void)
{
    /* If we collected a programs worth of bytes, queue the write for
     * the main loop. The state machine has already accepted up to
     * SNF_PAGE_SIZE bytes. */
    if (s_emu.cmd == SNF_CMD_PAGE_PROGRAM &&
        s_emu.state == STATE_PROGRAM_DATA &&
        s_emu.pgm_len > 0 &&
        wel() && !busy())
    {
        s_emu.pgm_pending = 1;
        set_busy(1);
        s_emu.total_programs++;
    }

    /* Reset the frame state. erase_pending and pgm_pending stay set
     * so the main loop can pick them up; busy stays set until the
     * main loop says so. */
    emu_reset_frame();
}

/* ------------------------------------------------------------------ */
/*  Main-loop tick: do the deferred slow stuff                        */
/* ------------------------------------------------------------------ */
unsigned snf_emu_tick(void)
{
    unsigned did = 0;

    /* Snapshot under disabled IRQ -- the SPI ISR could otherwise queue
     * a second program before we clear the flag. */
    __disable_irq();
    int      do_program = s_emu.pgm_pending;
    int      do_erase   = s_emu.erase_pending;
    uint32_t pgm_addr   = s_emu.pgm_addr;    /* captured at ADDR3 -> PROGRAM_DATA */
    uint32_t pgm_len    = s_emu.pgm_len;
    uint32_t erase_a    = s_emu.erase_addr;
    /* clear the flags here; we own pgm_buf / erase_addr until busy=0 */
    s_emu.pgm_pending   = 0;
    s_emu.erase_pending = 0;
    __enable_irq();
    if (do_program) {
        (void)snf_storage_program(pgm_addr, (const uint8_t *)s_emu.pgm_buf, pgm_len);
        set_wel(0);          /* programs auto-clear WEL */
        set_busy(0);
        did++;
    }

    if (do_erase) {
        if (do_erase == 2) (void)snf_storage_chip_erase();
        else               (void)snf_storage_erase_4k(erase_a);
        set_wel(0);
        set_busy(0);
        did++;
    }

    return did;
}

/* ------------------------------------------------------------------ */
/*  Introspection                                                     */
/* ------------------------------------------------------------------ */
uint32_t snf_emu_total_bytes_rx(void) { return s_emu.total_bytes; }
uint32_t snf_emu_total_cmds(void)     { return s_emu.total_cmds; }
uint32_t snf_emu_total_programs(void) { return s_emu.total_programs; }
uint32_t snf_emu_total_erases(void)   { return s_emu.total_erases; }
uint8_t  snf_emu_status_reg1(void)    { return s_emu.sr1; }
