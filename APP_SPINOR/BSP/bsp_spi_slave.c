/*
 * bsp_spi_slave.c -- see bsp_spi_slave.h
 *
 * Implementation notes
 *
 *   ISR latency is the critical path: at SPI clock 1 MHz, each byte
 *   is 8 us, so we have to consume RX + load TX inside ~6 us
 *   worst-case to keep up. We service the RX FIFO with a tight loop
 *   that empties everything queued, calls the callback once per byte,
 *   and stuffs the response straight into the TX FIFO. The hardware
 *   FIFO is 3 deep, which buys us latency tolerance for ISR jitter
 *   from other interrupts.
 *
 *   For host clocks above ~1 MHz, we'd need to migrate to DMA. That's
 *   a v2 enhancement. The current code targets the broadest host
 *   compatibility, which is 100 kHz - 1 MHz.
 *
 *   CS / EXTI: the SPI peripheral itself handles "NSS hardware" wiring,
 *   but for end-of-frame detection we want a software event. EXTI is
 *   the cheapest way -- we set RTSR on line 3 to fire on the rising
 *   edge of GPIO2.PIN03, which is exactly when the host releases CS.
 */
#include "bsp_spi_slave.h"
#include "TX32F01_periph.h"

/* ------------------------------------------------------------------ */
/*  Pin map -- match vendor 12.SPI/3.SPI_Slave demo                   */
/* ------------------------------------------------------------------ */
#define SNF_CS_PORT       GPIO2
#define SNF_CS_PIN        PIN03
#define SNF_CS_EXTI_LINE  3

#define SNF_CLK_PORT      GPIO3
#define SNF_CLK_PIN       PIN03
#define SNF_MOSI_PORT     GPIO3
#define SNF_MOSI_PIN      PIN04
#define SNF_MISO_PORT     GPIO3
#define SNF_MISO_PIN      PIN05

/* ------------------------------------------------------------------ */
/*  EXTI register helpers (direct, matches APP_FOC/foc_hall pattern)  */
/* ------------------------------------------------------------------ */
#define EXTI_CFG_PORT(line, port_id) \
    do { EXTI->CFGR &= ~((uint32_t)0x7U << ((line) * 3U));            \
         EXTI->CFGR |=  ((uint32_t)(port_id) << ((line) * 3U)); } while (0)
#define EXTI_RISE_EN(line)        (EXTI->RTSR |=  ((uint32_t)1U << (line)))
#define EXTI_FALL_DIS(line)       (EXTI->FTSR &= ~((uint32_t)1U << (line)))
#define EXTI_IT_EN(line)          (EXTI->IMR  |=  ((uint32_t)1U << (line)))
#define EXTI_CLR_PR(line)         (EXTI->PR   &= ~((uint32_t)1U << (line)))

static spi_byte_cb_t        s_byte_cb;
static spi_frame_end_cb_t   s_end_cb;

/* ------------------------------------------------------------------ */
/*  IRQ handlers: SPI and EXTI3                                       */
/*                                                                    */
/* The vendor's startup_TX32F01.s uses the names `SPI_Handler` and    */
/* `EXTI3_Handler` (not the CMSIS-style `_IRQHandler`). Strong symbols */
/* with those exact names override the WEAK defaults in startup.      */
/* ------------------------------------------------------------------ */
void SPI_Handler(void);
void EXTI3_Handler(void);

void SPI_Handler(void)
{
    /* Drain RX FIFO -- one byte in, one byte out. */
    while (!SPI_GetFlagStatus(SPI_RXEM_SR)) {
        uint8_t rx = (uint8_t)SPI_ReceiveData();
        uint8_t tx = s_byte_cb ? s_byte_cb(rx) : 0xFFU;
        SPI_SendData(tx);
    }
    /* Clear any sticky error flags. Overrun is the one we actually
     * care about; if the host clocks faster than we can serve, this
     * gets set and we lose bytes. The protocol layer needs to handle
     * partial frames gracefully -- it usually does because CS goes
     * high at the end and we reset state. */
    SPI_ClearFlag(SPI_OVFL_SR | SPI_UDRU_SR | SPI_TOUT_SR);
}

void EXTI3_Handler(void)
{
    EXTI_CLR_PR(SNF_CS_EXTI_LINE);
    if (s_end_cb) s_end_cb();
}

/* ------------------------------------------------------------------ */
/*  Init                                                              */
/* ------------------------------------------------------------------ */
static void cfg_pins(void)
{
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO2, ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    SCU_PeriphClockCmd(Periph_SPI,   ENABLE);
    SCU_Lock();

    GPIO_Init(SNF_CS_PORT,   SNF_CS_PIN,   GPIO_MODE_AF);
    GPIO_Init(SNF_CLK_PORT,  SNF_CLK_PIN,  GPIO_MODE_AF);
    GPIO_Init(SNF_MOSI_PORT, SNF_MOSI_PIN, GPIO_MODE_AF);
    GPIO_Init(SNF_MISO_PORT, SNF_MISO_PIN, GPIO_MODE_AF);

    GPIO_PinRemapConfig(SNF_CS_PORT,   SNF_CS_PIN,   GPIO_AF_SPI_CS);
    GPIO_PinRemapConfig(SNF_CLK_PORT,  SNF_CLK_PIN,  GPIO_AF_SPI_CLK);
    GPIO_PinRemapConfig(SNF_MOSI_PORT, SNF_MOSI_PIN, GPIO_AF_SPI_MOSI);
    GPIO_PinRemapConfig(SNF_MISO_PORT, SNF_MISO_PIN, GPIO_AF_SPI_MISO);

    /* Pull-ups on every line: CS so an idle host (which leaves CS as
     * an output but tri-stated during reset) doesn't trigger spurious
     * frame-end events; CLK and MOSI so a disconnected host doesn't
     * pick up noise as data. */
    GPIO_PullUpConfig(SNF_CS_PORT,   SNF_CS_PIN);
    GPIO_PullUpConfig(SNF_CLK_PORT,  SNF_CLK_PIN);
    GPIO_PullUpConfig(SNF_MOSI_PORT, SNF_MOSI_PIN);
    GPIO_PullUpConfig(SNF_MISO_PORT, SNF_MISO_PIN);
}

static void cfg_spi(void)
{
    SPI_InitTypeDef s;
    SPI_DeInit();
    s.SPI_Mode      = SPI_Mode_Slave;
    s.SPI_DataWidth = SPI_DataWidth_8b;
    s.SPI_CPOL      = SPI_CPOL_Low;      /* SPI mode 0 -- best NOR host compat */
    s.SPI_CPHA      = SPI_CPHA_1Edge;
    s.SPI_NSS       = SPI_NSS_Hard;      /* CS pin drives slave-select hardware */
    s.SPI_CLK_DIV   = SPI_CLK_DIV_2;     /* slave needs the lowest divider */
    s.SPI_FirstBit  = SPI_FirstBit_MSB;
    SPI_Init(&s);

    /* RX FIFO-not-empty IRQ is what we react to: as soon as the host
     * has clocked in one byte, we want to consume it and pre-load the
     * response. */
    SPI_ClearFlag(SPI_ALL_SR);
    SPI_ITConfig(SPI_RXF1_IE, ENABLE);   /* RX FIFO threshold = 1 byte */
    SPI_Cmd(ENABLE);
}

static void cfg_cs_exti(void)
{
    EXTI_CFG_PORT(SNF_CS_EXTI_LINE, 2);   /* GPIO2 = port id 2 */
    EXTI_RISE_EN(SNF_CS_EXTI_LINE);       /* CS rising edge = frame end */
    EXTI_FALL_DIS(SNF_CS_EXTI_LINE);
    EXTI_CLR_PR(SNF_CS_EXTI_LINE);
    EXTI_IT_EN(SNF_CS_EXTI_LINE);
}

void bsp_spi_slave_init(spi_byte_cb_t byte_cb, spi_frame_end_cb_t end_cb)
{
    s_byte_cb = byte_cb;
    s_end_cb  = end_cb;

    cfg_pins();
    cfg_spi();
    cfg_cs_exti();

    NVIC_SetPriority(SPI_IRQn,   0);   /* must be highest -- SPI clock waits for no one */
    NVIC_SetPriority(EXTI3_IRQn, 1);
    NVIC_EnableIRQ(SPI_IRQn);
    NVIC_EnableIRQ(EXTI3_IRQn);
}

int bsp_spi_slave_cs_active(void)
{
    return (GPIO_ReadInputDataBit(SNF_CS_PORT, SNF_CS_PIN) == 0);
}
