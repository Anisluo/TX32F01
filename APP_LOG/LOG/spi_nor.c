#include "spi_nor.h"
#include "TX32F01_periph.h"

#define CMD_WRITE_ENABLE        0x06
#define CMD_READ_STATUS         0x05
#define CMD_READ_DATA           0x03
#define CMD_PAGE_PROGRAM        0x02
#define CMD_SECTOR_ERASE_4K     0x20
#define CMD_CHIP_ERASE          0xC7
#define CMD_JEDEC_ID            0x9F
#define CMD_POWER_DOWN          0xB9
#define CMD_RELEASE_PD          0xAB

#define SR_BUSY_BIT             0x01

#define CS_LOW()   GPIO_ResetBits(GPIO2, PIN04)
#define CS_HIGH()  GPIO_SetBits  (GPIO2, PIN04)

static uint8_t xfer(uint8_t tx)
{
    SPI_ClearFlag(SPI_ALL_SR);
    SPI_SendData(tx);
    while (!SPI_GetFlagStatus(SPI_TXEM_SR)) { }
    while ( SPI_GetFlagStatus(SPI_RXEM_SR)) { }
    return SPI_ReceiveData();
}

static void cmd1(uint8_t op)
{
    CS_LOW();
    xfer(op);
    CS_HIGH();
}

static void send_addr24(uint32_t addr)
{
    xfer((uint8_t)(addr >> 16));
    xfer((uint8_t)(addr >>  8));
    xfer((uint8_t) addr);
}

static uint8_t read_status(void)
{
    uint8_t sr;
    CS_LOW();
    xfer(CMD_READ_STATUS);
    sr = xfer(0xFF);
    CS_HIGH();
    return sr;
}

void spi_nor_init(void)
{
    SPI_InitTypeDef s;

    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO2, ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    SCU_PeriphClockCmd(Periph_SPI,   ENABLE);
    SCU_Lock();

    GPIO_Init(GPIO2, PIN04, GPIO_MODE_OUTPUT_PP);
    CS_HIGH();

    GPIO_Init(GPIO2, PIN05, GPIO_MODE_AF);
    GPIO_Init(GPIO3, PIN00, GPIO_MODE_AF);
    GPIO_Init(GPIO3, PIN01, GPIO_MODE_AF);
    GPIO_PinRemapConfig(GPIO2, PIN05, GPIO_AF_SPI_CLK);
    GPIO_PinRemapConfig(GPIO3, PIN00, GPIO_AF_SPI_MOSI);
    GPIO_PinRemapConfig(GPIO3, PIN01, GPIO_AF_SPI_MISO);

    SPI_DeInit();
    s.SPI_Mode      = SPI_Mode_Master;
    s.SPI_DataWidth = SPI_DataWidth_8b;
    s.SPI_CPOL      = SPI_CPOL_High;
    s.SPI_CPHA      = SPI_CPHA_2Edge;
    s.SPI_NSS       = SPI_NSS_Soft;
    s.SPI_CLK_DIV   = SPI_CLK_DIV_8;        /* 24MHz / 8 = 3 MHz SCK */
    s.SPI_FirstBit  = SPI_FirstBit_MSB;
    SPI_Init(&s);
    SPI_Cmd(ENABLE);

    spi_nor_wakeup();
}

uint32_t spi_nor_read_jedec(void)
{
    uint32_t id = 0;
    CS_LOW();
    xfer(CMD_JEDEC_ID);
    id  = (uint32_t)xfer(0xFF) << 16;
    id |= (uint32_t)xfer(0xFF) <<  8;
    id |= (uint32_t)xfer(0xFF);
    CS_HIGH();
    return id;
}

void spi_nor_wakeup(void)    { cmd1(CMD_RELEASE_PD); }
void spi_nor_powerdown(void) { cmd1(CMD_POWER_DOWN); }

uint8_t spi_nor_busy(void)
{
    return (read_status() & SR_BUSY_BIT) ? 1U : 0U;
}

void spi_nor_read(uint32_t addr, void *buf, uint32_t len)
{
    uint8_t *p = (uint8_t *)buf;
    CS_LOW();
    xfer(CMD_READ_DATA);
    send_addr24(addr);
    while (len--) *p++ = xfer(0xFF);
    CS_HIGH();
}

void spi_nor_program_page(uint32_t addr, const void *buf, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    cmd1(CMD_WRITE_ENABLE);
    CS_LOW();
    xfer(CMD_PAGE_PROGRAM);
    send_addr24(addr);
    while (len--) xfer(*p++);
    CS_HIGH();
    /* tPP typ 0.7ms, max 3ms — caller polls spi_nor_busy(). */
}

void spi_nor_erase_sector_start(uint32_t sector_addr)
{
    cmd1(CMD_WRITE_ENABLE);
    CS_LOW();
    xfer(CMD_SECTOR_ERASE_4K);
    send_addr24(sector_addr);
    CS_HIGH();
    /* tSE typ 45ms, max 400ms — caller polls spi_nor_busy(). */
}

void spi_nor_erase_chip_start(void)
{
    cmd1(CMD_WRITE_ENABLE);
    cmd1(CMD_CHIP_ERASE);
    /* tCE typ 25s / max 100s on W25Q16 — caller polls spi_nor_busy(). */
}
