/*
 * APP_I2CEEPROM -- TX32F01 emulates a 16 KB encrypted I2C EEPROM
 * (24LC128-class device address) with a transparent monotonic
 * counter at the top of memory.
 *
 *   pins (don't move; SWD lives on GPIO0.PIN00/PIN01):
 *
 *     host_SCL  --->  GPIO1.PIN05   I2C_SCL
 *     host_SDA  <-->  GPIO1.PIN06   I2C_SDA
 *     GND       ===   GND
 *
 *     UART_TX   <---  GPIO3.PIN07   diagnostic out @ 115200 8N1
 *     UART_RX   --->  GPIO3.PIN06   (unused, available)
 *
 * No bootloader. The APP owns the reset vector at 0x01000000.
 */
#include "TX32F01_periph.h"
#include "bsp_uart.h"
#include "eeprom_emu.h"
#include "eeprom_storage.h"
#include "eeprom_protocol.h"

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

/* 1 ms tick via SysTick. */
static volatile uint32_t s_ms;
void SysTick_Handler(void) { s_ms++; }
static uint32_t now_ms(void) { return s_ms; }

static void print_stats(void)
{
    bsp_uart_puts("[I2CEE] up=");
    bsp_uart_put_u32(now_ms());
    bsp_uart_puts("ms  wr=");
    bsp_uart_put_u32(ee_emu_writes());
    bsp_uart_puts(" rd=");
    bsp_uart_put_u32(ee_emu_reads());
    bsp_uart_puts(" bytes=");
    bsp_uart_put_u32(ee_emu_bytes());
    bsp_uart_puts(" addr=");
    bsp_uart_put_hex((uint32_t)ee_emu_last_addr());
    bsp_uart_puts(" counter=");
    bsp_uart_put_u32(ee_storage_counter_get());
    bsp_uart_puts("\r\n");
}

int main(void)
{
    scu_init_24mhz();
    led_init();
    bsp_uart_init_115200();

    bsp_uart_puts("\r\n");
    bsp_uart_puts("===============================================\r\n");
    bsp_uart_puts("  TX32F01 I2C EEPROM Emulator\r\n");
    bsp_uart_puts("  16 KB host-visible, 24LC128-class semantics\r\n");
    bsp_uart_puts("  Bus address: 0x50 (7-bit)\r\n");
    bsp_uart_puts("  Encrypted region: 0x3F00 .. 0x3FFB (AES-128-CTR)\r\n");
    bsp_uart_puts("  Counter region:   0x3FFC .. 0x3FFF (read=value, write=+1)\r\n");
    bsp_uart_puts("  Pins: SCL=GPIO1.05  SDA=GPIO1.06\r\n");
    bsp_uart_puts("===============================================\r\n");

    ee_emu_init();

    SysTick_Config(24000U);
    NVIC_SetPriority(SysTick_IRQn, 3);

    bsp_uart_puts("[I2CEE] ready. waiting for host I2C activity...\r\n");

    uint32_t last_led  = 0;
    uint32_t last_stat = 0;

    for (;;) {
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
