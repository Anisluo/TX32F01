/*
 * bsp_i2c_slave.h
 *
 * I2C slave driver wired to the vendor's "9.IIC/5.Slave_IAP" pin map
 * (confirmed slave AF mapping):
 *
 *     GPIO1.PIN05   SCL
 *     GPIO1.PIN06   SDA
 *
 * The protocol layer registers four callbacks; the ISR invokes them
 * inline as the I2C state register reports each event.
 *
 *   start_write_cb()  -> host issued SLA+W, slave is about to receive
 *   rx_byte_cb(byte)  -> slave received one data byte
 *   start_read_cb()   -> host issued SLA+R, return first TX byte
 *   tx_byte_cb()      -> host ACK'd, return next TX byte
 *   stop_cb()         -> STOP detected (frame ended)
 *
 * Clock stretching: when our ISR holds SI=1 (we haven't cleared it),
 * the I2C peripheral pulls SCL low automatically. So we can spend
 * arbitrary time in the callback and the host transparently waits.
 * That's how we cover Flash-erase times -- the page program callback
 * returns only after the sector erase completes.
 */
#ifndef APP_I2CEEPROM_BSP_I2C_SLAVE_H
#define APP_I2CEEPROM_BSP_I2C_SLAVE_H

#include <stdint.h>

typedef void    (*i2c_start_write_cb_t)(void);
typedef int     (*i2c_rx_byte_cb_t)(uint8_t byte);    /* return 0 to ACK, 1 to NACK */
typedef uint8_t (*i2c_start_read_cb_t)(void);          /* return first byte */
typedef uint8_t (*i2c_tx_byte_cb_t)(void);             /* return next byte */
typedef void    (*i2c_stop_cb_t)(void);

typedef struct {
    i2c_start_write_cb_t start_write;
    i2c_rx_byte_cb_t     rx_byte;
    i2c_start_read_cb_t  start_read;
    i2c_tx_byte_cb_t     tx_byte;
    i2c_stop_cb_t        stop;
} i2c_slave_cb_t;

/* 7-bit slave address. Stored without the R/W direction bit. */
void bsp_i2c_slave_init(uint8_t addr_7bit, const i2c_slave_cb_t *cb);

#endif /* APP_I2CEEPROM_BSP_I2C_SLAVE_H */
