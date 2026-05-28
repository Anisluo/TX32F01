/*
 * APP_SPINOR -- TX32F01 emulates a 24 KB W25Q-compatible SPI NOR Flash.
 *
 * Wiring (host = the SPI master clocking us):
 *
 *     host_CS    --->  GPIO2.PIN03   SPI_CS
 *     host_CLK   --->  GPIO3.PIN03   SPI_CLK
 *     host_MOSI  --->  GPIO3.PIN04   SPI_MOSI
 *     host_MISO  <---  GPIO3.PIN05   SPI_MISO
 *     GND        ===   GND
 *
 * Plus our own debug UART on GPIO3.PIN06/07 at 115200 8N1 so a human
 * can watch what the emulator is doing.
 *
 * No bootloader: this APP owns the reset vector at 0x01000000 and uses
 * the first 6 KB of Flash for code; the remaining 24 KB at 0x01001800
 * is the host-visible storage area.
 *
 * Standalone proof:
 *   1) On a Linux box with a USB-SPI dongle (Bus Pirate, FT2232, etc.):
 *
 *        flashrom -p ft2232_spi:type=2232H,port=A,divisor=64 -r dump.bin
 *
 *      should successfully read 32 KB (our 24 KB + 8 KB of 0xFF
 *      padding) and report "Found Winbond W25Q-class".
 *
 *   2) Issue WRITE_ENABLE (0x06), SECTOR_ERASE (0x20 + 3-byte addr),
 *      poll status until BUSY=0, PAGE_PROGRAM (0x02 + addr + 256 B),
 *      poll status, READ (0x03 + addr) -- should round-trip cleanly.
 *
 *   3) Watch the UART line: every second the emulator prints a
 *      one-line summary of bytes received, commands seen, programs,
 *      erases, and current SR1 -- which is exactly what you want
 *      while debugging a host driver against it.
 */
#include "TX32F01_periph.h"
#include "bsp_uart.h"
#include "bsp_spi_slave.h"
#include "spinor_emu.h"
#include "spinor_protocol.h"

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

/* ---------- 1 ms tick via SysTick (no soft-vector glue needed; we
 * own the reset vector ourselves now). The standard CMSIS handler
 * symbol matches the startup file's WEAK alias. ------------------- */
static volatile uint32_t s_ms;
void SysTick_Handler(void) { s_ms++; }

static uint32_t now_ms(void) { return s_ms; }

/* ---------- Periodic stats line ---------- */
static void print_stats(void)
{
    bsp_uart_puts("[SPINOR] up=");
    bsp_uart_put_u32(now_ms());
    bsp_uart_puts("ms  rx=");
    bsp_uart_put_u32(snf_emu_total_bytes_rx());
    bsp_uart_puts(" cmds=");
    bsp_uart_put_u32(snf_emu_total_cmds());
    bsp_uart_puts(" prog=");
    bsp_uart_put_u32(snf_emu_total_programs());
    bsp_uart_puts(" eras=");
    bsp_uart_put_u32(snf_emu_total_erases());
    bsp_uart_puts(" sr1=");
    bsp_uart_put_hex(snf_emu_status_reg1());
    bsp_uart_puts("\r\n");
}

int main(void)
{
    scu_init_24mhz();
    led_init();
    bsp_uart_init_115200();

    bsp_uart_puts("\r\n");
    bsp_uart_puts("============================================\r\n");
    bsp_uart_puts("  TX32F01 SPI NOR Flash Emulator\r\n");
    bsp_uart_puts("  Emulating W25Q-class device, 24 KB backing\r\n");
    bsp_uart_puts("  JEDEC ID: 0xEF 0x40 0x0F\r\n");
    bsp_uart_puts("  Pins: CS=GPIO2.03 CLK=GPIO3.03 MO=GPIO3.04 MI=GPIO3.05\r\n");
    bsp_uart_puts("============================================\r\n");

    snf_emu_init();

    /* 1 ms tick. Priority lowest so SPI / EXTI3 always preempt. */
    SysTick_Config(24000U);
    NVIC_SetPriority(SysTick_IRQn, 3);

    bsp_uart_puts("[SPINOR] ready. waiting for host SPI activity...\r\n");

    uint32_t last_led  = 0;
    uint32_t last_stat = 0;

    for (;;) {
        /* Drain deferred Flash work (programs/erases) outside any ISR.
         * This is where the actual blocking happens. */
        snf_emu_tick();

        uint32_t now = now_ms();
        if (now - last_led >= 500U) {
            last_led = now;
            GPIO_Toggle(GPIO0, PIN03);
        }
        if (now - last_stat >= 1000U) {
            last_stat = now;
            print_stats();
        }
    }
}
