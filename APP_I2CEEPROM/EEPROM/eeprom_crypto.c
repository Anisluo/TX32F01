/*
 * eeprom_crypto.c -- minimal AES-128 + CTR implementation.
 *
 * Tiny, byte-oriented, no tables larger than the canonical S-box
 * (256 B). No round-key precompute beyond the standard schedule.
 * Big-endian byte order inside each AES state row, MSB-first, as in
 * FIPS-197.
 *
 * This is a teaching-grade implementation aimed at clarity over peak
 * throughput. Suitable for our use because the encrypted region only
 * sees ~64 bytes at a time during a page write.
 */
#include "eeprom_crypto.h"

/* ------------------------------------------------------------------ */
/*  S-box (FIPS-197 figure 7)                                          */
/* ------------------------------------------------------------------ */
static const uint8_t sbox[256] = {
  0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
  0xCA,0x82,0xC9,0x7D,0xFA,0x59,0x47,0xF0,0xAD,0xD4,0xA2,0xAF,0x9C,0xA4,0x72,0xC0,
  0xB7,0xFD,0x93,0x26,0x36,0x3F,0xF7,0xCC,0x34,0xA5,0xE5,0xF1,0x71,0xD8,0x31,0x15,
  0x04,0xC7,0x23,0xC3,0x18,0x96,0x05,0x9A,0x07,0x12,0x80,0xE2,0xEB,0x27,0xB2,0x75,
  0x09,0x83,0x2C,0x1A,0x1B,0x6E,0x5A,0xA0,0x52,0x3B,0xD6,0xB3,0x29,0xE3,0x2F,0x84,
  0x53,0xD1,0x00,0xED,0x20,0xFC,0xB1,0x5B,0x6A,0xCB,0xBE,0x39,0x4A,0x4C,0x58,0xCF,
  0xD0,0xEF,0xAA,0xFB,0x43,0x4D,0x33,0x85,0x45,0xF9,0x02,0x7F,0x50,0x3C,0x9F,0xA8,
  0x51,0xA3,0x40,0x8F,0x92,0x9D,0x38,0xF5,0xBC,0xB6,0xDA,0x21,0x10,0xFF,0xF3,0xD2,
  0xCD,0x0C,0x13,0xEC,0x5F,0x97,0x44,0x17,0xC4,0xA7,0x7E,0x3D,0x64,0x5D,0x19,0x73,
  0x60,0x81,0x4F,0xDC,0x22,0x2A,0x90,0x88,0x46,0xEE,0xB8,0x14,0xDE,0x5E,0x0B,0xDB,
  0xE0,0x32,0x3A,0x0A,0x49,0x06,0x24,0x5C,0xC2,0xD3,0xAC,0x62,0x91,0x95,0xE4,0x79,
  0xE7,0xC8,0x37,0x6D,0x8D,0xD5,0x4E,0xA9,0x6C,0x56,0xF4,0xEA,0x65,0x7A,0xAE,0x08,
  0xBA,0x78,0x25,0x2E,0x1C,0xA6,0xB4,0xC6,0xE8,0xDD,0x74,0x1F,0x4B,0xBD,0x8B,0x8A,
  0x70,0x3E,0xB5,0x66,0x48,0x03,0xF6,0x0E,0x61,0x35,0x57,0xB9,0x86,0xC1,0x1D,0x9E,
  0xE1,0xF8,0x98,0x11,0x69,0xD9,0x8E,0x94,0x9B,0x1E,0x87,0xE9,0xCE,0x55,0x28,0xDF,
  0x8C,0xA1,0x89,0x0D,0xBF,0xE6,0x42,0x68,0x41,0x99,0x2D,0x0F,0xB0,0x54,0xBB,0x16
};

/* Round constant: only first byte non-zero, rest implicit. */
static const uint8_t rcon[11] = {
  0x8d,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
};

/* ------------------------------------------------------------------ */
/*  Key expansion (FIPS-197 figure 11)                                 */
/* ------------------------------------------------------------------ */
void aes128_keyexp(aes128_ctx_t *ctx, const uint8_t key[AES_KEYLEN])
{
    uint8_t *rk = ctx->round_keys;
    int i;
    uint8_t t[4];

    for (i = 0; i < 16; i++) rk[i] = key[i];

    for (i = 16; i < AES_KEYEXPSIZE; i += 4) {
        t[0] = rk[i-4]; t[1] = rk[i-3]; t[2] = rk[i-2]; t[3] = rk[i-1];

        if ((i & 0xF) == 0) {
            /* RotWord + SubWord + Rcon */
            uint8_t k = t[0]; t[0] = t[1]; t[1] = t[2]; t[2] = t[3]; t[3] = k;
            t[0] = sbox[t[0]]; t[1] = sbox[t[1]]; t[2] = sbox[t[2]]; t[3] = sbox[t[3]];
            t[0] ^= rcon[i >> 4];
        }
        rk[i  ] = rk[i-16] ^ t[0];
        rk[i+1] = rk[i-15] ^ t[1];
        rk[i+2] = rk[i-14] ^ t[2];
        rk[i+3] = rk[i-13] ^ t[3];
    }
}

/* ------------------------------------------------------------------ */
/*  Round primitives                                                   */
/* ------------------------------------------------------------------ */
static __inline uint8_t xtime(uint8_t x)
{
    return (uint8_t)((x << 1) ^ ((x >> 7) & 1U ? 0x1B : 0));
}

static void sub_bytes(uint8_t s[16])
{
    int i; for (i = 0; i < 16; i++) s[i] = sbox[s[i]];
}

static void shift_rows(uint8_t s[16])
{
    uint8_t t;
    t = s[1];  s[1]  = s[5];  s[5]  = s[9];  s[9]  = s[13]; s[13] = t;
    t = s[2];  s[2]  = s[10]; s[10] = t;
    t = s[6];  s[6]  = s[14]; s[14] = t;
    t = s[15]; s[15] = s[11]; s[11] = s[7];  s[7]  = s[3];  s[3]  = t;
}

static void mix_columns(uint8_t s[16])
{
    int c;
    for (c = 0; c < 4; c++) {
        uint8_t *p = &s[c * 4];
        uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
        uint8_t t  = a0 ^ a1 ^ a2 ^ a3;
        p[0] ^= t ^ xtime(a0 ^ a1);
        p[1] ^= t ^ xtime(a1 ^ a2);
        p[2] ^= t ^ xtime(a2 ^ a3);
        p[3] ^= t ^ xtime(a3 ^ a0);
    }
}

static void add_round_key(uint8_t s[16], const uint8_t *rk)
{
    int i; for (i = 0; i < 16; i++) s[i] ^= rk[i];
}

/* ------------------------------------------------------------------ */
/*  Encrypt one block                                                  */
/* ------------------------------------------------------------------ */
void aes128_encrypt_block(const aes128_ctx_t *ctx, uint8_t block[AES_BLOCKLEN])
{
    const uint8_t *rk = ctx->round_keys;
    int r;

    add_round_key(block, rk); rk += 16;

    for (r = 1; r < 10; r++) {
        sub_bytes(block);
        shift_rows(block);
        mix_columns(block);
        add_round_key(block, rk); rk += 16;
    }
    /* final round (no MixColumns) */
    sub_bytes(block);
    shift_rows(block);
    add_round_key(block, rk);
}

/* ------------------------------------------------------------------ */
/*  CTR-mode XOR stream                                                */
/* ------------------------------------------------------------------ */
/* The nonce is treated as a big-endian 128-bit counter -- we increment
 * the low 64 bits which is enough; the EEPROM encrypted region is far
 * smaller than 2^64 bytes. */
static void incr_ctr(uint8_t n[AES_BLOCKLEN])
{
    int i;
    for (i = AES_BLOCKLEN - 1; i >= 8; i--) {
        if (++n[i] != 0) return;
    }
}

void aes128_ctr_xcrypt(const aes128_ctx_t *ctx,
                       uint8_t nonce[AES_BLOCKLEN],
                       uint8_t *buf, uint32_t len)
{
    uint8_t ks[AES_BLOCKLEN];
    while (len) {
        /* keystream block = E_K(nonce) */
        int i;
        for (i = 0; i < AES_BLOCKLEN; i++) ks[i] = nonce[i];
        aes128_encrypt_block(ctx, ks);

        uint32_t chunk = (len > AES_BLOCKLEN) ? AES_BLOCKLEN : len;
        for (i = 0; i < (int)chunk; i++) buf[i] ^= ks[i];

        buf += chunk;
        len -= chunk;
        incr_ctr(nonce);
    }
}
