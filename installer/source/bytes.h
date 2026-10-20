/*
 * bytes - little-endian integers in byte buffers.
 *
 * The SARC archive (spotpass.c) and the patch's settings block (ipscfg.c)
 * are little-endian, as the 3DS is; these helpers read and write them byte
 * by byte, so they work on any host and at any alignment.
 */
#ifndef BYTES_H
#define BYTES_H

#include <stdint.h>

/* Returns the little-endian 16-bit value at `p`. */
static inline uint32_t le16(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

/* Returns the little-endian 32-bit value at `p`. */
static inline uint32_t le32(const unsigned char *p)
{
	return le16(p) | le16(p + 2) << 16;
}

/* Writes `v` at `p` as a little-endian 32-bit value. */
static inline void put_le32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
}

#endif
