/*
 * titles - the Nintendo Badge Arcade releases the installer knows about.
 *
 * Title IDs and update versions come from Nintendo's eShop catalog, as listed
 * by 3dsdb (github.com/hax0kartik/3dsdb, jsons/list_{GB,US,JP}.json). There is
 * no Korean release.
 *
 * Which versions are *supported* is not decided here: the patch manifest
 * (manifest.h, built from patches/) lists one patch per title ID + version.
 */
#ifndef TITLES_H
#define TITLES_H

#include <stdint.h>

typedef struct
{
	const char *region;      /* "EUR", "USA", "JPN" - same names as [meta] region */
	uint64_t base_tid;       /* the game; luma/titles/<base_tid>/code.ips */
	uint64_t update_tid;     /* its update title (0004000E...) */
	uint16_t update_version; /* title version of v1.3.1, the final update: only
	                          * used to name it on screen; which versions have
	                          * a patch comes from the manifest */
} GameTitle;

#define GAME_TITLE_COUNT 3
extern const GameTitle GAME_TITLES[GAME_TITLE_COUNT];

/* Index in GAME_TITLES of the title with this base ID (as 16 hex digits,
 * either case), or -1. */
int titles_find(const char *base_tid_hex);

#endif
