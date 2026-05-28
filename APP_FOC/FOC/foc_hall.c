#include "foc_hall.h"
#include "TX32F01_periph.h"
#include "app_softvec.h"

/* --- EXTI register macros (mirrored from demo lib so we don't depend on
 *     anything outside the APP_FOC tree) -------------------------------- */
#define EXTI_GPIO_CFG(line, port_id)                                   \
    do { EXTI->CFGR &= ~((uint32_t)0x7U << ((line) * 3U));             \
         EXTI->CFGR |=  ((uint32_t)(port_id) << ((line) * 3U)); } while (0)
#define EXTI_RISE_EN(line)       (EXTI->RTSR |= (uint32_t)1U << (line))
#define EXTI_FALL_EN(line)       (EXTI->FTSR |= (uint32_t)1U << (line))
#define EXTI_IT_EN(line)         (EXTI->IMR  |= (uint32_t)1U << (line))
#define EXTI_CLR_PR(line)        (EXTI->PR   &= ~((uint32_t)1U << (line)))

#define GPIO_PORTID(p) (((p) == GPIO0) ? 0U : ((p) == GPIO1) ? 1U :    \
                       ((p) == GPIO2) ? 2U : 3U)

/* --- Sector tables -------------------------------------------------------
 *
 * Hall code CBA in [1..6] → sector index in [0..5].  Codes 0 and 7 are
 * invalid (open / shorted sensor); we keep the last good sector when we
 * see them, but mark a fault flag for the shell.
 *
 * Sector center θ (Q16 uint16, 0..65535 ≡ 0..2π):
 *
 *      sec   center        deg
 *       0    65535*30/360   ≈  5461       (0x1556)
 *       1    65535*90/360   ≈ 16384       (0x4000)
 *       2    65535*150/360  ≈ 27307       (0x6AAA)
 *       3    65535*210/360  ≈ 38229       (0x9555)
 *       4    65535*270/360  ≈ 49152       (0xC000)
 *       5    65535*330/360  ≈ 60075       (0xEAAA)
 *
 * This mapping is for a typical 120° hall arrangement.  Swap rows if
 * your motor was wound the other way.  At first power-on the user
 * should spin the rotor by hand and confirm `hall` shell command shows
 * 1→3→2→6→4→5→1→... or its reverse.
 */
static const uint16_t s_sector_center[8] = {
    0,           /* 000 invalid */
    16384,       /* 001 → 90°   */
    38229,       /* 010 → 210°  */
    27307,       /* 011 → 150°  */
    49152,       /* 100 → 270°  */
     5461,       /* 101 → 30°   */
    60075,       /* 110 → 330°  */
    0,           /* 111 invalid */
};

/* expected next code for forward / backward rotation, indexed by current */
static const uint8_t s_next_fwd[8] = { 0, 3, 6, 2, 5, 1, 4, 0 };
static const uint8_t s_next_rev[8] = { 0, 5, 3, 1, 6, 4, 2, 0 };

/* --- State -------------------------------------------------------------- */
static volatile uint16_t s_angle;            /* electrical, Q16 */
static volatile uint8_t  s_hall_code;        /* last valid hall code 1..6 */
static volatile int8_t   s_dir;              /* +1 forward, -1 reverse */
static volatile uint32_t s_tick;             /* PWM-period counter */
static volatile uint32_t s_last_edge_tick;
static volatile uint32_t s_last_period;
static volatile uint16_t s_fault_cnt;

/* foc_loop.c bumps this every PWM period so the Hall driver has a time base */
void foc_hall_tick(void)  { s_tick++; }

static uint8_t read_hall_code(void)
{
    uint8_t a = GPIO_ReadInputDataBit(FOC_HALL_A_PORT, FOC_HALL_A_PIN) & 1U;
    uint8_t b = GPIO_ReadInputDataBit(FOC_HALL_B_PORT, FOC_HALL_B_PIN) & 1U;
    uint8_t c = GPIO_ReadInputDataBit(FOC_HALL_C_PORT, FOC_HALL_C_PIN) & 1U;
    return (uint8_t)((c << 2) | (b << 1) | a);
}

static void hall_isr(void)
{
    /* Clear all three lines unconditionally — we routed all hall pins
     * through hall_isr() so we don't care which one fired. */
    EXTI_CLR_PR(FOC_HALL_A_EXTI);
    EXTI_CLR_PR(FOC_HALL_B_EXTI);
    EXTI_CLR_PR(FOC_HALL_C_EXTI);

    uint8_t code = read_hall_code();
    if (code == 0U || code == 7U) {
        s_fault_cnt++;
        return;                       /* keep previous angle */
    }

    /* Direction detection by comparing to the expected next-state map. */
    if (code == s_next_fwd[s_hall_code])      s_dir =  1;
    else if (code == s_next_rev[s_hall_code]) s_dir = -1;
    /* else: jumped by >1 sector → keep previous direction */

    /* Snap the angle to the new sector center. */
    s_angle      = s_sector_center[code];
    s_hall_code  = code;

    /* Period between commutations → ω.  Integer ticks of PWM period. */
    uint32_t now = s_tick;
    uint32_t dt  = now - s_last_edge_tick;
    s_last_edge_tick = now;
    if (dt > 0 && dt < 0x40000000U) s_last_period = dt;
}

int foc_hall_init(void)
{
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO2, ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    SCU_Lock();

    GPIO_Init(FOC_HALL_A_PORT, FOC_HALL_A_PIN, GPIO_MODE_INPUT_PU);
    GPIO_Init(FOC_HALL_B_PORT, FOC_HALL_B_PIN, GPIO_MODE_INPUT_PU);
    GPIO_Init(FOC_HALL_C_PORT, FOC_HALL_C_PIN, GPIO_MODE_INPUT_PU);

    EXTI_GPIO_CFG(FOC_HALL_A_EXTI, GPIO_PORTID(FOC_HALL_A_PORT));
    EXTI_GPIO_CFG(FOC_HALL_B_EXTI, GPIO_PORTID(FOC_HALL_B_PORT));
    EXTI_GPIO_CFG(FOC_HALL_C_EXTI, GPIO_PORTID(FOC_HALL_C_PORT));

    EXTI_RISE_EN(FOC_HALL_A_EXTI); EXTI_FALL_EN(FOC_HALL_A_EXTI);
    EXTI_RISE_EN(FOC_HALL_B_EXTI); EXTI_FALL_EN(FOC_HALL_B_EXTI);
    EXTI_RISE_EN(FOC_HALL_C_EXTI); EXTI_FALL_EN(FOC_HALL_C_EXTI);

    EXTI_IT_EN(FOC_HALL_A_EXTI);
    EXTI_IT_EN(FOC_HALL_B_EXTI);
    EXTI_IT_EN(FOC_HALL_C_EXTI);

    EXTI_CLR_PR(FOC_HALL_A_EXTI);
    EXTI_CLR_PR(FOC_HALL_B_EXTI);
    EXTI_CLR_PR(FOC_HALL_C_EXTI);

    /* Seed state from whatever the rotor is sitting at right now. */
    uint8_t c0 = read_hall_code();
    if (c0 >= 1U && c0 <= 6U) {
        s_hall_code = c0;
        s_angle = s_sector_center[c0];
    }
    s_dir = 1;

    app_softvec_register_irq(FOC_HALL_A_IRQN, hall_isr);
    app_softvec_register_irq(FOC_HALL_B_IRQN, hall_isr);
    app_softvec_register_irq(FOC_HALL_C_IRQN, hall_isr);

    NVIC_SetPriority(FOC_HALL_A_IRQN, 1);   /* slightly below FOC loop */
    NVIC_SetPriority(FOC_HALL_B_IRQN, 1);
    NVIC_SetPriority(FOC_HALL_C_IRQN, 1);
    NVIC_EnableIRQ(FOC_HALL_A_IRQN);
    NVIC_EnableIRQ(FOC_HALL_B_IRQN);
    NVIC_EnableIRQ(FOC_HALL_C_IRQN);

    return 0;
}

uint16_t foc_hall_angle(void)  { return s_angle; }
uint8_t  foc_hall_code(void)   { return s_hall_code; }

int32_t foc_hall_omega_q15(void)
{
    /* ω in Q15 fraction of fs/6 (one sector per electrical revolution
     * means 6 edges per full turn).  ω_elec = 2π / (6 * dt_s).
     * In our units dt is in PWM periods (Tpwm = 1/FOC_PWM_HZ).
     *
     *   ω_per_period_q15 = direction * Q15 / (6 * last_period)
     *
     * We dodge division by using a precomputed reciprocal — but here
     * dt is dynamic so we accept one __aeabi_idiv at telemetry rate,
     * which only the shell reads.  Inside the FOC loop we extrapolate
     * angle without touching ω.
     */
    uint32_t p = s_last_period;
    if (p == 0U) return 0;
    int32_t v = (int32_t)(32768 / (6 * (int32_t)p));
    return s_dir > 0 ? v : -v;
}
