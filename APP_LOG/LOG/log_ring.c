#include "log_ring.h"
#include "log_bd.h"
#include "spi_nor.h"
#include <string.h>

/* Forward decl — supplied by main.c so we can timestamp records. */
extern uint32_t coop_now_ms(void);

#define MAGIC_LOGR      0x4C4F4752UL    /* 'L','O','G','R' little-endian header word */

/* On-flash page layout (little-endian, packed):
 *   0..3   magic
 *   4..7   seq
 *   8..11  ts_ms
 *   12     len
 *   13     type
 *   14..15 crc16   (CCITT, poly 0x1021, init 0xFFFF)
 *   16..   payload (len bytes; bytes [16+len .. 256) ignored)
 */

static uint32_t s_total_sectors;
static uint32_t s_head_sec;
static uint32_t s_head_pg;
static uint32_t s_tail_sec;
static uint32_t s_next_seq;
static uint32_t s_record_count;

static uint32_t s_pe_sector;        /* sector pending/being erased */
static enum { PE_NONE, PE_REQ, PE_INFLIGHT } s_pe_state;

static uint8_t  s_pg[256];

/* ---------- CRC16-CCITT (poly 0x1021, init 0xFFFF), updatable ---------- */
static uint16_t crc16_update(uint16_t c, const uint8_t *p, uint32_t n)
{
    while (n--) {
        c ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++) {
            c = (c & 0x8000U) ? (uint16_t)((c << 1) ^ 0x1021U) : (uint16_t)(c << 1);
        }
    }
    return c;
}

/* ---------- header pack / unpack ---------- */
static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static uint16_t rd_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1]<<8)); }
static void     wr_u32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static void     wr_u16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }

/* Compute the record CRC: header bytes [0..14) followed by payload bytes [0..len). */
static uint16_t record_crc(const uint8_t *page256, uint8_t len)
{
    uint16_t c = crc16_update(0xFFFFU, page256, 14U);
    if (len) c = crc16_update(c, page256 + 16U, len);
    return c;
}

/* Read just the header word + seq for fast sector triage. */
static int read_page_header(uint32_t sec, uint32_t pg, uint8_t out16[16])
{
    log_bd_read(sec, pg * 256U, out16, 16U);
    return rd_u32(out16) == MAGIC_LOGR;
}

/* ---------- recovery on boot ----------
 * Strategy: read first-page header of every sector. If none valid → format.
 * Else: pick sector with max-seq first-page = candidate "live region head".
 * Walk that sector forward to find the highest-page valid record → head_pg.
 * tail_sec = sector with min-seq first-page (within the live region).
 */
static void recover(void)
{
    s_total_sectors = log_bd_sector_count();
    if (s_total_sectors == 0) { s_head_sec = s_head_pg = s_tail_sec = 0; s_next_seq = 1; return; }

    uint8_t  hdr[16];
    uint32_t max_seq = 0;            /* 0 == "never seen" sentinel; real seqs start at 1 */
    uint32_t max_sec = 0;
    uint32_t min_seq = 0xFFFFFFFFUL;
    uint32_t min_sec = 0;
    int      any = 0;

    for (uint32_t s = 0; s < s_total_sectors; s++) {
        if (!read_page_header(s, 0, hdr)) continue;
        uint32_t seq = rd_u32(hdr + 4);
        if (seq == 0xFFFFFFFFUL || seq == 0) continue;   /* implausible */
        any = 1;
        if (seq > max_seq)      { max_seq = seq; max_sec = s; }
        if (seq < min_seq)      { min_seq = seq; min_sec = s; }
    }

    if (!any) {
        /* fresh device or corrupted — format sector 0 */
        s_head_sec = 0;
        s_head_pg  = 0;
        s_tail_sec = 0;
        s_next_seq = 1;
        s_record_count = 0;
        /* Sector 0 needs erasing before first write. */
        s_pe_sector = 0;
        s_pe_state  = PE_REQ;
        return;
    }

    /* Walk max_sec forward to find last valid page. */
    uint32_t last_pg = 0;
    uint32_t last_seq = max_seq;
    for (uint32_t pg = 0; pg < 16; pg++) {
        if (!read_page_header(max_sec, pg, hdr)) break;
        last_pg = pg;
        last_seq = rd_u32(hdr + 4);
    }

    s_head_sec = max_sec;
    s_head_pg  = last_pg + 1;       /* next write slot; may be 16 → triggers rotation */
    s_tail_sec = min_sec;
    s_next_seq = last_seq + 1;

    /* Rough record count: |head - tail| sectors × 16 pages — upper bound only. */
    uint32_t span = (s_head_sec + s_total_sectors - s_tail_sec) % s_total_sectors;
    s_record_count = span * 16U + s_head_pg;

    /* Pre-erase next blank if head is about to rotate. */
    if (s_head_pg >= 16U) {
        s_pe_sector = (s_head_sec + 1U) % s_total_sectors;
        s_pe_state  = PE_REQ;
    } else {
        s_pe_state  = PE_NONE;
    }
}

/* ---------- public API ---------- */
int log_ring_init(void)
{
    if (log_bd_init() != 0) return -1;
    recover();
    return 0;
}

void log_ring_tick(void)
{
    switch (s_pe_state) {
    case PE_NONE: return;
    case PE_REQ:
        if (log_bd_erase_sector(s_pe_sector) == 0) s_pe_state = PE_INFLIGHT;
        return;
    case PE_INFLIGHT:
        if (!log_bd_busy()) s_pe_state = PE_NONE;
        return;
    }
}

/* Move head to the next sector. Returns 0 on success, -1 if blocked
 * (pre-erase still running). Advances tail when wrapping over it. */
static int rotate_head(void)
{
    uint32_t target = (s_head_sec + 1U) % s_total_sectors;

    /* We need 'target' to be erased before we can write to it. */
    if (s_pe_state != PE_NONE && s_pe_sector == target) return -1;     /* still erasing it */
    if (s_pe_state == PE_NONE && target != s_head_sec) {
        /* No erase scheduled — must do one synchronously-ish for target. */
        s_pe_sector = target;
        s_pe_state  = PE_REQ;
        return -1;
    }

    /* Pre-erase is done. Advance. */
    s_head_sec = target;
    s_head_pg  = 0;

    /* If we just stomped on the tail, push tail forward. */
    if (s_head_sec == s_tail_sec) {
        s_tail_sec = (s_tail_sec + 1U) % s_total_sectors;
        if (s_record_count >= 16U) s_record_count -= 16U;
    }

    /* Kick erase of the *next* blank so the next rotation is non-blocking. */
    s_pe_sector = (s_head_sec + 1U) % s_total_sectors;
    s_pe_state  = PE_REQ;
    return 0;
}

int log_ring_append(uint8_t type, const void *buf, uint8_t len)
{
    if (len > LOG_PAYLOAD_MAX) return -2;
    if (s_total_sectors == 0)  return -2;

    /* Pump erase before we ask whether we can write. */
    log_ring_tick();

    if (s_head_pg >= 16U) {
        if (rotate_head() != 0) return -1;
    }

    if (log_bd_busy()) return -1;     /* erase landed between tick and now */

    /* Build the 256-byte page image. Unused tail bytes stay 0xFF. */
    memset(s_pg, 0xFF, sizeof(s_pg));
    wr_u32(s_pg + 0, MAGIC_LOGR);
    wr_u32(s_pg + 4, s_next_seq);
    wr_u32(s_pg + 8, coop_now_ms());
    s_pg[12] = len;
    s_pg[13] = type;
    if (len) memcpy(s_pg + 16, buf, len);
    wr_u16(s_pg + 14, record_crc(s_pg, len));

    if (log_bd_program_page(s_head_sec, s_head_pg, s_pg, 256U) != 0) return -1;

    s_next_seq++;
    s_head_pg++;
    s_record_count++;

    /* Block briefly until program completes so the caller can safely call again.
       Page program is typ 0.7ms, max 3ms — at 24MHz this is ~17K..72K cycles.
       For a logger task running at e.g. 100ms cadence, busy-waiting is fine. */
    while (log_bd_busy()) { }
    return 0;
}

/* ---------- iteration ---------- */
void log_ring_iter_begin(log_iter_t *it)
{
    it->sector    = s_tail_sec;
    it->page      = 0;
    it->remaining = s_record_count;
}

/* Iteration shares s_pg with append(); callers must not append while iterating. */
int log_ring_iter_next(log_iter_t *it, log_record_t *out)
{
    if (it->remaining == 0) return 0;

    log_bd_read(it->sector, it->page * 256U, s_pg, 256U);

    it->page++;
    if (it->page >= 16U) {
        it->page   = 0;
        it->sector = (it->sector + 1U) % s_total_sectors;
    }
    it->remaining--;

    if (rd_u32(s_pg) != MAGIC_LOGR) return 0;            /* hit erased / sentinel */
    uint8_t len = s_pg[12];
    if (len > LOG_PAYLOAD_MAX) return 0;
    if (record_crc(s_pg, len) != rd_u16(s_pg + 14)) return 0;

    out->seq    = rd_u32(s_pg + 4);
    out->ts_ms  = rd_u32(s_pg + 8);
    out->len    = len;
    out->type   = s_pg[13];
    if (len) memcpy(out->payload, s_pg + 16, len);
    return 1;
}

void log_ring_clear(void)
{
    /* Chip erase: takes 25-100s on W25Q16, but it's the only way to ensure
       no ghost records resurrect after reset. We just kick it off — the next
       append() / tick() will see busy and back off. */
    while (log_bd_busy()) { }
    spi_nor_erase_chip_start();

    s_head_sec = 0;
    s_head_pg  = 0;
    s_tail_sec = 0;
    s_record_count = 0;
    s_pe_sector = 0;
    s_pe_state  = PE_NONE;     /* chip erase covers sector 0 too */
    /* next_seq deliberately NOT reset — preserves monotonicity across clears
       so that if a crash interrupts the wipe, max-seq recovery still picks
       the right "current" sector. */
}

void log_ring_stats(uint32_t *total_sectors, uint32_t *head_sec, uint32_t *head_page,
                    uint32_t *tail_sec, uint32_t *next_seq)
{
    if (total_sectors) *total_sectors = s_total_sectors;
    if (head_sec)      *head_sec      = s_head_sec;
    if (head_page)     *head_page     = s_head_pg;
    if (tail_sec)      *tail_sec      = s_tail_sec;
    if (next_seq)      *next_seq      = s_next_seq;
}
