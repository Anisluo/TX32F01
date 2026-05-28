/*
 * eeprom_crypto.h
 *
 * Minimal AES-128 + CTR-mode helpers used by eeprom_storage to make
 * the host-visible "encrypted region" actually encrypted at rest.
 *
 *   - aes128_keyexp: derive 11 round keys from a 16-byte master key
 *   - aes128_encrypt_block: encrypt one 16-byte block
 *   - aes128_ctr_xcrypt: stream cipher over arbitrary length using
 *                        AES-CTR. Reversible: same call decrypts.
 *
 * Algorithm choice:
 *
 *   - AES-128 because uECC and other "real" crypto would bloat the
 *     8 KB code budget. AES-128 fits in ~1.5 KB.
 *   - CTR mode because it lets us encrypt arbitrary byte offsets and
 *     read/write any byte in the encrypted region independently; ECB
 *     would force 16-byte alignment, XTS would need two keys, CBC
 *     would force whole-block re-write on partial updates.
 *
 * Key derivation (in eeprom_storage):
 *
 *   master_key[16] = SHA-128-truncated( chip_die_id || domain_string )
 *
 * SHA is overkill -- we just use a quick mixing function over the
 * 64-bit Die ID + a domain literal. Strength is "anyone who hasn't
 * read this MCU's silicon will see plausible noise"; not military
 * grade, but enough to defeat casual EEPROM cloning attacks.
 *
 * Performance: AES-128 round on Cortex-M0 @ 24 MHz with this code
 * is ~3500 cycles. 16-byte block = ~145 us. A 64-byte page program
 * therefore adds ~580 us of crypto on top of the Flash 40 ms erase
 * window -- negligible.
 */
#ifndef APP_I2CEEPROM_EEPROM_CRYPTO_H
#define APP_I2CEEPROM_EEPROM_CRYPTO_H

#include <stdint.h>

#define AES_BLOCKLEN     16U
#define AES_KEYLEN       16U
#define AES_KEYEXPSIZE   176U     /* 11 round keys * 16 bytes */

typedef struct {
    uint8_t round_keys[AES_KEYEXPSIZE];
} aes128_ctx_t;

/* One-time key schedule from a 16-byte master key. */
void aes128_keyexp(aes128_ctx_t *ctx, const uint8_t key[AES_KEYLEN]);

/* In-place encrypt one 16-byte block. */
void aes128_encrypt_block(const aes128_ctx_t *ctx, uint8_t block[AES_BLOCKLEN]);

/* AES-CTR XOR over arbitrary length. Same call encrypts or decrypts.
 *
 *   ctx        : key schedule (from aes128_keyexp)
 *   nonce      : 16-byte CTR initial value. For our use we synthesise
 *                this from (region_id, byte_addr) so each byte has a
 *                deterministic keystream position -- meaning re-reads
 *                of the same address return the same plaintext, and
 *                two different addresses never share keystream.
 *   buf, len   : input/output (in-place)
 *
 * The nonce IS modified internally; callers should rebuild it for
 * each new chunk.
 */
void aes128_ctr_xcrypt(const aes128_ctx_t *ctx,
                       uint8_t nonce[AES_BLOCKLEN],
                       uint8_t *buf, uint32_t len);

#endif /* APP_I2CEEPROM_EEPROM_CRYPTO_H */
