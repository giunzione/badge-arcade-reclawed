/*
 * Badge Arcade Reclawed installer - entry point and text UI.
 *
 * Shows the state of the console (Luma3DS, game patching, bundled patches)
 * and, on A, runs the install:
 *   1. copy every bundled romfs:/luma/titles/<title id>/code.ips to the same
 *      path on the SD card;
 *   2. set enable_game_patching = 1 in sdmc:/luma/config.ini.
 * Checks on the installed game (region, version) come in a later step.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include "fileops.h"
#include "installer.h"
#include "luma_config.h"
#include "luma_system.h"

#ifndef APP_VERSION
#define APP_VERSION "dev" /* normally set by the Makefile from VERSION */
#endif

#define BUNDLED_DIR  "romfs:/luma/titles"
#define SD_DIR       "sdmc:/luma/titles"
#define LUMA_CONFIG  "sdmc:/luma/config.ini"
#define CONFIG_BACKUP "sdmc:/3ds/badge-arcade-reclawed/config.ini.bak"

#define MAX_PATCHES 8
#define TITLE_ID_LEN 16

/* Title IDs (folder names) of the patches bundled in the RomFS. */
static char patches[MAX_PATCHES][TITLE_ID_LEN + 1];
static int patch_count;

static PrintConsole top, bottom;

/* ANSI colors understood by the libctru console. */
#define C_RESET "\x1b[0m"
#define C_GREEN "\x1b[32;1m"
#define C_RED   "\x1b[31;1m"
#define C_YELLOW "\x1b[33;1m"

static void find_bundled_patches(void)
{
	patch_count = 0;
	DIR *d = opendir(BUNDLED_DIR);
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) && patch_count < MAX_PATCHES)
	{
		if (strlen(e->d_name) != TITLE_ID_LEN)
			continue;
		char path[96];
		snprintf(path, sizeof(path), BUNDLED_DIR "/%s/code.ips", e->d_name);
		if (fops_exists(path))
			strcpy(patches[patch_count++], e->d_name);
	}
	closedir(d);
}

static void patch_paths(int i, char *src, char *dst, size_t len)
{
	snprintf(src, len, BUNDLED_DIR "/%.16s/code.ips", patches[i]);
	snprintf(dst, len, SD_DIR "/%.16s/code.ips", patches[i]);
}

/* Top screen: what the installer sees right now. */
static void draw_status(const LumaSystem *luma)
{
	consoleSelect(&top);
	consoleClear();
	printf("Badge Arcade Reclawed installer v%s\n\n", APP_VERSION);

	printf("Luma3DS:        ");
	if (luma->detected)
		printf(C_GREEN "v%u.%u.%u" C_RESET "\n", luma->major, luma->minor, luma->build);
	else
		printf(C_YELLOW "not detected" C_RESET "\n");

	printf("Game patching:  ");
	if (luma->detected)
		printf(luma->game_patching_active ? C_GREEN "active" C_RESET "\n"
		                                  : C_YELLOW "not active" C_RESET "\n");
	else
		printf("unknown\n");

	/* What the config file will say at the next boot. */
	printf("config.ini:     ");
	unsigned char *text;
	size_t size;
	if (fops_read(LUMA_CONFIG, &text, &size, FOPS_MAX_READ) != 0)
		printf(C_YELLOW "not found" C_RESET "\n");
	else
	{
		LumaCfgInfo info;
		LumaCfgStatus st = lumacfg_parse((const char *)text, size, &info);
		if (st != LUMACFG_OK)
			printf(C_RED "%s" C_RESET "\n", lumacfg_status_str(st));
		else
			printf("v%d.%d, game patching %s\n", info.version_major, info.version_minor,
			       info.game_patching ? C_GREEN "on" C_RESET : C_YELLOW "off" C_RESET);
		free(text);
	}

	printf("\nBundled patches:\n");
	if (patch_count == 0)
		printf(C_RED "  none (broken build?)" C_RESET "\n");
	for (int i = 0; i < patch_count; i++)
	{
		char src[96], dst[96];
		patch_paths(i, src, dst, sizeof(src));
		unsigned char *ips;
		size_t ips_size;
		const char *state = C_YELLOW "not installed" C_RESET;
		if (fops_read(src, &ips, &ips_size, FOPS_MAX_READ) == 0)
		{
			if (fops_same_content(dst, ips, ips_size))
				state = C_GREEN "installed" C_RESET;
			else if (fops_exists(dst))
				state = C_YELLOW "other version on SD" C_RESET;
			free(ips);
		}
		printf("  %s  %s\n", patches[i], state);
	}
}

static void print_result(const char *what, InstResult r, const char *msg)
{
	const char *tag = r == INST_DONE    ? C_GREEN "[done]" C_RESET
	                : r == INST_ALREADY ? C_GREEN "[ ok ]" C_RESET
	                                    : C_RED "[fail]" C_RESET;
	printf("%s %s:\n       %s\n", tag, what, msg);
}

/* Bottom screen: run both install steps and report each one. */
static void run_install(const LumaSystem *luma)
{
	consoleSelect(&bottom);
	consoleClear();

	if (luma->detected && !luma->config_on_sd)
	{
		printf(C_RED "Luma3DS keeps its files on the\n"
		       "internal memory (CTRNAND) on this\n"
		       "console. Not supported: install\n"
		       "the patch by hand." C_RESET "\n");
		return;
	}

	char msg[96];
	bool ok = patch_count > 0;
	for (int i = 0; i < patch_count; i++)
	{
		char src[96], dst[96];
		patch_paths(i, src, dst, sizeof(src));
		InstResult r = inst_copy_patch(src, dst, msg, sizeof(msg));
		print_result(patches[i], r, msg);
		ok = ok && r != INST_FAILED;
	}

	InstResult r = inst_enable_game_patching(LUMA_CONFIG, CONFIG_BACKUP, msg, sizeof(msg));
	print_result("Luma3DS config", r, msg);
	ok = ok && r != INST_FAILED;

	if (!ok)
		printf("\n" C_RED "Install incomplete." C_RESET "\n");
	else if (r == INST_DONE || (luma->detected && !luma->game_patching_active))
		printf("\n" C_GREEN "Done." C_RESET " Restart the console to\n"
		       "apply. Don't save Rosalina settings\n"
		       "before restarting: that would turn\n"
		       "game patching off again.\n");
	else
		printf("\n" C_GREEN "Done." C_RESET " Everything is in place.\n");
}

static void draw_controls(void)
{
	printf("\n(A) install    (START) exit\n");
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, &top);
	consoleInit(GFX_BOTTOM, &bottom);

	/* RomFS holds the patches; without it there is nothing to install. */
	bool romfs_ok = R_SUCCEEDED(romfsInit());
	if (romfs_ok)
		find_bundled_patches();

	LumaSystem luma;
	luma_system_query(&luma);
	draw_status(&luma);
	consoleSelect(&bottom);
	if (!romfs_ok)
		printf(C_RED "Can't open the app's RomFS." C_RESET "\n");
	draw_controls();

	while (aptMainLoop())
	{
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_START)
			break;
		if ((down & KEY_A) && patch_count > 0)
		{
			run_install(&luma);
			draw_status(&luma); /* refresh "installed" / config.ini lines */
			consoleSelect(&bottom);
			draw_controls();
		}

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	if (romfs_ok)
		romfsExit();
	gfxExit();
	return 0;
}
