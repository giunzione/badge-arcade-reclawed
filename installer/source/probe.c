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

/* The app's own patches (RomFS), loaded once. */
static Manifest builtin;
static bool builtin_loaded, builtin_ok;

/* The patch chosen for each game by probe_resolve(). */
static struct
{
	const unsigned char *data;
	size_t size;
	unsigned char *owned; /* built-in copy we must free; online data belongs to the job */
} chosen[GAME_TITLE_COUNT];

void probe_sd_patch_path(int index, char *out, size_t len)
{
	snprintf(out, len, SD_TITLES_DIR "/%016llX/code.ips",
	         (unsigned long long)GAME_TITLES[index].base_tid);
}

void probe_sd_record_path(int index, char *out, size_t len)
{
	snprintf(out, len, SD_TITLES_DIR "/%016llX/" INSTALLED_FILE,
	         (unsigned long long)GAME_TITLES[index].base_tid);
}

const unsigned char *probe_patch_data(int index, size_t *size)
{
	*size = chosen[index].size;
	return chosen[index].data;
}

void probe_free(void)
{
	for (int i = 0; i < GAME_TITLE_COUNT; i++)
	{
		free(chosen[i].owned);
		memset(&chosen[i], 0, sizeof(chosen[i]));
	}
}

static void load_builtin(void)
{
	if (builtin_loaded)
		return;
	builtin_loaded = true;
	unsigned char *text;
	size_t size;
	if (fops_read(BUILTIN_DIR "/patches.txt", &text, &size, FOPS_MAX_READ) == 0)
	{
		builtin_ok = manifest_parse((const char *)text, size, &builtin) == MANIFEST_OK;
		free(text);
	}
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
	s->game[i].installed = title_version(GAME_TITLES[i].base_tid, &s->game[i].base_version);
	s->game[i].update_installed =
		title_version(GAME_TITLES[i].update_tid, &s->game[i].update_version);

#ifdef RECLAWED_FAKE_GAME
	/* RECLAWED_FAKE_VERSION (make FAKE_VERSION=n) fakes another update version,
	 * e.g. one no patch exists for; 0 fakes "no update installed". */
#ifndef RECLAWED_FAKE_VERSION
#define RECLAWED_FAKE_VERSION GAME_TITLES[i].update_version
#endif
	if (strcmp(GAME_TITLES[i].region, "EUR") == 0)
	{
		s->game[i].installed = true;
		s->game[i].base_version = 4096;
		s->game[i].update_installed = RECLAWED_FAKE_VERSION != 0;
		s->game[i].update_version = RECLAWED_FAKE_VERSION;
	}
#endif

	/* The update's code runs whenever the update is installed. */
	if (s->game[i].update_installed)
	{
		s->game[i].code = CODE_UPDATE;
		s->game[i].code_version = s->game[i].update_version;
	}
	else
	{
		s->game[i].code = CODE_BASE;
		s->game[i].code_version = s->game[i].base_version;
	}
}

void probe_system(SystemState *s)
{
	memset(s, 0, sizeof(*s));
	load_builtin();
	if (builtin_ok)
		snprintf(s->builtin_release, sizeof(s->builtin_release), "%s", builtin.release);
	s->online = ONLINE_CHECKING;
	probe_luma(s);
	for (int i = 0; i < GAME_TITLE_COUNT; i++)
		probe_game(s, i);
}

void probe_online_wants(const SystemState *s, OnlineJob *job)
{
	for (int i = 0; i < GAME_TITLE_COUNT; i++)
	{
		job->want[i].wanted = s->game[i].installed;
		job->want[i].base_tid = GAME_TITLES[i].base_tid;
		job->want[i].code = s->game[i].code;
		job->want[i].version = s->game[i].code_version;
	}
}

/* The built-in file for an entry, if it is there and matches its hash. */
static unsigned char *read_builtin(const PatchEntry *e, size_t *size)
{
	char path[96];
	unsigned char *data;
	snprintf(path, sizeof(path), BUILTIN_DIR "/%s", e->file);
	if (fops_read(path, &data, size, MANIFEST_MAX_PATCH_SIZE) != 0)
		return NULL;
	if (!manifest_entry_matches(e, data, *size))
	{
		free(data);
		return NULL;
	}
	return data;
}

/* Pick the patch for game `i`: the latest release's, else the built-in one. */
static void choose_patch(SystemState *s, OnlineJob *job, int i)
{
	const PatchEntry *e;
	PatchSource src = manifest_choose(job->manifest_ok ? &job->manifest : NULL, job->got[i].ok,
	                                  builtin_ok ? &builtin : NULL, GAME_TITLES[i].base_tid,
	                                  s->game[i].code, s->game[i].code_version, &e);
	if (src == SOURCE_ONLINE)
	{
		chosen[i].data = job->got[i].data;
		chosen[i].size = job->got[i].size;
		snprintf(s->game[i].patch_release, sizeof(s->game[i].patch_release), "%s",
		         job->manifest.release);
	}
	else if (src == SOURCE_BUILTIN)
	{
		chosen[i].owned = read_builtin(e, &chosen[i].size);
		chosen[i].data = chosen[i].owned;
		if (!chosen[i].data)
			src = SOURCE_NONE; /* damaged build: behave as if it had no patch */
		else
			snprintf(s->game[i].patch_release, sizeof(s->game[i].patch_release), "%s",
			         builtin.release);
	}
	s->game[i].source = src;
}

/* Compare the code.ips on the SD with the chosen patch, using the record the
 * app left next to it (installed.h). */
static void classify_sd_patch(SystemState *s, int i, const char *sd_path)
{
	unsigned char *sd = NULL, *text = NULL;
	size_t sd_size = 0, text_size = 0;
	bool on_sd = fops_exists(sd_path);
	char sd_hex[SHA256_HEX_LEN + 1] = "";
	if (on_sd && fops_read(sd_path, &sd, &sd_size, FOPS_MAX_READ) == 0)
		sha256_hex(sd, sd_size, sd_hex);
	bool same = sd && chosen[i].data && sd_size == chosen[i].size &&
	            memcmp(sd, chosen[i].data, sd_size) == 0;

	char rec_path[96];
	InstalledRecord rec;
	probe_sd_record_path(i, rec_path, sizeof(rec_path));
	bool have_rec = on_sd && fops_read(rec_path, &text, &text_size, INSTALLED_MAX_SIZE) == 0 &&
	                installed_parse((const char *)text, text_size, &rec);

	s->game[i].sd_patch = installed_classify(on_sd, same, have_rec ? &rec : NULL, sd_hex,
	                                         s->game[i].patch_release, s->game[i].code,
	                                         s->game[i].code_version);
	if (have_rec && strcmp(rec.sha256, sd_hex) == 0)
		snprintf(s->game[i].sd_release, sizeof(s->game[i].sd_release), "%s", rec.release);
	free(sd);
	free(text);
}

void probe_resolve(SystemState *s, OnlineJob *job)
{
	probe_free();
	bool done = job && online_done(job);
	if (!done)
		s->online = ONLINE_CHECKING;
	else if (job->manifest_ok)
		s->online = ONLINE_OK;
	else
	{
		s->online = ONLINE_FAILED;
		s->online_error = job->status;
	}

	for (int i = 0; i < GAME_TITLE_COUNT; i++)
	{
		char sd_path[96];
		probe_sd_patch_path(i, sd_path, sizeof(sd_path));
		s->game[i].source = SOURCE_NONE;
		s->game[i].fetch = done && job->manifest_ok ? job->got[i].result : FETCH_NOT_TRIED;
		s->game[i].patch_release[0] = '\0';
		s->game[i].sd_release[0] = '\0';
		if (s->game[i].installed && done)
			choose_patch(s, job, i);
		classify_sd_patch(s, i, sd_path);
	}
}
