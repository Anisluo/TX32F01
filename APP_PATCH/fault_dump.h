/*
 * fault_dump.h — Cortex-M0 HardFault diagnostic framework for TX32F01.
 *
 * Design
 * ------
 *   On HardFault, the asm trampoline saves R4-R11 to the front of a NOINIT
 *   SRAM region (so asm only needs the base address — no offset math), then
 *   calls a C handler that writes the rest of the fault context (HW stacked
 *   frame, EXC_RETURN, MSP/PSP, ICSR, stack dump) and triggers a software
 *   reset. After reset, fault_dump_init() validates the region and, if a
 *   fault was captured, makes it available for drain via
 *   fault_dump_drain_to_uart() or fault_dump_get().
 *
 * Cortex-M0 caveats (honest limits of what we can capture)
 * --------------------------------------------------------
 *   - No CFSR/HFSR/MMFAR/BFAR: cannot classify BusFault / UsageFault /
 *     MemFault — every fault arrives as HardFault. We capture the stacked
 *     PC and disassemble offline.
 *   - No DWT: cannot timestamp the fault in cycles.
 *   - No VTOR: this APP receives the fault via the BL's HardFault trampoline
 *     that forwards through the SRAM soft-vector table at 0x20000004
 *     (APP_SVEC_HARDFAULT). The BL trampoline is assumed to be a minimal
 *     LDR/BX pair that preserves R4-R11 and LR=EXC_RETURN. If the BL is
 *     ever changed to clobber R4-R11, those fields become unreliable —
 *     the rest of the record is still valid.
 *
 * Memory layout
 * -------------
 *   The NOINIT region lives at 0x20000F00..0x20000FFF (256 B). The APP
 *   scatter must shrink RW_* by 0x100 to make room. Asm writes R4..R11
 *   starting at FAULT_REGION_ADDR (no struct offsets in asm).
 *
 * Wiring
 * ------
 *   1. Shrink scatter:    RW_LOG ... 0x00000EA0
 *   2. Register handler:  app_softvec_register_sys(APP_SVEC_HARDFAULT,
 *                                                 fault_dump_hardfault_entry);
 *   3. On boot:           fault_dump_init();
 *                         fault_dump_drain_to_uart(uart_putc);
 *   4. (optional shell)   add an 'f' command that calls
 *                         fault_dump_trigger_test(FAULT_TEST_INVALID_PC).
 */
#ifndef _FAULT_DUMP_H
#define _FAULT_DUMP_H

#include <stdint.h>

#define FAULT_REGION_ADDR     0x20000F00UL
#define FAULT_REGION_SIZE     256U

#define FAULT_LIVENESS_MAGIC  0xC0FFEEEEUL   /* set on first fault_dump_init() this power-cycle */
#define FAULT_RECORD_MAGIC    0xFA17B007UL   /* set when a fault is captured and waiting to drain */
#define FAULT_SCHEMA_VERSION  2U             /* v2: + module_tag, reset_reason populated */

#define FAULT_MODULE_TAG_LEN  8U             /* including terminator; first 7 chars usable */

/* flags */
#define FAULT_FLAG_FROM_PSP   (1U << 0)
#define FAULT_FLAG_NESTED     (1U << 1)
#define FAULT_FLAG_SP_INVALID (1U << 2)

/* reset_reason bit layout — mirrors SCU_RSR layout so the raw value is
 * preserved (bit0 = RMVF stays 0 in our snapshot). */
#define FAULT_RST_POR         (1U << 1)
#define FAULT_RST_BOR         (1U << 2)
#define FAULT_RST_PIN         (1U << 3)
#define FAULT_RST_IWDT        (1U << 4)
#define FAULT_RST_SOFT        (1U << 5)

typedef enum {
    FAULT_TEST_INVALID_PC = 0,   /* jump to 0xFFFFFFFE — guaranteed HardFault */
    FAULT_TEST_NULL_FN    = 1,   /* call (void(*)(void))0 */
    FAULT_TEST_BAD_LDR    = 2    /* read 0xE0000004 (reserved PPB on M0) */
} fault_test_kind_t;

/*
 * Layout note: R4..R11 sit at offset 0..28 so the asm trampoline can simply
 * STMIA the base address. Everything else follows.
 */
typedef struct {
    /* === asm-written (offset 0..31) === */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;

    /* === liveness header (preserved across warm reset; zeroed on cold boot) === */
    uint32_t liveness_magic;     /* FAULT_LIVENESS_MAGIC once init() has run */
    uint32_t boot_count;         /* +1 every fault_dump_init() */
    uint32_t fault_count;        /* +1 every captured fault */
    uint32_t reserved0;

    /* === record header (valid iff record_magic == FAULT_RECORD_MAGIC) === */
    uint32_t record_magic;
    uint32_t record_crc;         /* CRC32 over [schema_version .. end_of_record] */
    uint32_t schema_version;
    uint32_t reset_reason;       /* SCU_RSR at init() (0 if unsupported) */
    uint32_t flags;
    uint32_t reserved1;

    /* === HW-stacked frame (set by C handler from the offending stack) === */
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;

    /* === handler-mode state === */
    uint32_t exc_return;
    uint32_t sp_at_fault;
    uint32_t msp;
    uint32_t psp;
    uint32_t icsr;

    /* === stack walk: 8 words from sp_at_fault upward === */
    uint32_t stack_dump[8];

    /* === v2 additions === */
    char     module_tag[FAULT_MODULE_TAG_LEN];  /* current fault_dump_set_module() snapshot, NUL-padded */
    uint32_t reserved2;
} fault_record_t;

/* ------------------------------------------------------------------------- */
/*  Public API                                                               */
/* ------------------------------------------------------------------------- */

/* Call once, early in main() (after UART up if you plan to drain). Detects
 * cold vs warm boot, validates any pending fault record, bumps boot_count.
 * Returns 1 if a valid fault record is pending, 0 otherwise. */
int fault_dump_init(void);

/* Pending record, or NULL. Pointer stays valid until fault_dump_clear(). */
const fault_record_t *fault_dump_get(void);

/* Mark the pending record as drained (clears record_magic). Idempotent. */
void fault_dump_clear(void);

/* Pretty-print the pending record via putc_fn and auto-clear. No-op if
 * nothing is pending. Synchronous; safe to call from main(). */
void fault_dump_drain_to_uart(void (*putc_fn)(char));

uint32_t fault_dump_boot_count (void);
uint32_t fault_dump_fault_count(void);

/* Tag the "current module" for the next fault. Cheap, IRQ-safe — just
 * stores a pointer. On fault entry the C handler snapshots up to 7 chars
 * into the record. Pass NULL to clear. Typical use:
 *
 *   fault_dump_set_module("foc_isr");  // before entering FOC ISR
 *   ...
 *   fault_dump_set_module("main");     // back in main loop
 */
void fault_dump_set_module(const char *name);

/* Optional persistence hook — called by fault_dump_drain_to_uart() with
 * the pending record just before it is cleared. Lets the application
 * write the record to SPI NOR / internal Flash for post-mortem retrieval
 * after a power cycle. Pass NULL to disable. The callback must NOT
 * trigger another fault. */
typedef void (*fault_persist_cb_t)(const fault_record_t *fr);
void fault_dump_set_persist_cb(fault_persist_cb_t cb);

/* Register via app_softvec_register_sys(APP_SVEC_HARDFAULT, ...). */
extern void fault_dump_hardfault_entry(void);

/* Deliberately crash. Does not return. */
void fault_dump_trigger_test(fault_test_kind_t kind);

#endif
