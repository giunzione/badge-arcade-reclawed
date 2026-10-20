/*
 * ipscfg - see ipscfg.h.
 */
#include "ipscfg.h"

#include <string.h>

#include "ips.h"

static const unsigned char MAGIC[8] = {'R', 'C', 'L', 'W', 'C', 'F', 'G', '1'};

static uint32_t get32(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
}

long ipscfg_find(const unsigned char *ips, size_t size)
{
	if (!ips || !ips_is_valid(ips, size, NULL))
		return -1;
	long found = -1;
	/* Walk the records (ips_is_valid() checked their bounds); the block
	 * starts on a word boundary of a record's data, like all patch code. */
	for (size_t i = 5; i + 3 < size;)
	{
		size_t len = ((size_t)ips[i + 3] << 8) | ips[i + 4];
		const unsigned char *data = ips + i + 5;
		for (size_t at = 0; at + IPSCFG_SIZE <= len; at += 4)
			if (memcmp(data + at, MAGIC, sizeof(MAGIC)) == 0)
			{
				if (found >= 0)
					return -1; /* two blocks: don't guess which one the game reads */
				found = (long)(i + 5 + at);
			}
		i += 5 + len;
	}
	return found;
}

bool ipscfg_read(const unsigned char *ips, size_t size, IpsSettings *out)
{
	long at = ipscfg_find(ips, size);
	if (at < 0)
		return false;
	const unsigned char *p = ips + at;
	out->anchor_ms = (uint64_t)get32(p + 8) | (uint64_t)get32(p + 12) << 32;
	out->mode = get32(p + 16);
	return true;
}

bool ipscfg_write(unsigned char *ips, size_t size, const IpsSettings *in)
{
	if (in->mode != CATALOG_SPOTPASS && in->mode != CATALOG_BUILTIN)
		return false;
	long at = ipscfg_find(ips, size);
	if (at < 0)
		return false;
	unsigned char *p = ips + at;
	put32(p + 8, (uint32_t)in->anchor_ms);
	put32(p + 12, (uint32_t)(in->anchor_ms >> 32));
	put32(p + 16, in->mode);
	return true;
}

bool ipscfg_same_except_settings(const unsigned char *a, size_t a_size, const unsigned char *b,
                                 size_t b_size)
{
	if (a_size != b_size)
		return false;
	long at = ipscfg_find(a, a_size);
	if (at < 0 || ipscfg_find(b, b_size) != at)
		return false;
	/* Magic included in the comparison: only the values may differ. */
	size_t values = (size_t)at + 8;
	return memcmp(a, b, values) == 0 &&
	       memcmp(a + values + 12, b + values + 12, a_size - values - 12) == 0;
}
