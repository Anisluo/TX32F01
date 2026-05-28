/*
 * fault_dump.c — see fault_dump.h for design.
 */
#include "fault_dump.h"
#include "TX32F01_periph.h"
#include <stddef.h>

#define FR  (*((volatile fault_record_t *)FAULT_REGION_ADDR))

/* Compile-time guard: asm hard-codes that R4..R11 sit at offset 0..28.
 * If anyone reorders the struct, this typedef goes negative-sized and
 * the build fails. */
typedef char _fault_layout_r4_at_0
    [(offsetof(fault_record_t, r4)  == 0  &&
      offsetof(fault_record_t, r11) == 28 &&
      offsetof(fault_record_t, r0)  >= 32) ? 1 : -1];

/* ------------------------------------------------------------------------- */
/*  CRC32 — bitwise, ~600 cycles for the record. No table, ~30 B of code.   */
/* ------------------------------------------------------------------------- */
static uint32_t crc32_step(uint32_t crc, uint32_t word)
{
    int i;
    crc ^= word;
    for (i = 0; i < 32; i++) {
        uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
        crc = (crc >> 1) ^ (0xEDB88320U & mask);
    }
    return crc;
}

/* CRC over fault_record_t starting at schema_version, ending at reserved2. */
static uint32_t fault_record_crc(const volatile fault_record_t *fr)
{
    const volatile uint32_t *p = &fr->schema_version;
    const volatile uint32_t *end = &fr->reserved2 + 1;
    uint32_t c = 0xFFFFFFFFU;
    while (p < end) c = crc32_step(c, *p++);
    return c ^ 0xFFFFFFFFU;
}

/* ------------------------------------------------------------------------- */
/*  Module-tag pointer and persist callback live in regular RW, so they      */
/*  are reset on every cold boot — fine, callers re-register after init().   */
/* ------------------------------------------------------------------------- */
static const char        *s_module_name;     /* set by fault_dump_set_module */
static fault_persist_cb_t s_persist_cb;

void fault_dump_set_module(const char *name)
{
    s_module_name = name;
}

void fault_dump_set_persist_cb(fault_persist_cb_t cb)
{
    s_persist_cb = cb;
}

/* Best-effort copy of the current module name into the record. Truncates
 * to FAULT_MODULE_TAG_LEN-1 and NUL-pads. Safe to call from fault context:
 * uses only volatile reads, no library calls. */
static void snapshot_module_tag(volatile fault_record_t *fr)
{
    const char *src = s_module_name;
    unsigned i;
    if (!src) {
        for (i = 0; i < FAULT_MODULE_TAG_LEN; i++) fr->module_tag[i] = 0;
        return;
    }
    for (i = 0; i < FAULT_MODULE_TAG_LEN - 1U; i++) {
        char c = src[i];
        fr->module_tag[i] = c;
        if (c == 0) break;
    }
    for (; i < FAULT_MODULE_TAG_LEN; i++) fr->module_tag[i] = 0;
}

/* ------------------------------------------------------------------------- */
/*  C side of the HardFault entry. Called by fault_dump_asm.s after R4-R11   */
/*  have already been stored at offset 0..28 of the NOINIT region.           */
/*                                                                           */
/*  Arguments:                                                               */
/*    stacked_sp = MSP or PSP at fault entry, pointing at the HW frame       */
/*    exc_return = value of LR on exception entry (= EXC_RETURN)             */
/* ------------------------------------------------------------------------- */
void fault_dump_c_part(uint32_t *stacked_sp, uint32_t exc_return);

void fault_dump_c_part(uint32_t *stacked_sp, uint32_t exc_return)
{
    volatile fault_record_t *fr = &FR;
    uint32_t sp = (uint32_t)stacked_sp;
    uint32_t flags = 0;
    int i;

    __disable_irq();

    /* Don't trample a previously captured but undrained fault. */
    if (fr->record_magic == FAULT_RECORD_MAGIC) {
        NVIC_SystemReset();
        for (;;) { }
    }

    /* HW stacked frame. */
    if (sp >= 0x20000000UL && sp + 32U <= 0x20001000UL) {
        fr->r0  = stacked_sp[0];
        fr->r1  = stacked_sp[1];
        fr->r2  = stacked_sp[2];
        fr->r3  = stacked_sp[3];
        fr->r12 = stacked_sp[4];
        fr->lr  = stacked_sp[5];
        fr->pc  = stacked_sp[6];
        fr->psr = stacked_sp[7];
    } else {
        /* SP outside SRAM — stack is corrupted or we faulted very early. */
        fr->r0 = fr->r1 = fr->r2 = fr->r3 = 0xDEADBEEFU;
        fr->r12 = fr->lr = fr->pc = fr->psr = 0xDEADBEEFU;
        flags |= FAULT_FLAG_SP_INVALID;
    }

    fr->exc_return  = exc_return;
    fr->sp_at_fault = sp;
    fr->msp         = __get_MSP();
    fr->psp         = __get_PSP();
    fr->icsr        = SCB->ICSR;

    if ((exc_return & 0x4U) != 0U) flags |= FAULT_FLAG_FROM_PSP;
    /* ICSR.RETTOBASE is M3+; on M0 we detect nested by checking if MSP at
     * entry is below the bottom of the regular stack (i.e., we re-entered
     * before unwinding). Cheap heuristic: if EXC_RETURN says we return to
     * handler mode (0xFFFFFFF1), we came from a handler. */
    if ((exc_return & 0xFU) == 0x1U) flags |= FAULT_FLAG_NESTED;
    fr->flags = flags;

    /* Stack walk. */
    for (i = 0; i < 8; i++) {
        uint32_t addr = sp + (uint32_t)i * 4U;
        if (addr >= 0x20000000UL && addr + 4U <= 0x20001000UL) {
            fr->stack_dump[i] = *(volatile uint32_t *)addr;
        } else {
            fr->stack_dump[i] = 0xDEADC0DEU;
        }
    }

    fr->reserved1 = 0;
    fr->reserved2 = 0;
    fr->schema_version = FAULT_SCHEMA_VERSION;
    fr->reset_reason   = 0;  /* filled in by next boot's init() from SCU_RSR */
    snapshot_module_tag(fr);
    fr->fault_count   += 1U;
    fr->record_crc     = fault_record_crc(fr);
    fr->record_magic   = FAULT_RECORD_MAGIC;

    __DSB();
    NVIC_SystemReset();
    for (;;) { }
}

/* ------------------------------------------------------------------------- */
/*  Boot-time init / drain                                                   */
/* ------------------------------------------------------------------------- */

static int s_pending;   /* set by init() if a valid record is sitting */

/* Snapshot and clear SCU_RSR. Safe to call once at boot; flags latch
 * across resets so a 0 here means the reset cause was already cleared by
 * someone else (e.g. the bootloader did it for diagnostics of its own). */
static uint32_t snapshot_reset_reason(void)
{
    uint32_t rsr = SCU->RSR;
    /* Strip RMVF (bit0) — it's the "remove flags" control, not a cause. */
    uint32_t cause = rsr & ~SCU_RSR_RMVF_Msk;
    /* RMVF=1 clears all latched cause bits. SCU is unlock-protected, so
     * wrap with the standard unlock/lock sequence. */
    SCU_Unlock();
    SCU->RSR = SCU_RSR_RMVF_Msk;
    SCU_Lock();
    return cause;
}

int fault_dump_init(void)
{
    volatile fault_record_t *fr = &FR;
    uint32_t rst_cause = snapshot_reset_reason();

    s_pending = 0;

    if (fr->liveness_magic != FAULT_LIVENESS_MAGIC) {
        /* Cold boot — SRAM is uninitialized. Wipe the region. */
        volatile uint32_t *p = (volatile uint32_t *)FAULT_REGION_ADDR;
        int n = (int)(FAULT_REGION_SIZE / 4U);
        while (n--) *p++ = 0;
        fr->liveness_magic = FAULT_LIVENESS_MAGIC;
        fr->boot_count = 1;
        fr->fault_count = 0;
        return 0;
    }

    /* Warm boot. */
    fr->boot_count += 1U;

    /* Stamp the cause of THIS reset onto the pending record (if any)
     * before validating the CRC, so the CRC covers it. We only do this
     * when the record was just produced by our fault handler this cycle:
     * record_magic is set and reset_reason is still the sentinel 0. */
    if (fr->record_magic == FAULT_RECORD_MAGIC && fr->reset_reason == 0U) {
        fr->reset_reason = rst_cause;
        fr->record_crc   = fault_record_crc(fr);
    }

    if (fr->record_magic == FAULT_RECORD_MAGIC &&
        fr->schema_version == FAULT_SCHEMA_VERSION &&
        fr->record_crc == fault_record_crc(fr)) {
        s_pending = 1;
        return 1;
    }

    /* Record present but invalid → discard. */
    fr->record_magic = 0;
    return 0;
}

const fault_record_t *fault_dump_get(void)
{
    return s_pending ? (const fault_record_t *)&FR : (const fault_record_t *)0;
}

void fault_dump_clear(void)
{
    FR.record_magic = 0;
    s_pending = 0;
}

uint32_t fault_dump_boot_count (void) { return FR.boot_count;  }
uint32_t fault_dump_fault_count(void) { return FR.fault_count; }

/* ------------------------------------------------------------------------- */
/*  Pretty-print                                                             */
/* ------------------------------------------------------------------------- */
static void p_str(void (*putc_fn)(char), const char *s)
{
    while (*s) putc_fn(*s++);
}

static void p_hex32(void (*putc_fn)(char), uint32_t v)
{
    static const char H[] = "0123456789ABCDEF";
    int i;
    putc_fn('0'); putc_fn('x');
    for (i = 7; i >= 0; i--) putc_fn(H[(v >> (i * 4)) & 0xFU]);
}

static void p_u32(void (*putc_fn)(char), uint32_t v)
{
    char b[11]; int n = 0;
    if (!v) { putc_fn('0'); return; }
    while (v && n < 10) { b[n++] = (char)('0' + v % 10U); v /= 10U; }
    while (n--) putc_fn(b[n]);
}

static void p_kv(void (*putc_fn)(char), const char *k, uint32_t v)
{
    p_str(putc_fn, "  "); p_str(putc_fn, k); p_str(putc_fn, " = ");
    p_hex32(putc_fn, v);  p_str(putc_fn, "\r\n");
}

static const char *exc_return_meaning(uint32_t er)
{
    /* CM0 EXC_RETURN: 0xFFFFFFF1 = handler→handler (nested),
     *                 0xFFFFFFF9 = thread w/ MSP,
     *                 0xFFFFFFFD = thread w/ PSP. */
    switch (er) {
    case 0xFFFFFFF1U: return "handler-mode (nested)";
    case 0xFFFFFFF9U: return "thread-mode, MSP";
    case 0xFFFFFFFDU: return "thread-mode, PSP";
    default:          return "unknown";
    }
}

void fault_dump_drain_to_uart(void (*putc_fn)(char))
{
    const fault_record_t *fr;
    uint32_t vect;
    int i;

    if (!s_pending || !putc_fn) return;
    fr = (const fault_record_t *)&FR;

    p_str(putc_fn, "\r\n========== HARDFAULT CAPTURED ==========\r\n");
    p_str(putc_fn, "boot_count  = "); p_u32  (putc_fn, fr->boot_count);  p_str(putc_fn, "\r\n");
    p_str(putc_fn, "fault_count = "); p_u32  (putc_fn, fr->fault_count); p_str(putc_fn, "\r\n");

    {   /* module tag, NUL-terminated within the buffer */
        unsigned i;
        p_str(putc_fn, "module      = \"");
        for (i = 0; i < FAULT_MODULE_TAG_LEN && fr->module_tag[i]; i++) {
            char c = fr->module_tag[i];
            putc_fn((c >= 0x20 && c < 0x7F) ? c : '?');
        }
        p_str(putc_fn, "\"\r\n");
    }

    p_str(putc_fn, "reset_rsr   = "); p_hex32(putc_fn, fr->reset_reason);
    if (fr->reset_reason & FAULT_RST_POR)  p_str(putc_fn, " POR");
    if (fr->reset_reason & FAULT_RST_BOR)  p_str(putc_fn, " BOR");
    if (fr->reset_reason & FAULT_RST_PIN)  p_str(putc_fn, " PIN");
    if (fr->reset_reason & FAULT_RST_IWDT) p_str(putc_fn, " IWDT");
    if (fr->reset_reason & FAULT_RST_SOFT) p_str(putc_fn, " SOFT");
    p_str(putc_fn, "\r\n");

    p_str(putc_fn, "flags       = "); p_hex32(putc_fn, fr->flags);
    if (fr->flags & FAULT_FLAG_FROM_PSP)   p_str(putc_fn, " FROM_PSP");
    if (fr->flags & FAULT_FLAG_NESTED)     p_str(putc_fn, " NESTED");
    if (fr->flags & FAULT_FLAG_SP_INVALID) p_str(putc_fn, " SP_INVALID");
    p_str(putc_fn, "\r\n");

    p_str(putc_fn, "exc_return  = "); p_hex32(putc_fn, fr->exc_return);
    p_str(putc_fn, " ("); p_str(putc_fn, exc_return_meaning(fr->exc_return)); p_str(putc_fn, ")\r\n");

    vect = fr->icsr & 0x3FU;
    p_str(putc_fn, "icsr        = "); p_hex32(putc_fn, fr->icsr);
    p_str(putc_fn, "  active_vect = "); p_u32(putc_fn, vect);
    if (vect == 0)      p_str(putc_fn, " (thread)");
    else if (vect == 3) p_str(putc_fn, " (HardFault)");
    else if (vect == 14)p_str(putc_fn, " (PendSV)");
    else if (vect == 15)p_str(putc_fn, " (SysTick)");
    else if (vect >= 16) {
        p_str(putc_fn, " (IRQ#");
        p_u32(putc_fn, vect - 16U);
        p_str(putc_fn, ")");
    }
    p_str(putc_fn, "\r\n");

    p_str(putc_fn, "-- HW stacked frame --\r\n");
    p_kv(putc_fn, "R0 ", fr->r0);
    p_kv(putc_fn, "R1 ", fr->r1);
    p_kv(putc_fn, "R2 ", fr->r2);
    p_kv(putc_fn, "R3 ", fr->r3);
    p_kv(putc_fn, "R12", fr->r12);
    p_kv(putc_fn, "LR ", fr->lr);
    p_kv(putc_fn, "PC ", fr->pc);
    p_kv(putc_fn, "PSR", fr->psr);

    p_str(putc_fn, "-- callee-saved (R4-R11) --\r\n");
    p_kv(putc_fn, "R4 ", fr->r4);  p_kv(putc_fn, "R5 ", fr->r5);
    p_kv(putc_fn, "R6 ", fr->r6);  p_kv(putc_fn, "R7 ", fr->r7);
    p_kv(putc_fn, "R8 ", fr->r8);  p_kv(putc_fn, "R9 ", fr->r9);
    p_kv(putc_fn, "R10", fr->r10); p_kv(putc_fn, "R11", fr->r11);

    p_str(putc_fn, "-- stacks --\r\n");
    p_kv(putc_fn, "SP@fault", fr->sp_at_fault);
    p_kv(putc_fn, "MSP     ", fr->msp);
    p_kv(putc_fn, "PSP     ", fr->psp);

    p_str(putc_fn, "-- stack dump (sp..+32) --\r\n");
    for (i = 0; i < 8; i++) {
        p_str(putc_fn, "  [+"); p_u32(putc_fn, (uint32_t)i * 4U); p_str(putc_fn, "] ");
        p_hex32(putc_fn, fr->stack_dump[i]);
        p_str(putc_fn, "\r\n");
    }

    p_str(putc_fn, "Decode hint: arm-none-eabi-addr2line -e <app.elf> -fpC ");
    p_hex32(putc_fn, fr->pc); p_str(putc_fn, "\r\n");
    p_str(putc_fn, "========================================\r\n\r\n");

    /* Hand the record to the optional persistence sink (SPI NOR / Flash)
     * BEFORE clearing — once cleared the next boot won't see it. The
     * callback gets a const snapshot pointer; it must not block on
     * interrupts that haven't been re-enabled yet by the caller. */
    if (s_persist_cb) s_persist_cb(fr);

    fault_dump_clear();
}

/* ------------------------------------------------------------------------- */
/*  Test injection                                                            */
/* ------------------------------------------------------------------------- */
void fault_dump_trigger_test(fault_test_kind_t kind)
{
    typedef void (*vfn_t)(void);
    volatile uint32_t sink;
    (void)sink;

    switch (kind) {
    case FAULT_TEST_NULL_FN: {
        vfn_t f = (vfn_t)0;
        f();
        break;
    }
    case FAULT_TEST_BAD_LDR: {
        /* 0xE0000004 is reserved PPB on Cortex-M0 — accessing it asserts
         * a bus error → HardFault. */
        sink = *(volatile uint32_t *)0xE0000004UL;
        break;
    }
    case FAULT_TEST_INVALID_PC:
    default: {
        /* 0xFFFFFFFE has bit0=0 → invalid Thumb target → HardFault on BX. */
        vfn_t f = (vfn_t)0xFFFFFFFEUL;
        f();
        break;
    }
    }

    /* If somehow we return, spin so we don't silently continue. */
    for (;;) { }
}
