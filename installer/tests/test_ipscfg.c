/*
 * Tests for source/ipscfg.c, on hand-built IPS files with a settings block
 * laid out as patches/<version>.toml writes it.
 */
#include "ipscfg.h"
#include "test.h"

/* "PATCH", a 4-byte code record, the 20-byte settings record, "EOF". The
 * block: magic, anchor 0x0000008826204000 (2018-07-13), mode 0. */
static const unsigned char PATCH_FILE[] = {
	'P', 'A', 'T', 'C', 'H',
	0x23, 0xF6, 0x0C, 0x00, 0x04, 0xC0, 0x3B, 0xF0, 0xEB,
	0x32, 0x37, 0xA0, 0x00, 0x14,
	'R', 'C', 'L', 'W', 'C', 'F', 'G', '1',
	0x00, 0x40, 0x20, 0x26, 0x88, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	'E', 'O', 'F',
};
#define BLOCK_AT 19 /* 5 + (5 + 4) + 5 */

static void find_and_read(void)
{
	CHECK(ipscfg_find(PATCH_FILE, sizeof(PATCH_FILE)) == BLOCK_AT);
	IpsSettings s;
	CHECK(ipscfg_read(PATCH_FILE, sizeof(PATCH_FILE), &s));
	CHECK(s.anchor_ms == 584755200000ull && s.mode == CATALOG_SPOTPASS);
}

static void write_values(void)
{
	unsigned char p[sizeof(PATCH_FILE)];
	memcpy(p, PATCH_FILE, sizeof(p));
	IpsSettings in = {600393600000ull, CATALOG_BUILTIN}; /* 2019-01-10 */
	CHECK(ipscfg_write(p, sizeof(p), &in));
	IpsSettings out;
	CHECK(ipscfg_read(p, sizeof(p), &out));
	CHECK(out.anchor_ms == in.anchor_ms && out.mode == CATALOG_BUILTIN);
	/* Little-endian u64 then u32, right after the magic. */
	CHECK(p[BLOCK_AT + 8] == 0x00 && p[BLOCK_AT + 9] == 0x4C && p[BLOCK_AT + 12] == 0x8B);
	CHECK(p[BLOCK_AT + 16] == 1);
	/* Nothing else moved. */
	CHECK(memcmp(p, PATCH_FILE, BLOCK_AT + 8) == 0);
	CHECK(memcmp(p + BLOCK_AT + 20, PATCH_FILE + BLOCK_AT + 20, sizeof(p) - BLOCK_AT - 20) == 0);

	IpsSettings bad = {0, 2}; /* unknown mode: refused, file unchanged */
	unsigned char q[sizeof(PATCH_FILE)];
	memcpy(q, PATCH_FILE, sizeof(q));
	CHECK(!ipscfg_write(q, sizeof(q), &bad));
	CHECK(memcmp(q, PATCH_FILE, sizeof(q)) == 0);
}

static void same_except_settings(void)
{
	unsigned char a[sizeof(PATCH_FILE)], b[sizeof(PATCH_FILE)];
	memcpy(a, PATCH_FILE, sizeof(a));
	memcpy(b, PATCH_FILE, sizeof(b));
	CHECK(ipscfg_same_except_settings(a, sizeof(a), b, sizeof(b)));
	IpsSettings s = {1, CATALOG_BUILTIN};
	ipscfg_write(b, sizeof(b), &s);
	CHECK(ipscfg_same_except_settings(a, sizeof(a), b, sizeof(b)));

	b[10] ^= 1; /* a code byte: another patch */
	CHECK(!ipscfg_same_except_settings(a, sizeof(a), b, sizeof(b)));
	b[10] ^= 1;
	b[BLOCK_AT + 3] ^= 1; /* the magic: no block at all in b */
	CHECK(!ipscfg_same_except_settings(a, sizeof(a), b, sizeof(b)));
	CHECK(!ipscfg_same_except_settings(a, sizeof(a), a, sizeof(a) - 1));
}

static void no_or_bad_block(void)
{
	IpsSettings s;
	/* Old patch, no block. */
	static const unsigned char old[] = "PATCH" "\x23\xF6\x0C" "\x00\x04" "abcd" "EOF";
	CHECK(ipscfg_find(old, sizeof(old) - 1) == -1);
	CHECK(!ipscfg_read(old, sizeof(old) - 1, &s));

	/* Not an IPS. */
	CHECK(ipscfg_find(PATCH_FILE, sizeof(PATCH_FILE) - 1) == -1);
	CHECK(ipscfg_find(NULL, 0) == -1);

	/* Magic cut short by the end of its record. */
	static const unsigned char cut[] = "PATCH" "\x32\x37\xA0" "\x00\x0C"
	                                   "RCLWCFG1" "\0\0\0\0" "EOF";
	CHECK(ipscfg_find(cut, sizeof(cut) - 1) == -1);

	/* Two blocks: the game reads one, the app can't know which. */
	unsigned char two[5 + 2 * 25 + 3];
	memcpy(two, "PATCH", 5);
	for (int i = 0; i < 2; i++)
	{
		unsigned char *r = two + 5 + 25 * i;
		r[0] = 0x32; r[1] = 0x37; r[2] = (unsigned char)(0xA0 + 0x20 * i); r[3] = 0; r[4] = 20;
		memcpy(r + 5, "RCLWCFG1", 8);
		memset(r + 13, 0, 12);
	}
	memcpy(two + 55, "EOF", 3);
	CHECK(ipscfg_find(two, sizeof(two)) == -1);

	/* Off a word boundary inside the record: not a block. */
	static const unsigned char odd[] = "PATCH" "\x32\x37\xA0" "\x00\x16"
	                                   "xxRCLWCFG1" "\0\0\0\0\0\0\0\0\0\0\0\0" "EOF";
	CHECK(ipscfg_find(odd, sizeof(odd) - 1) == -1);
}

void test_ipscfg(void)
{
	find_and_read();
	write_values();
	same_except_settings();
	no_or_bad_block();
}
