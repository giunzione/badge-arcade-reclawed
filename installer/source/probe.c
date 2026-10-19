/*
 * probe - see probe.h.
 *
 * Build with `make FAKE_GAME=1` to pretend Badge Arcade EUR v1.3.1 is
 * installed, so the whole install flow can be tried in an emulator without
 * the game. Never ship such a build.
 */
#include "probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include "fileops.h"
#include "luma_config.h"
#include "luma_system.h"

void probe_patch_paths(int index, char *bundled, char *on_sd, size_t len)
{
	unsigned long long tid = GAME_TITLES[index].base_tid;
	snprintf(bundled, len, BUNDLED_DIR "/%016llX/code.ips", tid);
	snprintf(on_sd, len, SD_TITLES_DIR "/%016llX/code.ips", tid);
}

/* Is the title installed on the SD, and with which version? */
static bool title_version(u64 tid, unsigned *version)
{
	AM_TitleEntry entry;
	if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &tid, &entry)))
		return false;
	*version = entry.version;
	return true;
}

static void probe_luma(SystemState *s)
{
	LumaSystem luma;
	luma_system_query(&luma);
	s->luma_detected = luma.detected;
	s->luma_on_sd = luma.config_on_sd;
	s->patching_active = luma.game_patching_active;
	s->luma_version[0] = luma.major;
	s->luma_version[1] = luma.minor;
	s->luma_version[2] = luma.build;

	unsigned char *text;
	size_t size;
	s->config_found = fops_read(LUMA_CONFIG, &text, &size, FOPS_MAX_READ) == 0;
	if (s->config_found)
	{
		LumaCfgInfo info;
		s->config_status = lumacfg_parse((const char *)text, size, &info);
		s->config_game_patching = info.game_patching;
		free(text);
	}
}

static void probe_game(SystemState *s, int i)
{
	unsigned base_version;
	s->game[i].installed = title_version(GAME_TITLES[i].base_tid, &base_version);
	s->game[i].update_installed =
		title_version(GAME_TITLES[i].update_tid, &s->game[i].update_version);

#ifdef RECLAWED_FAKE_GAME
	if (strcmp(GAME_TITLES[i].region, "EUR") == 0)
	{
		s->game[i].installed = true;
		s->game[i].update_installed = true;
		s->game[i].update_version = GAME_TITLES[i].update_version;
	}
#endif

	char bundled[96], on_sd[96];
	probe_patch_paths(i, bundled, on_sd, sizeof(bundled));
	unsigned char *ips;
	size_t ips_size;
	s->game[i].patch_bundled = fops_read(bundled, &ips, &ips_size, FOPS_MAX_READ) == 0;
	if (!s->game[i].patch_bundled)
	{
		/* Nothing to compare with, but a leftover patch can still be removed. */
		s->game[i].sd_patch = fops_exists(on_sd) ? PATCH_DIFFERENT : PATCH_NOT_ON_SD;
		return;
	}
	if (fops_same_content(on_sd, ips, ips_size))
		s->game[i].sd_patch = PATCH_SAME;
	else if (fops_exists(on_sd))
		s->game[i].sd_patch = PATCH_DIFFERENT;
	else
		s->game[i].sd_patch = PATCH_NOT_ON_SD;
	free(ips);
}

void probe_system(SystemState *s)
{
	memset(s, 0, sizeof(*s));
	probe_luma(s);
	for (int i = 0; i < GAME_TITLE_COUNT; i++)
		probe_game(s, i);
}
