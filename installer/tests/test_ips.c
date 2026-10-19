/*
 * Tests for source/ips.c, on hand-built byte strings.
 */
#include "ips.h"
#include "test.h"

#define IPS(s) (const unsigned char *)(s), sizeof(s) - 1

void test_ips(void)
{
	unsigned n = 99;

	/* No records at all is still a well-formed file. */
	CHECK(ips_is_valid(IPS("PATCHEOF"), &n) && n == 0);

	/* Two records: offset 0x000010 size 2, offset 0x000020 size 1. */
	CHECK(ips_is_valid(IPS("PATCH" "\x00\x00\x10" "\x00\x02" "ab"
	                        "\x00\x00\x20" "\x00\x01" "c" "EOF"), &n));
	CHECK(n == 2);

	/* A record whose offset spells "EOF" is data, not the footer. */
	CHECK(ips_is_valid(IPS("PATCH" "EOF" "\x00\x01" "z" "EOF"), &n) && n == 1);

	/* Records may be NULL. */
	CHECK(ips_is_valid(IPS("PATCHEOF"), NULL));

	CHECK(!ips_is_valid(IPS(""), NULL));
	CHECK(!ips_is_valid(IPS("PATCH"), NULL));                 /* no footer */
	CHECK(!ips_is_valid(IPS("PATHCEOF"), NULL));              /* bad header */
	CHECK(!ips_is_valid(IPS("PATCHEOFx"), NULL));             /* data after footer */
	CHECK(!ips_is_valid(IPS("PATCH" "\x00\x00\x10" "\x00\x05" "ab" "EOF"), NULL)); /* truncated */
	CHECK(!ips_is_valid(IPS("PATCH" "\x00\x00\x10" "\x00\x00" "\x00\x04" "x" "EOF"), NULL)); /* RLE */
}
