/*
 * can_sync.h — multi-node time synchronisation over SoftCAN.
 *
 * Protocol on the wire (CANopen-flavoured, simplified):
 *
 *   Master, every SYNC_PERIOD_MS:
 *     1. Send SYNC  (ID = 0x080, DLC=1, data[0]=counter++).
 *        At the SOF of this frame the master records its local µs
 *        timestamp T_m via the SoftCAN TX callback.
 *     2. Send FUP   (ID = 0x081, DLC=8, data = T_m little-endian
 *        followed by counter, padded). FUP = "Follow-Up", a separate
 *        frame carrying the SYNC's master-side timestamp. This split
 *        (two-step time message) is what PTP/AUTOSAR use to avoid
 *        in-frame latency dependencies.
 *
 *   Slave, on RX:
 *     - On SYNC: record local µs SOF timestamp T_s.
 *     - On FUP with matching counter: derive offset and skew using a
 *       Cristian-style algorithm with EWMA smoothing.
 *
 * Slave clock model:
 *   virtual_us(now_local) = anchor_virtual + (now_local - anchor_local)
 *                           * (1 + skew_ppm / 1e6)
 *   On each sync event the anchor is re-pinned to (T_s, T_m).
 *   skew_ppm is updated as a low-pass filter of the measured drift
 *   between consecutive sync events.
 */
#ifndef CAN_SYNC_H
#define CAN_SYNC_H

#include <stdint.h>

#ifndef CAN_SYNC_ID
#define CAN_SYNC_ID    0x080U
#endif
#ifndef CAN_FUP_ID
#define CAN_FUP_ID     0x081U
#endif

typedef struct {
    uint8_t  is_master;
    uint16_t sync_period_ms;        /* master only */
} can_sync_cfg_t;

void     can_sync_init(const can_sync_cfg_t *cfg);

/* Call periodically from a 1-ms tick (or from main loop). Master uses
 * this to schedule SYNC emission; slave uses it only to log stats. */
void     can_sync_tick_ms(void);

/* Synchronised wall-clock in microseconds. Master returns its own local
 * µs (which IS the reference); slave returns its corrected virtual µs. */
uint64_t can_sync_virtual_us(void);

/* Diagnostics. */
int32_t  can_sync_last_offset_us(void);   /* most recent residual error */
int32_t  can_sync_skew_ppm(void);
uint32_t can_sync_updates(void);          /* number of sync events applied */

/* Internal hooks — wired by main.c when SoftCAN delivers a frame. */
void     can_sync_on_rx(uint16_t id, const uint8_t *data, uint8_t dlc,
                        uint32_t sof_local_us);
void     can_sync_on_tx(uint16_t id, uint32_t sof_local_us);

#endif
