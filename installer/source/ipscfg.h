/*
 * ipscfg - the settings block inside the patch (code.ips).
 *
 * Every patch carries one 20-byte block that the game code reads at run time
 * (the "Settings block" entry of each patches/<version>.toml):
 *
 *     +0x00  "RCLW" "CFG1"   magic and format
 *     +0x08  anchor          first day of the SpotPass week, ms since
 *                            2000-01-01 (u64, little-endian)
 *     +0x10  catalog mode    0 = SpotPass week (boss.sarc), 1 = built-in
 *
 * The block is found by its magic among the IPS records, so the app needs
 * no table of addresses. The installer writes the values for the console
 * before saving code.ips. As built, the block holds no date and selects the
 * built-in machines (patches/<version>.toml).
 *
 * Pure byte handling, unit-tested in tests/test_ipscfg.c.
 */
#ifndef IPSCFG_H
#define IPSCFG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IPSCFG_SIZE 20

typedef enum
{
	CATALOG_SPOTPASS = 0, /* the week cached in boss.sarc */
	CATALOG_BUILTIN = 1,  /* the game's own RomFS catalog */
} CatalogMode;

typedef struct
{
	uint64_t anchor_ms;
	uint32_t mode; /* CatalogMode */
} IpsSettings;

/* Returns the offset of the settings block in the `size`-byte IPS file `ips`,
 * or -1 if the file is not a valid IPS or holds no block (or more than one). */
long ipscfg_find(const unsigned char *ips, size_t size);

/* Reads the settings block of the IPS `ips` into `out`.
 *
 * Returns false if the file has no block (see ipscfg_find()). */
bool ipscfg_read(const unsigned char *ips, size_t size, IpsSettings *out);

/* Writes `in` into the settings block of the IPS `ips`, in place.
 *
 * Returns false, leaving the file unchanged, if it has no block or the mode
 * is not a known one. */
bool ipscfg_write(unsigned char *ips, size_t size, const IpsSettings *in);

/* Returns true if the patches `a` and `b` are the same file apart from their
 * settings: both have a block, at the same place, and every other byte
 * (the magic included) is equal. */
bool ipscfg_same_except_settings(const unsigned char *a, size_t a_size, const unsigned char *b,
                                 size_t b_size);

#endif
