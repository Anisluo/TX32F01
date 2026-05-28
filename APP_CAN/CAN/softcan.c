/*
 * softcan.c — bit-banged CAN 2.0A MAC for TX32F01.
 *
 * Timing model
 * ------------
 *  - HCLK = 24 MHz, default bitrate = 10 kbps. One CAN bit = 100 µs.
 *  - We model 8 time quanta per bit (SYNC=1, PROP=2, PHASE1=3, PHASE2=2)
 *    and sample at 6 tq (~75% of the bit), but we do NOT take 8 IRQs/bit:
 *    we take 1 IRQ per bit, scheduled at the sample point. This is a
 *    common simplification for software CAN at low bitrate where the bus
 *    is short enough that the sample point window is wide.
 *
 *    On SOF (EXTI falling edge, IDLE state):
 *        TIM->CNT = 0; TIM->ARR = sample_offset_cycles
 *    Next CCIF fires AT THE SAMPLE POINT of the SOF bit. After the SOF
 *    handler runs we restore ARR to BIT_CYCLES so subsequent IRQs fire
 *    once per bit aligned to that initial sample-point.
 *
 *  - For TX-initiated frames we drive TX dominant from main context,
 *    set CNT=0, ARR=sample_offset_cycles, and follow the same cadence.
 *
 *  - We resync only on SOF; intra-frame resync (CAN's "hard sync") is
 *    omitted because at 10 kbps the worst-case clock drift over 135
 *    bits is < 0.02 % * 13.5 ms < 3 µs, well within our ±25 µs sample
 *    window margin.
 *
 * Frame model
 * -----------
 *  - 11-bit standard frames only (CAN 2.0A).
 *  - At TX queue time we build the full bit-stream into a buffer with
 *    bit-stuffing applied through the CRC field; the suffix (CRC_DEL +
 *    ACK + ACK_DEL + EOF + IFS) is appended raw.
 *  - For RX we feed bus samples through an inline destuffer.
 *
 * Error handling
 * --------------
 *  - We detect bit / stuff / form / CRC / ACK errors, bump TEC/REC per
 *    ISO 11898-1 §12.1.4, and transition to error-passive / bus-off.
 *  - We do NOT emit error frames on the bus (would need 6-bit dominant
 *    transmission, possible but adds complexity for marginal benefit
 *    on a software stack).
 */

#include "softcan.h"
#include "HAL_GPIO.h"
#include "HAL_SCU.h"
#include "HAL_TIM.h"

#ifndef SOFTCAN_BITRATE_DEFAULT
#define SOFTCAN_BITRATE_DEFAULT   10000U
#endif

#ifndef SOFTCAN_HCLK_HZ
#define SOFTCAN_HCLK_HZ           24000000UL
#endif

#define TQ_PER_BIT                8U
#define SAMPLE_TQ                 6U    /* sample at 75% of bit */

/* CAN-15 CRC: polynomial x^15+x^14+x^10+x^8+x^7+x^4+x^3+1 = 0x4599. */
#define CAN_CRC15_POLY            0x4599U

/* ---------------- Per-bit timing (set in softcan_init) ---------------- */
static uint16_t s_bit_cycles;           /* TIM ticks per full bit */
static uint16_t s_sample_offset_cycles; /* TIM ticks from SOF edge to first sample */

/* ---------------- Pin handles (set in softcan_init) ------------------- */
static GPIO_Type *s_tx_port; static uint8_t s_tx_pin;
static GPIO_Type *s_rx_port; static uint8_t s_rx_pin;
static uint8_t    s_rx_exti_line;
static softcan_rx_cb_t s_rx_cb;
static softcan_tx_cb_t s_tx_cb;
static uint32_t        s_tx_sof_us;     /* SOF timestamp of in-flight TX */

/* Driver-internal: in OD mode "1" releases the bus (recessive). */
static inline void bus_drive_dominant(void) { GPIO_ResetBits(s_tx_port, s_tx_pin); }
static inline void bus_release_recessive(void){ GPIO_SetBits  (s_tx_port, s_tx_pin); }
static inline uint8_t bus_sample(void) { return GPIO_ReadInputDataBit(s_rx_port, s_rx_pin); }

/* ---------------- µs free-running clock (TIM2) ------------------------ */
static volatile uint32_t s_us_hi = 0;

static void us_clock_init(void)
{
    TIM_InitTypeDef t;
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_TIM2, ENABLE);
    SCU_Lock();
    TIM_DeInit(TIM2);
    /* PCLK/DIV gives 1 MHz tick, ARR=0xFFFE → overflow ~65.534 ms. */
    t.TIM_Mode      = TIM_Mode_CNT;
    t.TIM_Prescaler = (uint16_t)(SOFTCAN_HCLK_HZ / 1000000UL);   /* 24 → 1 µs */
    t.TIM_Period    = 0xFFFE;
    TIM_Init(TIM2, &t);
    TIM_ClearFlag(TIM2, TIM_IFR_CNTIF);
    TIM_ITConfig (TIM2, TIM_IER_CNTIE, ENABLE);
    NVIC_SetPriority(TIM2_IRQn, 2);     /* lower than the CAN bit timer */
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM_Cmd(TIM2, ENABLE);
}

/* Read the 32-bit µs counter atomically. Loop until hi reading is
 * stable around the lo reading. */
uint32_t softcan_now_us(void)
{
    uint32_t hi1, hi2, lo;
    do {
        hi1 = s_us_hi;
        lo  = TIM2->CNT & 0xFFFFU;
        hi2 = s_us_hi;
    } while (hi1 != hi2);
    return (hi1 << 16) | lo;
}

void softcan_us_overflow_isr(void)
{
    if (TIM_GetFlagStatus(TIM2, TIM_IFR_CNTIF)) {
        TIM_ClearFlag(TIM2, TIM_IFR_CNTIF);
        s_us_hi++;
    }
}

/* ---------------- Bit-stream layout & helpers ------------------------- */

/* Maximum bit-stream length: SOF(1) + ID(11) + RTR(1) + IDE(1) + r0(1) +
 * DLC(4) + DATA(64) + CRC(15) = 98 raw bits, plus up to 24 stuff bits in
 * theory, plus suffix (CRC_DEL+ACK+ACK_DEL+EOF+IFS = 13). Round up. */
#define MAX_STREAM_BITS   160

typedef struct {
    uint8_t  bits[MAX_STREAM_BITS];   /* 1=recessive, 0=dominant */
    uint16_t len;                     /* total bits including suffix */
    uint16_t arb_end_idx;             /* exclusive: end of arbitration */
    uint16_t crc_del_idx;             /* first non-stuffed bit (CRC_DEL) */
    uint16_t ack_slot_idx;
    can_frame_t frame;
} can_tx_stream_t;

/* CRC-15 of `nbits` bits read MSB-first from `bits[]`. */
static uint16_t crc15_compute(const uint8_t *bits, uint16_t nbits)
{
    uint16_t crc = 0;
    for (uint16_t i = 0; i < nbits; ++i) {
        uint16_t in = bits[i] & 1U;
        uint16_t bit_out = ((crc >> 14) ^ in) & 1U;
        crc = (crc << 1) & 0x7FFFU;
        if (bit_out) crc ^= CAN_CRC15_POLY;
    }
    return crc & 0x7FFFU;
}

/* Build the bit-stream for one frame. Returns 0 on success. */
static int tx_stream_build(can_tx_stream_t *s, const can_frame_t *f)
{
    if (f->dlc > SOFTCAN_MAX_DLC) return -1;
    s->frame = *f;

    /* 1. Pre-stuff raw stream: SOF + ID + RTR + IDE + r0 + DLC + DATA. */
    uint8_t  raw[1+11+1+1+1+4+64];
    uint16_t n = 0;
    raw[n++] = 0;                                                /* SOF */
    for (int i = 10; i >= 0; --i) raw[n++] = (f->id >> i) & 1U;  /* ID  */
    raw[n++] = f->rtr ? 1U : 0U;                                 /* RTR */
    raw[n++] = 0;                                                /* IDE = 0 std */
    raw[n++] = 0;                                                /* r0  */
    for (int i = 3; i >= 0; --i) raw[n++] = (f->dlc >> i) & 1U;  /* DLC */
    uint8_t db = f->dlc; if (db > 8) db = 8;
    for (uint8_t k = 0; k < db; ++k)
        for (int i = 7; i >= 0; --i) raw[n++] = (f->data[k] >> i) & 1U;

    /* 2. CRC over the raw (unstuffed) header+data. */
    uint16_t crc = crc15_compute(raw, n);
    for (int i = 14; i >= 0; --i) raw[n++] = (crc >> i) & 1U;    /* CRC */

    /* 3. Bit-stuff the entire raw stream into s->bits[]. */
    s->len = 0;
    uint8_t  last = 0xFF;
    uint8_t  run  = 0;
    for (uint16_t i = 0; i < n; ++i) {
        uint8_t b = raw[i];
        if (b == last) {
            run++;
        } else {
            run = 1;
            last = b;
        }
        s->bits[s->len++] = b;
        if (run == 5) {
            /* Insert opposite-polarity stuff bit. */
            uint8_t stuff = b ^ 1U;
            s->bits[s->len++] = stuff;
            last = stuff;
            run  = 1;
        }
        /* Record where arbitration ends — after SOF(1)+ID(11)+RTR(1). */
        if (i == (1 + 11)) {            /* just emitted RTR at raw index 12 */
            s->arb_end_idx = s->len;    /* exclusive */
        }
    }

    /* 4. Suffix (non-stuffed). */
    s->crc_del_idx  = s->len; s->bits[s->len++] = 1; /* CRC delimiter */
    s->ack_slot_idx = s->len; s->bits[s->len++] = 1; /* ACK slot: TX releases */
                              s->bits[s->len++] = 1; /* ACK delimiter */
    for (int i = 0; i < 7; ++i) s->bits[s->len++] = 1; /* EOF */
    for (int i = 0; i < 3; ++i) s->bits[s->len++] = 1; /* IFS */

    return 0;
}

/* ---------------- TX queue ------------------------------------------- */

static can_tx_stream_t s_tx_q[SOFTCAN_TX_QUEUE_DEPTH];
static volatile uint8_t s_tx_head, s_tx_tail, s_tx_count;
static can_tx_stream_t *s_tx_cur;       /* the stream currently being TX'd */

static int tx_q_push(const can_frame_t *f)
{
    __disable_irq();
    if (s_tx_count >= SOFTCAN_TX_QUEUE_DEPTH) { __enable_irq(); return -1; }
    can_tx_stream_t *slot = &s_tx_q[s_tx_tail];
    __enable_irq();
    if (tx_stream_build(slot, f) != 0) return -1;
    __disable_irq();
    s_tx_tail = (uint8_t)((s_tx_tail + 1) % SOFTCAN_TX_QUEUE_DEPTH);
    s_tx_count++;
    __enable_irq();
    return 0;
}

static can_tx_stream_t *tx_q_peek(void)
{
    return s_tx_count ? &s_tx_q[s_tx_head] : 0;
}

static void tx_q_pop(void)
{
    if (s_tx_count) {
        s_tx_head = (uint8_t)((s_tx_head + 1) % SOFTCAN_TX_QUEUE_DEPTH);
        s_tx_count--;
    }
}

/* ---------------- State machine -------------------------------------- */

typedef enum {
    ST_IDLE  = 0,
    ST_TX_RUN,
    ST_RX_RUN
} state_t;

static volatile state_t s_state;

/* TX runtime: bit index into s_tx_cur->bits, count of consecutive recessive
 * bits seen on bus (used for IFS detection). */
static uint16_t s_tx_bit_idx;

/* RX runtime: collected raw (unstuffed) bits. */
typedef struct {
    uint8_t  raw[1 + 11 + 1 + 1 + 1 + 4 + 64 + 15];
    uint16_t raw_n;            /* index into raw[] for the next raw bit  */
    uint16_t bits_received;    /* total physical bus bits consumed       */
    uint8_t  last_bit;         /* last decoded raw bit (for stuff check) */
    uint8_t  same_run;         /* consecutive same-polarity raw bits     */
    uint8_t  expecting_stuff;  /* set when same_run==5; next bit must be opposite */
    uint16_t crc_received;
    uint8_t  in_suffix;        /* set after CRC bits captured            */
    uint8_t  suffix_idx;       /* index into suffix bits */
    uint32_t sof_us;           /* SOF timestamp captured by EXTI         */
    can_frame_t frame;         /* decoded so far */
} rx_state_t;
static rx_state_t s_rx;

static softcan_stats_t s_stats;

/* ----- error counter helpers (ISO 11898-1 §12.1.4 simplified) -------- */
static void update_err_state(void)
{
    if (s_stats.tec >= 256) {
        s_stats.err_state = SOFTCAN_BUS_OFF;
    } else if (s_stats.tec >= 128 || s_stats.rec >= 128) {
        s_stats.err_state = SOFTCAN_ERROR_PASSIVE;
    } else {
        s_stats.err_state = SOFTCAN_ERROR_ACTIVE;
    }
}
static void tec_bump(uint16_t inc)
{
    uint32_t v = (uint32_t)s_stats.tec + inc;
    if (v > 0xFFFFU) v = 0xFFFFU;
    s_stats.tec = (uint16_t)v;
    update_err_state();
}
static void rec_bump(uint16_t inc)
{
    uint32_t v = (uint32_t)s_stats.rec + inc;
    if (v > 127) v = 127;       /* REC saturates at 127 in real CAN */
    s_stats.rec = (uint16_t)v;
    update_err_state();
}
static void tec_credit(void)  { if (s_stats.tec) s_stats.tec--; update_err_state(); }
static void rec_credit(void)  { if (s_stats.rec) s_stats.rec--; update_err_state(); }

/* ----- abort frame, return to IDLE ----------------------------------- */
static void abort_to_idle(void)
{
    bus_release_recessive();
    s_state = ST_IDLE;
    s_tx_cur = 0;
    s_rx.raw_n = 0;
    s_rx.bits_received = 0;
    s_rx.same_run = 0;
    s_rx.expecting_stuff = 0;
    s_rx.in_suffix = 0;
    s_rx.suffix_idx = 0;
    /* Re-arm SOF EXTI. (TX32F01 PR is clear-by-writing-0, not W1C.) */
    EXTI->PR  &= ~(1U << s_rx_exti_line);
    EXTI->IMR |=  (1U << s_rx_exti_line);
}

/* ----- RX raw-bit consumer with destuff ------------------------------- */
/* Returns 1 if a raw (kept) bit was produced into rx.raw[], 0 otherwise
 * (stuff bit consumed). Sets *err to non-zero on stuff error.            */
static uint8_t rx_feed_bit(uint8_t bit, uint8_t *err)
{
    *err = 0;
    if (s_rx.expecting_stuff) {
        s_rx.expecting_stuff = 0;
        if (bit == s_rx.last_bit) { *err = 1; return 0; }
        s_rx.last_bit = bit;
        s_rx.same_run = 1;
        return 0;                   /* stuff bit consumed */
    }
    if (s_rx.raw_n == 0 || bit != s_rx.last_bit) {
        s_rx.same_run = 1;
    } else {
        s_rx.same_run++;
    }
    s_rx.last_bit = bit;
    s_rx.raw[s_rx.raw_n++] = bit;
    if (s_rx.same_run == 5) s_rx.expecting_stuff = 1;
    return 1;
}

/* Pull n raw bits MSB-first starting at index `start` into an integer. */
static uint32_t bits_to_uint(const uint8_t *b, uint16_t start, uint8_t n)
{
    uint32_t v = 0;
    for (uint8_t i = 0; i < n; ++i) v = (v << 1) | (b[start + i] & 1U);
    return v;
}

/* Deliver the current RX frame to the user callback if it parses. */
static void rx_deliver(void)
{
    can_rx_event_t ev;
    ev.frame = s_rx.frame;
    ev.sof_timestamp_us = s_rx.sof_us;
    s_stats.rx_frames++;
    rec_credit();
    if (s_rx_cb) s_rx_cb(&ev);
}

/* Forward decl: defined further below — invoked from the bit-timer ISR
 * to drain the TX queue at frame boundaries. */
static void try_start_tx(void);

/* ---------------- bit-timer ISR -------------------------------------- */
/*
 * Cadence:
 *   - On first IRQ after SOF/TX-start, we read the bus (RX) or have already
 *     written the bit (TX). All we need to do is decide what to do next.
 *
 * Layout per bit:
 *   [0 cyc]  edge / TX driver state set
 *   [SAMPLE_TQ * tq_cycles]  TIM IRQ fires here → we sample / latch
 *   [TQ_PER_BIT * tq_cycles] start of next bit
 *
 * In our 1-IRQ-per-bit scheme: on the ISR we first sample (RX) or compare
 * (TX), then immediately load the *next bit value* into the TX pin. The
 * remaining (8-SAMPLE_TQ)=2 tq give comfortable headroom before the next
 * sample. ARR is BIT_CYCLES so the next ISR is one bit later.
 */
void softcan_timer_isr(void)
{
    if (!TIM_GetFlagStatus(TIM0, TIM_IFR_CNTIF)) return;
    TIM_ClearFlag(TIM0, TIM_IFR_CNTIF);

    /* After the first scheduled IRQ (which used sample_offset_cycles),
     * pin ARR to full bit time. */
    if (TIM0->ARR != s_bit_cycles) TIM0->ARR = s_bit_cycles;

    uint8_t sample = bus_sample();

    if (s_state == ST_TX_RUN) {
        can_tx_stream_t *s = s_tx_cur;
        uint8_t expected = s->bits[s_tx_bit_idx];

        /* Arbitration phase: if we wrote recessive but bus is dominant,
         * we lost arbitration to a higher-priority frame. Convert to RX. */
        if (s_tx_bit_idx < s->arb_end_idx) {
            if (expected == 1 && sample == 0) {
                s_stats.arb_lost++;
                /* Switch to RX mid-frame. We have already consumed
                 * `s_tx_bit_idx + 1` physical bits, all matching the
                 * arbitration bits we wrote. Re-seed RX state from those.
                 * Simpler & robust: drop the frame back into the queue
                 * head, abort, let the winning frame's SOF retrigger us
                 * — but we're already past SOF. Accept partial loss:
                 * mark as not consumed (we keep it queued) and become a
                 * passive listener for the remainder of this frame. */
                bus_release_recessive();
                /* Re-seed RX from arbitration bits seen so far (all known
                 * to be `expected == sample`, except this last one). */
                s_rx.raw_n = 0; s_rx.bits_received = 0;
                s_rx.same_run = 0; s_rx.expecting_stuff = 0;
                s_rx.last_bit = 0xFF;
                uint8_t err;
                for (uint16_t i = 0; i <= s_tx_bit_idx; ++i) {
                    uint8_t b = (i < s_tx_bit_idx) ? s->bits[i] : sample;
                    (void)rx_feed_bit(b, &err);
                    if (err) { abort_to_idle(); return; }
                }
                s_rx.sof_us = s_tx_sof_us;        /* same SOF as winning frame, near enough */
                s_state = ST_RX_RUN;
                s_tx_bit_idx = 0;
                return;
            }
            /* else: bus matches expected — continue. */
        } else {
            /* Bit-monitor outside arbitration. ACK slot is special: TX
             * writes recessive (1) but expects to *see* dominant from a
             * remote listener. */
            if (s_tx_bit_idx == s->ack_slot_idx) {
                if (sample != 0) {
                    s_stats.ack_err++;
                    tec_bump(8);
                    abort_to_idle();
                    return;
                }
            } else if (sample != expected) {
                s_stats.bit_err++;
                tec_bump(8);
                abort_to_idle();
                return;
            }
        }

        /* Advance to next bit and drive it. */
        s_tx_bit_idx++;
        if (s_tx_bit_idx >= s->len) {
            /* Frame done. We have emitted EOF (7) + IFS (3) as part of
             * the stream, so it's spec-legal to begin a new TX SOF on
             * the next bit boundary. */
            s_stats.tx_frames++;
            tec_credit();
            can_frame_t f_done = s->frame;
            uint32_t sof = s_tx_sof_us;
            softcan_tx_cb_t cb = s_tx_cb;
            tx_q_pop();
            abort_to_idle();
            if (cb) cb(&f_done, sof);
            try_start_tx();         /* drain queue if more frames waiting */
            return;
        }
        uint8_t next = s->bits[s_tx_bit_idx];
        if (next) bus_release_recessive(); else bus_drive_dominant();
        return;
    }

    if (s_state == ST_RX_RUN) {
        s_rx.bits_received++;

        /* Suffix processing (after stuff-zone ends). */
        if (s_rx.in_suffix) {
            uint8_t expected_recessive;
            switch (s_rx.suffix_idx) {
                case 0:  /* CRC delimiter, must be recessive */
                    expected_recessive = 1;
                    if (sample != expected_recessive) {
                        s_stats.form_err++; rec_bump(1); abort_to_idle(); return;
                    }
                    break;
                case 1:  /* ACK slot: drive dominant if we accepted */
                    /* Listener drives ACK regardless of acceptance (we
                     * don't have a filter table yet). The IRQ ran *at*
                     * the sample point — we cannot retroactively drive
                     * earlier. So at SAMPLE point we already missed our
                     * chance to assert the ACK. Strategy: drive dominant
                     * for the remainder of this bit (about (TQ_PER_BIT
                     * - SAMPLE_TQ)*tq_cycles). It still pulls the bus
                     * down for the second half of the slot, which a
                     * synchronous receiver doing the same will see as
                     * dominant if its sample is later than ours.
                     *
                     * For two-MCU symmetric installations this works
                     * because both nodes share the same clock-relative
                     * sample point and a recessive ACK slot from the
                     * TX'er releases the bus just as we pull it down. */
                    bus_drive_dominant();
                    expected_recessive = 0;     /* sample shows whatever */
                    break;
                case 2:  /* ACK delimiter */
                    bus_release_recessive();
                    expected_recessive = 1;
                    if (sample != expected_recessive) {
                        s_stats.form_err++; rec_bump(1); abort_to_idle(); return;
                    }
                    break;
                default:
                    /* EOF (7 bits) + IFS (3 bits) = bits 3..12 */
                    expected_recessive = 1;
                    if (s_rx.suffix_idx < 10 && sample != expected_recessive) {
                        s_stats.form_err++; rec_bump(1); abort_to_idle(); return;
                    }
                    break;
            }
            s_rx.suffix_idx++;
            if (s_rx.suffix_idx == 13) {
                /* EOF+IFS satisfied → deliver, then drain TX queue. */
                rx_deliver();
                abort_to_idle();
                try_start_tx();
            }
            return;
        }

        /* Stuff-zone (SOF..CRC). Feed bit through destuffer. */
        uint8_t err;
        uint8_t kept = rx_feed_bit(sample, &err);
        if (err) {
            s_stats.stuff_err++; rec_bump(1); abort_to_idle(); return;
        }
        if (kept) {
            /* After 1+11+1+1+1+4 raw bits we know the header. */
            if (s_rx.raw_n == 19) {
                s_rx.frame.id  = (uint16_t)bits_to_uint(s_rx.raw, 1, 11);
                s_rx.frame.rtr = s_rx.raw[12];
                /* raw[13] = IDE, raw[14] = r0, raw[15..18] = DLC */
                uint8_t dlc = (uint8_t)bits_to_uint(s_rx.raw, 15, 4);
                if (dlc > 8) dlc = 8;
                s_rx.frame.dlc = dlc;
                if (s_rx.raw[13] != 0) {        /* IDE must be 0 for std */
                    s_stats.form_err++; rec_bump(1); abort_to_idle(); return;
                }
            }
            /* After header + DATA we have all the bytes. */
            uint16_t hdr = 19;
            uint16_t data_bits = (uint16_t)s_rx.frame.dlc * 8U;
            if (s_rx.raw_n == hdr + data_bits) {
                for (uint8_t k = 0; k < s_rx.frame.dlc; ++k) {
                    s_rx.frame.data[k] =
                        (uint8_t)bits_to_uint(s_rx.raw, hdr + (uint16_t)k * 8U, 8);
                }
            }
        }
        /* End-of-stuff-zone transition. The CRC delimiter is NOT part of
         * the stuffed region; if the CRC ends with 5 same-polarity raw
         * bits, the next bus bit is a stuff bit (handled above) and only
         * the bit after that is CRC_DEL. Hence check expecting_stuff. */
        {
            uint16_t want = 19U + (uint16_t)s_rx.frame.dlc * 8U + 15U;
            if (s_rx.raw_n == want && !s_rx.expecting_stuff) {
                uint16_t got  = (uint16_t)bits_to_uint(s_rx.raw, want - 15, 15);
                uint16_t calc = crc15_compute(s_rx.raw, want - 15);
                if (got != calc) {
                    s_stats.crc_err++; rec_bump(1); abort_to_idle(); return;
                }
                s_rx.in_suffix  = 1;
                s_rx.suffix_idx = 0;
            }
        }
        return;
    }

    /* ST_IDLE: stray IRQ — disable until next SOF. */
    TIM_ITConfig(TIM0, TIM_IER_CNTIE, DISABLE);
}

/* ---------------- EXTI SOF ISR --------------------------------------- */

void softcan_rx_edge_isr(void)
{
    if (!(EXTI->PR & (1U << s_rx_exti_line))) return;
    EXTI->PR &= ~(1U << s_rx_exti_line);        /* clear pending */

    if (s_state != ST_IDLE) return;             /* ignore during frame */
    if (s_stats.err_state == SOFTCAN_BUS_OFF) return;

    /* Capture SOF µs for time-sync. The TIM2 read is a few cycles of
     * latency past the actual edge; both ends incur roughly the same
     * latency so the sync residual is small and constant. */
    s_rx.sof_us = softcan_now_us();
    s_rx.raw_n = 0;
    s_rx.bits_received = 0;
    s_rx.same_run = 0;
    s_rx.last_bit = 0xFF;
    s_rx.expecting_stuff = 0;
    s_rx.in_suffix = 0;
    s_rx.suffix_idx = 0;
    /* SOF gets fed by the first timer ISR at the sample point. */

    /* Reset bit timer so the next IRQ fires at the sample point of SOF. */
    TIM0->CNT = 0;
    TIM0->ARR = s_sample_offset_cycles;
    TIM_ClearFlag(TIM0, TIM_IFR_CNTIF);
    TIM_ITConfig (TIM0, TIM_IER_CNTIE, ENABLE);

    /* Mask EXTI until end-of-frame; we don't want intra-frame edges. */
    EXTI->IMR &= ~(1U << s_rx_exti_line);

    s_state = ST_RX_RUN;
}

/* ---------------- Try to start a queued TX from main / ISR tail ------ */
static void try_start_tx(void)
{
    if (s_state != ST_IDLE) return;
    if (s_stats.err_state == SOFTCAN_BUS_OFF) return;
    can_tx_stream_t *s = tx_q_peek();
    if (!s) return;
    s_tx_cur = s;
    s_tx_bit_idx = 0;

    /* Bus must look idle (recessive at the moment we look). A simple
     * software heuristic: read the input pin and only start if it is
     * high. Real CAN requires 3 recessive bits of IFS, which the
     * abort_to_idle path provides after every frame. */
    if (bus_sample() == 0) return;

    /* Mask EXTI before pulling TX low (we'd trigger our own edge). */
    EXTI->IMR &= ~(1U << s_rx_exti_line);

    /* Drive SOF dominant now; schedule first sample. */
    bus_drive_dominant();
    s_tx_sof_us = softcan_now_us();
    TIM0->CNT = 0;
    TIM0->ARR = s_sample_offset_cycles;
    TIM_ClearFlag(TIM0, TIM_IFR_CNTIF);
    TIM_ITConfig (TIM0, TIM_IER_CNTIE, ENABLE);
    s_state = ST_TX_RUN;
}

/* ---------------- Public API ----------------------------------------- */

int softcan_send(const can_frame_t *frame)
{
    if (!frame) return -1;
    int r = tx_q_push(frame);
    if (r == 0) {
        __disable_irq();
        try_start_tx();
        __enable_irq();
    }
    return r;
}

void softcan_get_stats(softcan_stats_t *out) { if (out) *out = s_stats; }

int softcan_init(const softcan_cfg_t *cfg)
{
    if (!cfg) return -1;
    if (cfg->rx_exti_line > 7) return -1;

    s_tx_port = cfg->tx_port; s_tx_pin = cfg->tx_pin;
    s_rx_port = cfg->rx_port; s_rx_pin = cfg->rx_pin;
    s_rx_exti_line = cfg->rx_exti_line;
    s_rx_cb = cfg->rx_cb;
    s_tx_cb = cfg->tx_cb;

    uint32_t br = cfg->bitrate ? cfg->bitrate : SOFTCAN_BITRATE_DEFAULT;

    /* cycles_per_bit = HCLK / bitrate; tq_cycles = bit/8.
     * Effective sample-point delay = SAMPLE_TQ * tq_cycles. */
    uint32_t cyc = SOFTCAN_HCLK_HZ / br;
    if (cyc < 8 || cyc > 0xFFFEU) return -1;
    s_bit_cycles           = (uint16_t)(cyc - 1U);     /* ARR is reload-1 */
    s_sample_offset_cycles = (uint16_t)((cyc * SAMPLE_TQ) / TQ_PER_BIT - 1U);

    /* --- GPIO --- */
    SCU_Unlock();
    if (s_tx_port == GPIO0) SCU_PeriphClockCmd(Periph_GPIO0, ENABLE);
    if (s_tx_port == GPIO1) SCU_PeriphClockCmd(Periph_GPIO1, ENABLE);
    if (s_tx_port == GPIO2) SCU_PeriphClockCmd(Periph_GPIO2, ENABLE);
    if (s_tx_port == GPIO3) SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    if (s_rx_port == GPIO0) SCU_PeriphClockCmd(Periph_GPIO0, ENABLE);
    if (s_rx_port == GPIO1) SCU_PeriphClockCmd(Periph_GPIO1, ENABLE);
    if (s_rx_port == GPIO2) SCU_PeriphClockCmd(Periph_GPIO2, ENABLE);
    if (s_rx_port == GPIO3) SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    SCU_PeriphClockCmd(Periph_TIM0, ENABLE);
    SCU_Lock();
    GPIO_Init(s_tx_port, s_tx_pin, GPIO_MODE_OUTPUT_OD);
    bus_release_recessive();
    GPIO_Init(s_rx_port, s_rx_pin, GPIO_MODE_INPUT_PU);

    /* --- TIM0 (bit timer) — counter mode, IRQ on overflow --- */
    {
        TIM_InitTypeDef t;
        TIM_DeInit(TIM0);
        t.TIM_Mode = TIM_Mode_CNT;
        t.TIM_Prescaler = 0;                /* count HCLK directly */
        t.TIM_Period = s_bit_cycles;
        TIM_Init(TIM0, &t);
        TIM_ClearFlag(TIM0, TIM_IFR_CNTIF);
        /* leave IRQ disabled until we have something to do */
        NVIC_SetPriority(TIM0_IRQn, 0);     /* highest — bit timing must not slip */
        NVIC_EnableIRQ(TIM0_IRQn);
        TIM_Cmd(TIM0, ENABLE);
    }

    /* --- EXTI for SOF falling edge --- */
    {
        uint32_t cfgr = EXTI->CFGR;
        cfgr &= ~(0x7UL << (s_rx_exti_line * 3));
        cfgr |= ((uint32_t)cfg->rx_exti_gpio_sel & 0x7U) << (s_rx_exti_line * 3);
        EXTI->CFGR = cfgr;
        EXTI->FTSR |= (1U << s_rx_exti_line);
        EXTI->RTSR &= ~(1U << s_rx_exti_line);
        EXTI->PR   &= ~(1U << s_rx_exti_line);
        EXTI->IMR  |=  (1U << s_rx_exti_line);
        NVIC_SetPriority((IRQn_Type)(EXTI0_IRQn + s_rx_exti_line), 1);
        NVIC_EnableIRQ ((IRQn_Type)(EXTI0_IRQn + s_rx_exti_line));
    }

    /* --- µs clock --- */
    us_clock_init();

    s_state = ST_IDLE;
    s_stats.err_state = SOFTCAN_ERROR_ACTIVE;
    return 0;
}
