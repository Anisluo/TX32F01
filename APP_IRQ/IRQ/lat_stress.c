#include "lat_stress.h"
#include "TX32F01_periph.h"

static stress_t s_mode = STRESS_NONE;

void     lat_stress_set(stress_t s) { if (s < STRESS_COUNT) s_mode = s; }
stress_t lat_stress_get(void)       { return s_mode; }

const char *lat_stress_name(stress_t s)
{
    switch (s) {
    case STRESS_NONE:      return "NONE (wfi)";
    case STRESS_BUSY:      return "BUSY (tight spin)";
    case STRESS_CRIT_64:   return "CRIT_64";
    case STRESS_CRIT_256:  return "CRIT_256";
    case STRESS_CRIT_1024: return "CRIT_1024";
    default:               return "??";
    }
}

/* Pure-asm cycle burner.
 *   Cortex-M0: SUBS=1, BNE_taken=3, BNE_not_taken=1, BX_lr=3.
 *   Total = 4*n - 2 + BX(3) + call BL(3) ≈ 4*n + 4 cycles.
 * The function name is the entry label; the BNE branches back to SUBS. */
__asm static void burn_n(uint32_t n)
{
    SUBS r0, r0, #1
    BNE  burn_n
    BX   lr
}

/* Hold PRIMASK for ~cycles, then drop briefly so the pending SysTick
   has a window to fire. The brief gap makes the histogram bimodal:
     - a peak near baseline (IRQ landed in the gap)
     - a peak / shelf out to ~cycles + baseline (IRQ landed inside the
       critical section and waited for it to end)
   That bimodality is the whole point — it visualizes
   max_blocking_latency = critical_section_length. */
static void crit_hold(uint32_t cycles)
{
    __disable_irq();
    burn_n(cycles >> 2);                  /* 4 cyc/iter */
    __enable_irq();
    /* ~6 cycles unmasked window before we re-enter — enough for one pending
       SysTick to be serviced if it arrived while PRIMASK was held. */
    __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP();
}

void lat_stress_step(void)
{
    switch (s_mode) {
    case STRESS_NONE:
        /* nothing — scheduler returns and falls into WFI */
        break;
    case STRESS_BUSY:
        burn_n(256U);                     /* ~1024 cyc, never disables IRQs */
        break;
    case STRESS_CRIT_64:
        crit_hold(64U);
        break;
    case STRESS_CRIT_256:
        crit_hold(256U);
        break;
    case STRESS_CRIT_1024:
        crit_hold(1024U);
        break;
    default:
        break;
    }
}
