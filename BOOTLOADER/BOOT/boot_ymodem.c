#include "boot_ymodem.h"
#include "boot_uart.h"
#include "boot_flash.h"

/* YMODEM 控制字符 */
#define SOH     0x01    /* 128 B 包 */
#define STX     0x02    /* 1024 B 包 */
#define EOT     0x04
#define ACK     0x06
#define NAK     0x15
#define CAN     0x18
#define C_CHR   0x43    /* 'C' */

#define PKT_TIMEOUT_MS    3000U
#define BYTE_TIMEOUT_MS   1000U
#define MAX_RETRIES       10

/* CRC-16/XMODEM (poly 0x1021, init 0)*/
static uint16_t crc16_xmodem(const uint8_t *p, uint32_t n)
{
    uint16_t crc = 0;
    while (n--) {
        crc ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

/* 收一整包到 buf（不含 header 的 3 字节和尾部 2 字节 CRC）。
   返回数据载荷长度：128 / 1024 / 0(EOT) / -1(错) */
static int recv_packet(uint8_t *buf, uint8_t *out_seq)
{
    uint8_t hdr;
    if (!buart_recv_byte(&hdr, PKT_TIMEOUT_MS)) return -1;

    uint32_t plen;
    if (hdr == SOH)      plen = 128;
    else if (hdr == STX) plen = 1024;
    else if (hdr == EOT) return 0;
    else if (hdr == CAN) return -1;
    else                 return -1;

    uint8_t seq, nseq;
    if (!buart_recv_byte(&seq,  BYTE_TIMEOUT_MS)) return -1;
    if (!buart_recv_byte(&nseq, BYTE_TIMEOUT_MS)) return -1;
    if ((uint8_t)(seq ^ nseq) != 0xFF) return -1;
    *out_seq = seq;

    for (uint32_t i = 0; i < plen; i++) {
        if (!buart_recv_byte(&buf[i], BYTE_TIMEOUT_MS)) return -1;
    }
    uint8_t ch, cl;
    if (!buart_recv_byte(&ch, BYTE_TIMEOUT_MS)) return -1;
    if (!buart_recv_byte(&cl, BYTE_TIMEOUT_MS)) return -1;
    uint16_t rx_crc = ((uint16_t)ch << 8) | cl;
    if (rx_crc != crc16_xmodem(buf, plen)) return -1;

    return (int)plen;
}

static uint32_t parse_filesize(const uint8_t *blk)
{
    /* 包 0 的载荷：filename\0 filesize_decimal_ascii ' ' or \0 ... */
    /* 跳过文件名 */
    uint32_t i = 0;
    while (i < 128 && blk[i] != 0) i++;
    if (i >= 128) return 0;
    i++;
    uint32_t size = 0;
    while (i < 128 && blk[i] >= '0' && blk[i] <= '9') {
        size = size * 10 + (blk[i] - '0');
        i++;
    }
    return size;
}

uint32_t ymodem_recv_to_app(uint32_t *out_size)
{
    static uint8_t pkt[1024];
    uint32_t app_size = 0;
    uint32_t recv_bytes = 0;
    uint32_t write_addr = FLASH_APP_BASE;
    uint8_t  seq, expect_seq;
    int      plen;
    int      retries;

    /* === 第一阶段：等首包 (block 0) === */
    for (retries = 0; retries < MAX_RETRIES; retries++) {
        buart_send_byte(C_CHR);                /* 发 'C' 触发 PC */
        plen = recv_packet(pkt, &seq);
        if (plen > 0 && seq == 0) break;
        if (plen == 0) { buart_send_byte(ACK); /* EOT 在头段不应来，但容错 */ }
    }
    if (retries >= MAX_RETRIES) return 0;

    if (pkt[0] == 0) {
        /* PC 直接发空包结束，没文件传 */
        buart_send_byte(ACK);
        return 0;
    }
    app_size = parse_filesize(pkt);
    if (app_size == 0 || app_size > FLASH_APP_SIZE) {
        buart_send_byte(CAN); buart_send_byte(CAN);
        return 0;
    }
    buart_send_byte(ACK);                       /* 确认头包 */

    /* === 第二阶段：循环收数据 === */
    expect_seq = 1;
    buart_send_byte(C_CHR);                     /* 进入 CRC 数据传输 */

    for (;;) {
        retries = 0;
        do {
            plen = recv_packet(pkt, &seq);
            if (plen < 0) {
                buart_send_byte(NAK);
                retries++;
            }
        } while (plen < 0 && retries < MAX_RETRIES);
        if (plen < 0) return 0;

        if (plen == 0) {
            /* EOT */
            buart_send_byte(NAK);               /* 第一次 EOT 回 NAK */
            uint8_t eot2;
            if (!buart_recv_byte(&eot2, PKT_TIMEOUT_MS)) return 0;
            if (eot2 != EOT) return 0;
            buart_send_byte(ACK);
            buart_send_byte(C_CHR);
            /* 等结束包（filename 全 0） */
            for (retries = 0; retries < MAX_RETRIES; retries++) {
                plen = recv_packet(pkt, &seq);
                if (plen > 0 && seq == 0) break;
            }
            buart_send_byte(ACK);
            break;
        }

        if (seq != expect_seq) {
            /* 重复包：再 ACK 一次（PC 没收到上次 ACK 重发）*/
            if (seq == (uint8_t)(expect_seq - 1)) {
                buart_send_byte(ACK);
                continue;
            }
            buart_send_byte(NAK);
            continue;
        }

        /* 计算这一包要写到哪、覆盖几个扇区，先擦后写。
           1024 B 整好 = 2 个 512 B 扇区。 */
        if (!bflash_erase_app_range(write_addr, (uint32_t)plen)) {
            buart_send_byte(CAN); buart_send_byte(CAN);
            return 0;
        }
        /* 末包可能比 app_size 多出 padding，但写整包没问题，CRC 用 app_size 控制范围 */
        if (!bflash_program(write_addr, pkt, (uint32_t)plen)) {
            buart_send_byte(CAN); buart_send_byte(CAN);
            return 0;
        }
        write_addr += (uint32_t)plen;
        recv_bytes += (uint32_t)plen;

        buart_send_byte(ACK);
        expect_seq++;

        if (recv_bytes >= app_size) {
            /* 已收齐，但继续走 EOT 流程，PC 会自己发 EOT */
        }
    }

    if (out_size) *out_size = app_size;
    return app_size;
}
