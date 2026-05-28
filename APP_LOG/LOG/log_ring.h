/*
 * log_ring.h — Append-only ring log over SPI NOR.
 *
 *   - 1 record per 256-byte page (header 16 B + payload ≤ 240 B).
 *   - Sectors used as a circular buffer. Oldest is overwritten when full.
 *   - Boot recovery scans every sector's first-page header and locates
 *     head/tail by min/max sequence number — no metadata sectors.
 *   - Erases are asynchronous: log_ring_tick() pumps the state machine.
 *
 * Threading model: single-threaded (cooperative). All entry points must
 * be called from the same context (the COOP scheduler's task callbacks).
 */
#ifndef _LOG_RING_H
#define _LOG_RING_H

#include <stdint.h>

#define LOG_PAYLOAD_MAX     240U
#define LOG_HEADER_SIZE     16U

typedef struct {
    uint32_t seq;
    uint32_t ts_ms;
    uint8_t  type;
    uint8_t  len;
    uint8_t  payload[LOG_PAYLOAD_MAX];
} log_record_t;

typedef struct {
    uint32_t sector;
    uint32_t page;
    uint32_t remaining;       /* upper bound, decremented as we walk */
} log_iter_t;

/* Recovers head/tail from flash. Returns 0 on success, -1 if BD init failed. */
int  log_ring_init(void);

/* Append a record. Returns:
 *    0  : programmed
 *   -1  : busy (caller retries next tick — typical during sector rotation)
 *   -2  : invalid args (len > 240)
 */
int  log_ring_append(uint8_t type, const void *buf, uint8_t len);

/* Pump erase state machine. Call from a COOP task at ≤10 ms cadence. */
void log_ring_tick(void);

/* Iteration over all live records (oldest → newest). */
void log_ring_iter_begin(log_iter_t *it);
int  log_ring_iter_next (log_iter_t *it, log_record_t *out);  /* 1=ok, 0=end */

/* Reset the log (drops all records; tail=head=0, next_seq preserved+1). */
void log_ring_clear(void);

/* Status snapshot. Any pointer may be NULL. */
void log_ring_stats(uint32_t *total_sectors,
                    uint32_t *head_sec, uint32_t *head_page,
                    uint32_t *tail_sec,
                    uint32_t *next_seq);

#endif
