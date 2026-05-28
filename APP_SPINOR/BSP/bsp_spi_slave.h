/*
 * bsp_spi_slave.h
 *
 * Drives the SPI peripheral in slave mode with the same pin map the
 * vendor's "12.SPI/3.SPI_Slave" demo uses (confirmed to be the slave
 * AF mapping, not master). Adds an EXTI watcher on the CS line so the
 * protocol layer is told when the host deasserts CS — that's our only
 * reliable end-of-frame signal in pure SPI.
 *
 *   pins (do not move; SWD lives on GPIO0.PIN00/PIN01):
 *
 *     GPIO2.PIN03   SPI_CS   (input, EXTI line 3 for frame boundary)
 *     GPIO3.PIN03   SPI_CLK  (input, hardware AF)
 *     GPIO3.PIN04   SPI_MOSI (input, hardware AF)
 *     GPIO3.PIN05   SPI_MISO (output, hardware AF)
 *
 *   mode 0 (CPOL=0, CPHA=0)  --  the most common for SPI NOR. Hosts
 *   that want mode 3 simply produce CLK-idle-high and edge-on-first
 *   bit; the silicon can be reconfigured later if a host needs it.
 *
 * Threading model
 *   - the protocol layer registers a single per-byte callback. The
 *     ISR pulls each byte out of the RX FIFO as it arrives and invokes
 *     the callback in interrupt context. The callback returns the byte
 *     to ship out in response to the *next* clocked byte.
 *   - the protocol layer also registers a frame-end callback. EXTI3
 *     fires on CS rising edge and that callback runs in IRQ context.
 *
 * Both callbacks must be short. Anything slow (Flash erase) is queued
 * in the main loop via flags the protocol layer manages itself.
 */
#ifndef APP_SPINOR_BSP_SPI_SLAVE_H
#define APP_SPINOR_BSP_SPI_SLAVE_H

#include <stdint.h>

typedef uint8_t (*spi_byte_cb_t)(uint8_t rx_byte);
typedef void    (*spi_frame_end_cb_t)(void);

/* Configure GPIOs, enable peripheral clocks, init SPI in slave mode 0,
 * wire EXTI line 3 to the CS pin, register callbacks, and enable
 * interrupts at the NVIC. Idempotent: safe to call once at boot. */
void bsp_spi_slave_init(spi_byte_cb_t byte_cb, spi_frame_end_cb_t end_cb);

/* True while CS is asserted (low). Useful for the protocol layer to
 * decide whether to keep pushing READ data or stop. */
int  bsp_spi_slave_cs_active(void);

#endif /* APP_SPINOR_BSP_SPI_SLAVE_H */
