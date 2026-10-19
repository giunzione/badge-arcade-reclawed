/*
 * checks - decide whether the patch can be installed, and explain why not.
 *
 * Pure logic: probe.c (3DS only) fills a SystemState snapshot, checks_run()
 * turns it into a list of lines for the screen plus the install decision.
 * Unit-tested on the PC with hand-made snapshots (tests/test_checks.c).
 */
#ifndef CHECKS_H
#define CHECKS_H

#include <stdbool.h>

#include "luma_config.h"
#include "titles.h"

typedef enum
{
	PATCH_NOT_ON_SD, /* no code.ips for this title on the SD */
	PATCH_SAME,      /* the SD holds exactly the bundled patch */
	PATCH_DIFFERENT, /* the SD holds some other code.ips */
} PatchState;

/* What the probe found on the console. */
typedef struct
{
	/* Luma3DS, from luma_system.c */
	bool luma_detected;
	bool luma_on_sd;          /* only meaningful when detected */
	bool patching_active;     /* as booted; only meaningful when detected */
	unsigned luma_version[3];

	/* sdmc:/luma/config.ini */
	bool config_found;
	LumaCfgStatus config_status;
	int config_game_patching; /* 0/1 when config_status is LUMACFG_OK */

	/* One entry per GAME_TITLES row */
	struct
	{
		bool installed;          /* base title on the SD */
		bool update_installed;
		unsigned update_version;
		bool patch_bundled;      /* a code.ips for it is in the app */
		PatchState sd_patch;     /* PATCH_DIFFERENT when not bundled but a code.ips is there */
	} game[GAME_TITLE_COUNT];
} SystemState;

typedef enum
{
	CHECK_OK,    /* fine as it is */
	CHECK_INFO,  /* something the install will do or the user should know */
	CHECK_WARN,  /* odd, but install is still allowed */
	CHECK_STOP,  /* install is not possible */
} CheckLevel;

#define CHECK_TEXT_LEN 43 /* 42 chars + the 7-char tag = one 50-column line */
#define CHECK_MAX_ITEMS 12

typedef struct
{
	CheckLevel level;
	char text[CHECK_TEXT_LEN];
} CheckItem;

typedef struct
{
	CheckItem items[CHECK_MAX_ITEMS];
	int count;
	int target;          /* GAME_TITLES index to install for, or -1 */
	bool can_install;    /* no CHECK_STOP and a target was found */
	bool nothing_to_do;  /* patch already there and game patching already on */
	int uninstall_target; /* GAME_TITLES index whose code.ips is on the SD, or -1 */
} CheckReport;

void checks_run(const SystemState *s, CheckReport *out);

#endif
