/*
 * APP_CAN — SoftCAN + multi-node time sync on TX32F01.
 *
 * Build two boards (or N) with this same firmware. Distinguish the
 * master from slaves either by hard-wired GPIO or by recompiling with
 * IS_MASTER = 1.
 *
 * Wiring per node (default pin map):
 *   CAN_TX  : GPIO1.PIN00   — open-drain output
 *   CAN_RX  : GPIO1.PIN01   — input pull-up, EXTI line 1
 *   UART    : GPIO3.PIN06/07 @ 115200, 8N1 — stats output
 *
 * Bus wiring:
 *   Tie all nodes' CAN_TX together. Tie all nodes' CAN_RX together.
 *   Tie the TX-pool and RX-pool together (one bus wire). Add ONE
 *   external pull-up (~4.7 kΩ) to 3V3 anywhere on the bus.
 *   For a single-board sanity check, jumper PIN00 to PIN01 and set
 *   IS_MASTER to 1; the slave parsing path will be silent but the
 *   master will see its own frames decoded back (self-ACK works
 *   because both halves share the same sample window).
 *
 * Output (UART @115200) every 500 ms:
 *   [m] rx=N tx=N crc=N stuff=N bit=N ack=N arb=N tec=N rec=N
 *   [s] up=N off=±Nus skew=±Nppm v=Nus
 */

#include "TX32F01_periph.h"
#include "softcan.h"
#include "can_sync.h"
#include <stdint.h>

#ifndef IS_MASTER
#define IS_MASTER  1                    /* set to 0 on slave nodes */
#endif

#ifndef SYNC_PERIOD_MS
#define SYNC_PERIOD_MS 100
#endif

/* ---------- BSP ---------- */
static void scu_init_24mhz(void)
{
    SCU_Unlock();
    SCU_SetSysClock(SysClock_24M);
    SCU_ResetPeriphClock(Periph_ALL);
    SCU_SetBor(BOR_2P5V, ENABLE);
    SCU_ClearPWR_Flag();
    SCU_Lock();
}

static void led_init(void)
{
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO0, ENABLE);
    SCU_Lock();
    GPIO_Init(GPIO0, PIN03, GPIO_MODE_OUTPUT_PP);
}

static void uart_init_115200(void)
{
    UART_InitTypeDef u;
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    SCU_PeriphClockCmd(Periph_UART,  ENABLE);
    SCU_Lock();
    GPIO_Init(GPIO3, PIN06, GPIO_MODE_AF);
    GPIO_Init(GPIO3, PIN07, GPIO_MODE_AF);
    GPIO_PinRemapConfig(GPIO3, PIN07, GPIO_AF_UART_TX);
    GPIO_PinRemapConfig(GPIO3, PIN06, GPIO_AF_UART_RX);
    UART_DeInit();
    u.UART_BaudRate=115200; u.UART_WordLength=UART_8DATABIT;
    u.UART_StopBits=UART_1STOPBIT; u.UART_Parity=UART_Pority_None;
    u.UART_Mode=UART_Mode_Rx|UART_Mode_Tx;
    UART_Init(&u);
    UART_Cmd(ENABLE);
}

static void uart_putc(char c){
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}
static void uart_puts(const char *s){ while (*s) uart_putc(*s++); }
static void put_i32(int32_t v) {
    char b[12]; int n=0; uint32_t u;
    if (v < 0) { uart_putc('-'); u = (uint32_t)(-v); } else u = (uint32_t)v;
    if (!u) { uart_putc('0'); return; }
    while (u && n<11) { b[n++]=(char)('0'+u%10); u/=10; }
    while (n--) uart_putc(b[n]);
}
static void put_u32(uint32_t v) { put_i32((int32_t)v); /* unsigned ≤ 2^31-1 prints fine; > prints negative — fine for stats */ }

/* ---------- 1 ms tick via SysTick (polling, no IRQ) ---------- */
/* We deliberately keep SysTick as a polling clock so the SoftCAN
 * timer + EXTI ISRs are not preempted by another tick. The CAN
 * timing budget at 24 MHz / 10 kbps is generous, but isolating IRQs
 * to one stack (CAN) keeps the bit cadence very predictable. */
static uint32_t s_ms;
static void tick_init(void)
{
    SysTick->LOAD = 24000U - 1U;
    SysTick->VAL  = 0;
    SysTick->CTRL = 0x5;        /* enable, processor clock, no IRQ */
}
static uint8_t tick_consume(void)
{
    if (SysTick->CTRL & 0x10000U) { s_ms++; return 1; }
    return 0;
}

/* ---------- CAN callbacks (run in ISR context) ---------- */
static volatile uint32_t s_rx_seen;     /* heartbeat for the LED */

static void on_rx(const can_rx_event_t *ev)
{
    s_rx_seen++;
    can_sync_on_rx(ev->frame.id, ev->frame.data, ev->frame.dlc,
                   ev->sof_timestamp_us);
}
static void on_tx(const can_frame_t *frame, uint32_t sof_us)
{
    can_sync_on_tx(frame->id, sof_us);
}

/* ---------- IRQ entry-point trampolines ---------- */
/* TX32F01 weak symbols: TIMER0_Handler / EXTI1_Handler / TIMER2_Handler.
 * The startup file declares these — we override them here. */
void TIMER0_Handler(void) { softcan_timer_isr(); }
void TIMER2_Handler(void) { softcan_us_overflow_isr(); }
void EXTI1_Handler (void) { softcan_rx_edge_isr(); }

int main(void)
{
    scu_init_24mhz();
    led_init();
    uart_init_115200();
    tick_init();

    uart_puts("\r\n[APP_CAN] SoftCAN + time-sync, ");
    uart_puts(IS_MASTER ? "MASTER\r\n" : "SLAVE\r\n");

    softcan_cfg_t scfg = {
        .bitrate          = 10000,
        .tx_port          = GPIO1, .tx_pin = PIN00,
        .rx_port          = GPIO1, .rx_pin = PIN01,
        .rx_exti_line     = 1,
        .rx_exti_gpio_sel = 1,                  /* GPIO1 */
        .rx_cb            = on_rx,
        .tx_cb            = on_tx,
    };
    if (softcan_init(&scfg) != 0) {
        uart_puts("[err] softcan_init\r\n");
        for(;;);
    }

    can_sync_cfg_t ycfg = {
        .is_master      = IS_MASTER,
        .sync_period_ms = SYNC_PERIOD_MS,
    };
    can_sync_init(&ycfg);

    uint32_t last_print_ms = 0;
    uint32_t last_led_ms   = 0;

    for (;;) {
        if (!tick_consume()) continue;

        can_sync_tick_ms();

        if ((s_ms - last_led_ms) >= 500U) {
            last_led_ms = s_ms;
            GPIO_Toggle(GPIO0, PIN03);
        }

        if ((s_ms - last_print_ms) >= 500U) {
            last_print_ms = s_ms;
            softcan_stats_t st;
            softcan_get_stats(&st);

            uart_puts(IS_MASTER ? "[m] " : "[s] ");
            uart_puts("rx=");    put_u32(st.rx_frames);
            uart_puts(" tx=");   put_u32(st.tx_frames);
            uart_puts(" crc=");  put_u32(st.crc_err);
            uart_puts(" stuff=");put_u32(st.stuff_err);
            uart_puts(" bit=");  put_u32(st.bit_err);
            uart_puts(" ack=");  put_u32(st.ack_err);
            uart_puts(" arb=");  put_u32(st.arb_lost);
            uart_puts(" tec=");  put_u32(st.tec);
            uart_puts(" rec=");  put_u32(st.rec);

            if (!IS_MASTER) {
                uart_puts(" | up=");   put_u32(can_sync_updates());
                uart_puts(" off=");    put_i32(can_sync_last_offset_us());
                uart_puts("us skew="); put_i32(can_sync_skew_ppm());
                uart_puts("ppm v=");   put_u32((uint32_t)can_sync_virtual_us());
                uart_puts("us");
            }
            uart_puts("\r\n");
        }
    }
}
