/*
 * sha256 - SHA-256 (FIPS 180-4), to check downloaded and bundled patches
 * against the hashes in the patch manifest.
 *
 * Small and dependency-free so the same code runs on the 3DS and in the PC
 * unit tests (tests/test_manifest.c checks it against the NIST test vectors).
 */
#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

#define SHA256_LEN 32
#define SHA256_HEX_LEN (2 * SHA256_LEN) /* without the NUL */

/* Writes the SHA-256 of the `size` bytes at `data` into `out`. */
void sha256(const void *data, size_t size, uint8_t out[SHA256_LEN]);

/* Writes the SHA-256 of the `size` bytes at `data` into `out` as 64
 * lower-case hex digits and a NUL. */
void sha256_hex(const void *data, size_t size, char out[SHA256_HEX_LEN + 1]);

#endif
