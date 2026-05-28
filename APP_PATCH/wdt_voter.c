/*
 * wdt_voter.c — see wdt_voter.h
 *
 * Implementation choices
 * ----------------------
 *   - Fixed-size static table (WDT_MAX_MODULES). No malloc. 4 KB SRAM
 *     is too tight to risk dynamic allocation in safety code.
 *   - All counters are uint16_t (max 65 s with 1 ms tick). For a single
 *     module deadline you'd never go past a few seconds before panic.
 *   - wdt_checkin does ONE write per call. It's safe to call from ISR.
 *   - wdt_voter_tick uses local copies of the per-module counters so
 *     a concurrent wdt_checkin (from a higher-priority ISR) doesn't
 *     race with the comparison.
 *
 * IWDT clock model
 * ----------------
 *   The TX32F01 IWDT counts down at LSI / prescaler. LSI nominally 32 kHz
 *   (see datasheet — exact value varies ±30%, so we treat this as a
 *   coarse outer bound, not a precise timeout). Reload max is 12 bits
 *   on most LSI-derived watchdogs of this class.
 *
 *   We pick:
 *     ms = 1000 → PR=4   (8 kHz), reload = 8 * ms ≤ 4095
 *     ms ≤ 512 → PR=8    use reload accordingly
 *     larger   → PR=256  with reload = ms * 32 / 256 = ms / 8
 *
 *   Conservative: assume LSI = 32 kHz, accept ±30% real-world drift.
 */
#include "wdt_voter.h"
#include "fault_dump.h"
#include "TX32F01_periph.h"     /* brings in HAL_IWDT.h */

/* ------------------------------------------------------------------------- */
/*  Module table                                                             */
/* ------------------------------------------------------------------------- */
typedef struct {
    const char *name;
    uint16_t    deadline_ms;
    uint16_t    ms_since;        /* +elapsed in wdt_voter_tick, reset by wdt_checkin */
    uint32_t    breadcrumb;
} wdt_slot_t;

static wdt_slot_t s_mods[WDT_MAX_MODULES];
static uint8_t    s_count;
static uint8_t    s_armed;       /* IWDT enabled */

/* ------------------------------------------------------------------------- */
/*  IWDT timeout selection                                                   */
/* ------------------------------------------------------------------------- */
static void iwdt_arm(uint16_t timeout_ms)
{
    /* Target: timeout_ms = reload * prescaler / LSI_freq
     * Assume LSI ≈ 32 kHz. Solve for reload after picking prescaler.
     * Keep reload ≤ 4095. */
    uint8_t  pr;
    uint32_t reload;

    /* Pick the smallest prescaler that keeps reload ≤ 4095. */
    static const struct { uint8_t code; uint16_t divisor; }
        tbl[] = {
            { IWDT_PR_4,   4   },
            { IWDT_PR_8,   8   },
            { IWDT_PR_16,  16  },
            { IWDT_PR_32,  32  },
            { IWDT_PR_64,  64  },
            { IWDT_PR_128, 128 },
            { IWDT_PR_256, 256 },
        };
    pr = IWDT_PR_256; reload = 4095;
    for (unsigned i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
        /* counts = LSI / divisor   →   counts_per_ms = 32 / divisor   */
        uint32_t cnt = (uint32_t)timeout_ms * 32U / tbl[i].divisor;
        if (cnt <= 4095U) {
            pr = tbl[i].code;
            reload = cnt ? cnt : 1U;
            break;
        }
    }

    /* IWDT runs off LSI; no peripheral clock enable needed. The
     * existing demo (8.IWDT/1.IWDT) confirms this — IWDT_Init alone is
     * enough. */
    IWDT_Init(pr, (uint16_t)reload);
    IWDT_CmdEnable();
    IWDT_ReloadCounter();
}

/* ------------------------------------------------------------------------- */
/*  Init                                                                     */
/* ------------------------------------------------------------------------- */
void wdt_voter_init(uint16_t iwdt_timeout_ms)
{
    s_count = 0;
    for (unsigned i = 0; i < WDT_MAX_MODULES; i++) {
        s_mods[i].name        = 0;
        s_mods[i].deadline_ms = 0;
        s_mods[i].ms_since    = 0;
        s_mods[i].breadcrumb  = 0;
    }
    iwdt_arm(iwdt_timeout_ms);
    s_armed = 1;
}

/* ------------------------------------------------------------------------- */
/*  Register / checkin                                                       */
/* ------------------------------------------------------------------------- */
wdt_token_t wdt_register(const char *name, uint16_t deadline_ms)
{
    if (!name) return -1;
    if (s_count >= WDT_MAX_MODULES) return -1;

    uint8_t i = s_count++;
    s_mods[i].name        = name;
    s_mods[i].deadline_ms = deadline_ms;
    s_mods[i].ms_since    = 0;
    s_mods[i].breadcrumb  = 0;
    return (wdt_token_t)i;
}

void wdt_checkin(wdt_token_t tok)
{
    if ((uint8_t)tok >= s_count) return;          /* covers -1 and out-of-range */
    s_mods[tok].ms_since = 0;                     /* single store — IRQ-safe */
}

void wdt_set_breadcrumb(wdt_token_t tok, uint32_t crumb)
{
    if ((uint8_t)tok >= s_count) return;
    s_mods[tok].breadcrumb = crumb;
}

/* ------------------------------------------------------------------------- */
/*  Panic — name the stalled module in fault_dump, then deliberately fault.  */
/* ------------------------------------------------------------------------- */
static void wdt_panic(const char *modname)
{
    /* Build "wdt:NAME" into a tiny buffer (fault_dump_set_module just
     * stores the pointer; we need our string to outlive the call until
     * the fault handler snapshots it). */
    static char tag_buf[FAULT_MODULE_TAG_LEN];
    const char *p = "wdt:";
    unsigned i = 0;
    while (*p && i < FAULT_MODULE_TAG_LEN - 1U) tag_buf[i++] = *p++;
    if (modname) {
        while (*modname && i < FAULT_MODULE_TAG_LEN - 1U) tag_buf[i++] = *modname++;
    }
    tag_buf[i] = 0;

    fault_dump_set_module(tag_buf);

    /* Trigger a guaranteed HardFault. The fault_dump asm trampoline
     * records callee-saved regs and the C handler records the stacked
     * frame + resets. Stack dump field of the record will point at
     * fault_dump_trigger_test's call site, NOT the original stall — the
     * module name in the tag is the actionable bit. */
    fault_dump_trigger_test(FAULT_TEST_INVALID_PC);

    /* Defence in depth — if for any reason the trigger doesn't fault
     * (e.g. fault_dump's softvec entry isn't wired), just reset. */
    NVIC_SystemReset();
    for (;;) { }
}

/* ------------------------------------------------------------------------- */
/*  Tick                                                                     */
/* ------------------------------------------------------------------------- */
void wdt_voter_tick(uint16_t elapsed_ms)
{
    if (!s_armed) return;

    uint8_t count = s_count;
    for (uint8_t i = 0; i < count; i++) {
        /* Snapshot to avoid racing with a concurrent wdt_checkin. */
        uint16_t since   = s_mods[i].ms_since;
        uint16_t newval  = since + elapsed_ms;
        /* Saturate so a stuck-for-minutes module doesn't wrap quietly. */
        if (newval < since) newval = 0xFFFFu;
        s_mods[i].ms_since = newval;

        if (newval > s_mods[i].deadline_ms) {
            wdt_panic(s_mods[i].name);
            /* unreachable */
        }
    }

    /* All modules alive → feed the hardware dog. */
    IWDT_ReloadCounter();
}

/* ------------------------------------------------------------------------- */
/*  Introspection                                                            */
/* ------------------------------------------------------------------------- */
uint16_t wdt_ms_since(wdt_token_t tok)
{
    if ((uint8_t)tok >= s_count) return 0;
    return s_mods[tok].ms_since;
}

uint32_t wdt_breadcrumb(wdt_token_t tok)
{
    if ((uint8_t)tok >= s_count) return 0;
    return s_mods[tok].breadcrumb;
}

const char *wdt_name(wdt_token_t tok)
{
    if ((uint8_t)tok >= s_count) return 0;
    return s_mods[tok].name;
}

uint8_t wdt_module_count(void) { return s_count; }
