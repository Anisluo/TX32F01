/*
 * bsp_i2c_slave.c -- see bsp_i2c_slave.h
 *
 * The TX32F01 I2C controller is Philips-style: when an event occurs
 * (start, slave-address match, data byte transferred, stop), the
 * peripheral updates SR with a state code, raises SI, and stretches
 * SCL low until we clear SI. We just look at SR and dispatch.
 *
 * State codes that matter for SLAVE operation:
 *
 *   0x60 = SLA+W received, ACK returned     -> host starting a write
 *   0x80 = data received, ACK returned      -> received one byte
 *   0x88 = data received, NACK returned     -> received last byte
 *   0xA8 = SLA+R received, ACK returned     -> host starting a read
 *   0xB8 = byte transmitted, ACK received   -> host wants more
 *   0xC0 = byte transmitted, NACK received  -> host done reading
 *   0xC8 = last byte transmitted, NACK ack  -> last-byte case
 *   0xD0 = STOP / re-START detected
 *
 * The callback layer turns these into protocol semantics.
 */
#include "bsp_i2c_slave.h"
#include "TX32F01_periph.h"

/* ----- I2C control-bit macros (mirroring vendor Slave_IAP demo) ----- */
#define I2C_ENABLE_BIT       (1U << 6)
#define I2C_AA_BIT           (1U << 2)
#define I2C_SI_BIT           (1U << 3)

#define I2C_CLEAR_SI()       (I2C->CR &= ~I2C_SI_BIT)
#define I2C_SET_AA()         (I2C->CR |=  I2C_AA_BIT)
#define I2C_CLR_AA()         (I2C->CR &= ~I2C_AA_BIT)
#define I2C_ENABLE()         (I2C->CR |=  I2C_ENABLE_BIT)
#define I2C_DISABLE()        (I2C->CR &= ~I2C_ENABLE_BIT)

/* ----- pin map ----- */
#define EE_SCL_PORT   GPIO1
#define EE_SCL_PIN    PIN05
#define EE_SDA_PORT   GPIO1
#define EE_SDA_PIN    PIN06

static i2c_slave_cb_t s_cb;

/* ------------------------------------------------------------------ */
/*  ISR                                                                */
/* ------------------------------------------------------------------ */
void I2C_Handler(void);
void I2C_Handler(void)
{
    uint8_t sr = (uint8_t)I2C->SR;

    switch (sr) {

    case 0x60: /* SLA+W received, ACK'd */
        if (s_cb.start_write) s_cb.start_write();
        I2C_SET_AA();
        I2C_CLEAR_SI();
        break;

    case 0x80: { /* data received, ACK'd */
        uint8_t b = (uint8_t)I2C->DR;
        int nack = 0;
        if (s_cb.rx_byte) nack = s_cb.rx_byte(b);
        if (nack) I2C_CLR_AA(); else I2C_SET_AA();
        I2C_CLEAR_SI();
        break;
    }

    case 0x88: { /* data received, slave returned NACK -- last byte */
        uint8_t b = (uint8_t)I2C->DR;
        if (s_cb.rx_byte) (void)s_cb.rx_byte(b);
        I2C_SET_AA();
        I2C_CLEAR_SI();
        break;
    }

    case 0xA8: { /* SLA+R received, ACK'd */
        uint8_t b = s_cb.start_read ? s_cb.start_read() : 0xFFU;
        I2C->DR = b;
        I2C_SET_AA();
        I2C_CLEAR_SI();
        break;
    }

    case 0xB8: { /* TX byte ACK'd by host */
        uint8_t b = s_cb.tx_byte ? s_cb.tx_byte() : 0xFFU;
        I2C->DR = b;
        I2C_SET_AA();
        I2C_CLEAR_SI();
        break;
    }

    case 0xC0: /* TX byte NACK'd: host done reading */
    case 0xC8:
        I2C_SET_AA();
        I2C_CLEAR_SI();
        break;

    case 0xD0: /* STOP / re-START */
        if (s_cb.stop) s_cb.stop();
        I2C_SET_AA();
        I2C_CLEAR_SI();
        break;

    default:
        I2C_CLEAR_SI();
        break;
    }
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */
void bsp_i2c_slave_init(uint8_t addr_7bit, const i2c_slave_cb_t *cb)
{
    s_cb = *cb;

    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO1, ENABLE);
    SCU_PeriphClockCmd(Periph_I2C,   ENABLE);
    SCU_Lock();

    GPIO_Init(EE_SCL_PORT, EE_SCL_PIN, GPIO_MODE_AF);
    GPIO_Init(EE_SDA_PORT, EE_SDA_PIN, GPIO_MODE_AF);
    GPIO_PinRemapConfig(EE_SCL_PORT, EE_SCL_PIN, GPIO_AF_SCL);
    GPIO_PinRemapConfig(EE_SDA_PORT, EE_SDA_PIN, GPIO_AF_SDA);
    /* I2C lines need pull-ups -- if the host PCB doesn't have them,
     * the internal ones at least keep the line valid at idle. */
    GPIO_PullUpConfig(EE_SCL_PORT, EE_SCL_PIN);
    GPIO_PullUpConfig(EE_SDA_PORT, EE_SDA_PIN);

    I2C_DISABLE();

    /* address: bits[7:1] = 7-bit addr, bit 0 = GCE (general call enable;
     * we leave it 0 -- only respond to our address). */
    I2C->AR = (uint32_t)((addr_7bit & 0x7FU) << 1);

    I2C_SET_AA();    /* AA=1: respond to our address with ACK */
    I2C_ENABLE();

    NVIC_SetPriority(I2C_IRQn, 0);   /* I2C bit timing waits for no one */
    NVIC_EnableIRQ(I2C_IRQn);
}
