/*
 * softcan.h — bit-banged CAN 2.0A controller for TX32F01 (Cortex-M0).
 *
 * The TX32F01 has no on-chip CAN. We implement the CAN MAC layer in
 * software over a single open-drain GPIO + matching input + EXTI for
 * SOF detection. One TIM IRQ per bit drives the TX/RX state machine
 * (sample point at 75% of bit time). Bus electrical layer is the
 * classic wire-OR: dominant=0 wins arbitration over recessive=1.
 *
 * Coverage of ISO 11898-1 we keep faithfully:
 *   - 11-bit standard frame (data & RTR), DLC up to 8
 *   - bit stuffing (5+1, between SOF and CRC inclusive)
 *   - CRC-15 (poly 0x4599)
 *   - arbitration with TX→RX downgrade on lost recessive bit
 *   - ACK slot honoured (listeners ACK dominant)
 *   - CRC error, form error, stuff error, bit error, ACK error
 *   - TEC / REC counters, error-active / passive / bus-off states
 *   - 3-bit IFS before next frame
 *
 * Out of scope for this port:
 *   - 29-bit extended IDs (IDE=1)
 *   - active error frame *emission* (we detect errors and bump counters
 *     and abort the frame, but do not transmit a 6-bit error flag —
 *     this is a stretch on a software stack at low bitrate and not
 *     needed for the time-sync demo)
 *   - bus-off recovery via 128*11 recessive bits monitor (simplified
 *     to a manual reset)
 */
#ifndef SOFTCAN_H
#define SOFTCAN_H

#include <stdint.h>
#include "TX32F01_periph.h"

#define SOFTCAN_MAX_DLC          8
#define SOFTCAN_TX_QUEUE_DEPTH   4
#define SOFTCAN_RX_QUEUE_DEPTH   8

typedef struct {
    uint16_t id;                          /* 11-bit standard ID */
    uint8_t  dlc;                         /* 0..8 */
    uint8_t  rtr;                         /* 0/1 */
    uint8_t  data[SOFTCAN_MAX_DLC];
} can_frame_t;

/* Filled in by the driver on each successful reception. The timestamp
 * is the value of the µs free-running clock captured at the SOF falling
 * edge (EXTI ISR), not the moment the frame finished — so it is suitable
 * for time-sync use. */
typedef struct {
    can_frame_t frame;
    uint32_t    sof_timestamp_us;
} can_rx_event_t;

typedef enum {
    SOFTCAN_ERROR_ACTIVE  = 0,
    SOFTCAN_ERROR_PASSIVE = 1,
    SOFTCAN_BUS_OFF       = 2
} softcan_err_state_t;

typedef struct {
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t crc_err;
    uint32_t stuff_err;
    uint32_t form_err;
    uint32_t bit_err;
    uint32_t ack_err;
    uint32_t arb_lost;
    uint16_t tec;            /* transmit error counter */
    uint16_t rec;            /* receive  error counter */
    softcan_err_state_t err_state;
} softcan_stats_t;

typedef void (*softcan_rx_cb_t)(const can_rx_event_t *ev);
typedef void (*softcan_tx_cb_t)(const can_frame_t *frame, uint32_t sof_timestamp_us);

typedef struct {
    uint32_t   bitrate;                   /* bps; e.g. 10000 */
    GPIO_Type *tx_port;  uint8_t tx_pin;  /* open-drain output */
    GPIO_Type *rx_port;  uint8_t rx_pin;  /* input; must drive the same bus */
    uint8_t    rx_exti_line;              /* must match rx_pin index, 0..7 */
    uint8_t    rx_exti_gpio_sel;          /* 0..3, value matching rx_port */
    softcan_rx_cb_t rx_cb;                /* called from ISR context */
    softcan_tx_cb_t tx_cb;                /* called from ISR context after a frame finishes */
} softcan_cfg_t;

/* Initialise SoftCAN. Returns 0 on success. May only be called once. */
int  softcan_init(const softcan_cfg_t *cfg);

/* Queue one frame for transmit. Returns 0 on success, -1 if queue full.
 * The frame is copied; the caller does not need to keep it around. */
int  softcan_send(const can_frame_t *frame);

/* Snapshot of stats (compiled, not a pointer into live counters). */
void softcan_get_stats(softcan_stats_t *out);

/* Best-effort current µs reading from TIM2 32-bit virtual counter.
 * Safe from main and ISR contexts. */
uint32_t softcan_now_us(void);

/* IRQ entry points — called from the project's startup vector handlers. */
void softcan_timer_isr(void);
void softcan_rx_edge_isr(void);
void softcan_us_overflow_isr(void);

#endif /* SOFTCAN_H */
