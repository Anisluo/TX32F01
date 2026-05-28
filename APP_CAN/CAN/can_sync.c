/*
 * can_sync.c — see header for protocol description.
 *
 * The master half is trivial: a 1-ms tick scheduler emits SYNC; on the
 * TX-complete callback for SYNC, the SoftCAN driver hands us the SOF
 * timestamp; we then queue an FUP carrying that timestamp.
 *
 * The slave half maintains:
 *   anchor_local_us   - local µs at last accepted SYNC SOF
 *   anchor_virtual_us - corresponding master µs
 *   skew_ppm          - LPF of fractional drift between consecutive syncs
 *
 * Algorithm:
 *   Let dT_l = local_T - prev_local_T
 *       dT_m = master_T - prev_master_T
 *   instantaneous_skew_ppm = (dT_m - dT_l) * 1e6 / dT_l
 *   skew_ppm = (1 - α) * skew_ppm + α * instantaneous_skew_ppm
 *
 * For α = 1/8 we get good rejection of bus-jitter while still tracking
 * real drift of ~1000 ppm crystal mismatch within a few seconds.
 */

#include "can_sync.h"
#include "softcan.h"
#include <string.h>

static can_sync_cfg_t s_cfg;

/* --- master-side state --- */
static uint8_t  s_sync_counter;
static uint16_t s_ms_since_sync;
static uint8_t  s_pending_fup;            /* set in TX-cb, consumed in tick */
static uint32_t s_pending_fup_sof_us;
static uint8_t  s_pending_fup_counter;

/* --- slave-side state --- */
static uint8_t  s_have_last_sync_local;
static uint32_t s_last_sync_local_us;
static uint8_t  s_last_sync_counter;

static uint8_t  s_have_anchor;
static uint32_t s_anchor_local_us;
static uint64_t s_anchor_virtual_us;
static int32_t  s_skew_ppm;                /* signed */
static uint8_t  s_have_prev;
static uint32_t s_prev_local_us;
static uint64_t s_prev_virtual_us;

static int32_t  s_last_offset_us;
static uint32_t s_updates;

#define SKEW_ALPHA_NUM   1
#define SKEW_ALPHA_DEN   8

/* ---------------- master TX path ------------------------------------ */

static void emit_sync(void)
{
    can_frame_t f;
    f.id  = CAN_SYNC_ID;
    f.rtr = 0;
    f.dlc = 1;
    f.data[0] = ++s_sync_counter;
    (void)softcan_send(&f);
}

static void emit_fup(uint32_t sof_us, uint8_t counter)
{
    can_frame_t f;
    f.id  = CAN_FUP_ID;
    f.rtr = 0;
    f.dlc = 8;
    f.data[0] = (uint8_t)(sof_us      );
    f.data[1] = (uint8_t)(sof_us >>  8);
    f.data[2] = (uint8_t)(sof_us >> 16);
    f.data[3] = (uint8_t)(sof_us >> 24);
    f.data[4] = counter;
    f.data[5] = 0; f.data[6] = 0; f.data[7] = 0;
    (void)softcan_send(&f);
}

void can_sync_on_tx(uint16_t id, uint32_t sof_local_us)
{
    if (!s_cfg.is_master) return;
    if (id == CAN_SYNC_ID) {
        s_pending_fup_sof_us = sof_local_us;
        s_pending_fup_counter = s_sync_counter;
        s_pending_fup = 1;
    }
}

/* ---------------- slave RX path ------------------------------------- */

static int64_t s64_abs(int64_t v) { return v < 0 ? -v : v; }

static void slave_apply_pair(uint32_t local_us, uint32_t master_us)
{
    /* First-time anchor: jump-set, no skew yet. */
    if (!s_have_anchor) {
        s_anchor_local_us   = local_us;
        s_anchor_virtual_us = master_us;
        s_have_anchor = 1;
        s_have_prev   = 1;
        s_prev_local_us   = local_us;
        s_prev_virtual_us = master_us;
        s_last_offset_us  = 0;
        s_updates = 1;
        return;
    }

    /* Compute predicted virtual at this new local from current model. */
    uint32_t dlocal_anchor = local_us - s_anchor_local_us;       /* µs */
    int64_t  drift = ((int64_t)dlocal_anchor * s_skew_ppm) / 1000000;
    int64_t  predicted_virtual = (int64_t)s_anchor_virtual_us +
                                 (int64_t)dlocal_anchor + drift;
    int64_t  err = (int64_t)master_us - predicted_virtual;
    s_last_offset_us = (int32_t)(err > 1000000 ? 1000000 :
                                 err < -1000000 ? -1000000 : err);

    /* Skew update from inter-sync drift. */
    if (s_have_prev) {
        uint32_t dT_l = local_us  - s_prev_local_us;
        uint32_t dT_m = (uint32_t)(master_us - (uint32_t)s_prev_virtual_us);
        if (dT_l > 1000) {           /* require ≥1 ms to avoid noise blow-up */
            int64_t inst_ppm = ((int64_t)dT_m - (int64_t)dT_l)
                               * 1000000 / (int64_t)dT_l;
            /* LPF: skew += α * (inst - skew) */
            int64_t delta = (inst_ppm - (int64_t)s_skew_ppm)
                          * SKEW_ALPHA_NUM / SKEW_ALPHA_DEN;
            int64_t new_skew = (int64_t)s_skew_ppm + delta;
            if (new_skew >  100000) new_skew =  100000;   /* clamp to ±10 % */
            if (new_skew < -100000) new_skew = -100000;
            s_skew_ppm = (int32_t)new_skew;
        }
    }

    /* Re-anchor virtual clock to the freshly-known truth. */
    s_anchor_local_us   = local_us;
    s_anchor_virtual_us = master_us;

    s_prev_local_us   = local_us;
    s_prev_virtual_us = master_us;
    s_have_prev = 1;
    s_updates++;
}

void can_sync_on_rx(uint16_t id, const uint8_t *data, uint8_t dlc,
                    uint32_t sof_local_us)
{
    if (s_cfg.is_master) return;             /* master ignores its own kind */

    if (id == CAN_SYNC_ID && dlc >= 1) {
        s_last_sync_local_us = sof_local_us;
        s_last_sync_counter  = data[0];
        s_have_last_sync_local = 1;
        return;
    }
    if (id == CAN_FUP_ID && dlc >= 5 && s_have_last_sync_local) {
        if (data[4] != s_last_sync_counter) {
            /* SYNC/FUP mismatch — out of order or dropped frame; drop. */
            s_have_last_sync_local = 0;
            return;
        }
        uint32_t master_us = (uint32_t)data[0]
                           | ((uint32_t)data[1] <<  8)
                           | ((uint32_t)data[2] << 16)
                           | ((uint32_t)data[3] << 24);
        slave_apply_pair(s_last_sync_local_us, master_us);
        s_have_last_sync_local = 0;
        (void)s64_abs;
    }
}

/* ---------------- public ------------------------------------------- */

void can_sync_init(const can_sync_cfg_t *cfg)
{
    s_cfg = *cfg;
    if (s_cfg.sync_period_ms == 0) s_cfg.sync_period_ms = 100;
    s_sync_counter = 0;
    s_ms_since_sync = 0;
    s_pending_fup = 0;
    s_have_last_sync_local = 0;
    s_have_anchor = 0;
    s_have_prev   = 0;
    s_skew_ppm    = 0;
    s_updates     = 0;
    s_last_offset_us = 0;
}

void can_sync_tick_ms(void)
{
    if (s_cfg.is_master) {
        /* Emit FUP first (we want it to follow SYNC on the bus, but
         * SoftCAN serializes — pushing FUP right after the SYNC TX
         * callback runs keeps the pair adjacent). */
        if (s_pending_fup) {
            s_pending_fup = 0;
            emit_fup(s_pending_fup_sof_us, s_pending_fup_counter);
        }
        s_ms_since_sync++;
        if (s_ms_since_sync >= s_cfg.sync_period_ms) {
            s_ms_since_sync = 0;
            emit_sync();
        }
    }
}

uint64_t can_sync_virtual_us(void)
{
    if (s_cfg.is_master) return (uint64_t)softcan_now_us();
    /* Snapshot the slave clock model atomically — on_rx ISR may update
     * all three fields together. */
    __disable_irq();
    uint8_t  have   = s_have_anchor;
    uint32_t alocal = s_anchor_local_us;
    uint64_t avirt  = s_anchor_virtual_us;
    int32_t  skew   = s_skew_ppm;
    __enable_irq();
    uint32_t now_local = softcan_now_us();
    if (!have) return (uint64_t)now_local;
    uint32_t dl = now_local - alocal;
    int64_t  drift = ((int64_t)dl * skew) / 1000000;
    return avirt + (uint64_t)dl + (uint64_t)drift;
}

int32_t  can_sync_last_offset_us(void) { return s_last_offset_us; }
int32_t  can_sync_skew_ppm     (void) { return s_skew_ppm; }
uint32_t can_sync_updates      (void) { return s_updates; }
