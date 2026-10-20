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
#include "ipscfg.h"
#include "luma_config.h"
#include "luma_system.h"
#include "spotpass.h"

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

/* Parses the app's own patches.txt (RomFS) the first time it is needed. */
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

/* Luma3DS as booted, and its config.ini on the SD. */
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

/* Read `len` bytes at `offset` of an open file; true if all of them came. */
static bool read_at(Handle f, u64 offset, void *buf, u32 len)
{
	u32 got = 0;
	return R_SUCCEEDED(FSFILE_Read(f, &got, offset, buf, len)) && got == len;
}

/* "Not found" from FS: the generic description, or FS's own not-found range
 * (descriptions 100-179). Any other failure (e.g. access denied) must not
 * pass for "no boss.sarc", or the user would silently lose the SpotPass week. */
static bool not_found(Result r)
{
	u32 d = R_DESCRIPTION(r);
	return d == RD_NOT_FOUND || (d >= 100 && d < 180);
}

/* Which SpotPass week the game's extdata holds: boss.sarc's Schedule.xml,
 * read in place (the archive is ~25 MB; only its header and that file are
 * read). */
static void probe_spotpass(SpotPassWeek *w, u32 extdata_id)
{
	u32 path[3] = {MEDIATYPE_SD, extdata_id, 0};
	FS_Archive arch;
	FS_Path where = {PATH_BINARY, sizeof(path), path};
	Result r = FSUSER_OpenArchive(&arch, ARCHIVE_EXTDATA, where);
	if (R_FAILED(r))
	{
		w->status = not_found(r) ? SPOTPASS_MISSING : SPOTPASS_UNREADABLE;
		return;
	}
	Handle f;
	r = FSUSER_OpenFile(&f, arch, fsMakePath(PATH_ASCII, SPOTPASS_FILE), FS_OPEN_READ, 0);
	if (R_FAILED(r))
	{
		w->status = not_found(r) ? SPOTPASS_MISSING : SPOTPASS_UNREADABLE;
		FSUSER_CloseArchive(arch);
		return;
	}

	unsigned char head[SARC_HEADER_MIN], *hdr = NULL;
	char *xml = NULL;
	u64 file_size = 0;
	uint32_t hdr_size, start, end;
	w->status = SPOTPASS_UNREADABLE;
	if (R_FAILED(FSFILE_GetSize(f, &file_size)) || !read_at(f, 0, head, sizeof(head)))
		goto done;
	w->status = SPOTPASS_NOT_SARC;
	if (!sarc_header_size(head, sizeof(head), &hdr_size) || hdr_size > file_size ||
	    !(hdr = malloc(hdr_size)))
		goto done;
	w->status = SPOTPASS_UNREADABLE;
	if (!read_at(f, 0, hdr, hdr_size))
		goto done;
	w->status = SPOTPASS_NO_SCHEDULE;
	if (!sarc_find(hdr, hdr_size, SPOTPASS_SCHEDULE, &start, &end) || end > file_size ||
	    end - start > SCHEDULE_MAX_SIZE || !(xml = malloc(end - start ? end - start : 1)))
		goto done;
	w->status = SPOTPASS_UNREADABLE;
	if (read_at(f, start, xml, end - start))
		schedule_week(xml, end - start, w); /* OK or NO_MACHINES */
done:
	free(xml);
	free(hdr);
	FSFILE_Close(f);
	FSUSER_CloseArchive(arch);
}

/* GAME_TITLES[i]: installed versions, the code that runs, boss.sarc. */
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

	s->game[i].spotpass.status = SPOTPASS_MISSING;
	if (s->game[i].installed)
		probe_spotpass(&s->game[i].spotpass, GAME_TITLES[i].extdata_id);

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

/* The settings for game `i` (ipscfg.h): the catalog mode from the checks,
 * and the first day of the SpotPass week when boss.sarc has one; otherwise
 * the anchor stays as in `base` (the patch's default, unused in built-in
 * mode anyway). */
static IpsSettings wanted_settings(const SystemState *s, int i, const IpsSettings *base)
{
	IpsSettings w = *base;
	const SpotPassWeek *week = &s->game[i].spotpass;
	w.mode = checks_catalog(week, s->want_builtin);
	if (week->status == SPOTPASS_OK)
		w.anchor_ms = date_ms(week->first_day);
	return w;
}

/* Returns true if `a` and `b` set the same anchor and catalog mode. */
static bool same_settings(const IpsSettings *a, const IpsSettings *b)
{
	return a->anchor_ms == b->anchor_ms && a->mode == b->mode;
}

/* Replace the chosen patch with a copy of `data` written with the settings
 * for this console: that copy is what Install saves, and what the SD is
 * compared with. */
static void set_chosen_with_settings(SystemState *s, int i, const unsigned char *data, size_t size)
{
	unsigned char *copy = malloc(size ? size : 1);
	if (copy)
		memcpy(copy, data, size);
	free(chosen[i].owned);
	chosen[i].owned = copy;
	chosen[i].data = copy;
	chosen[i].size = copy ? size : 0;
	if (!copy)
	{
		s->game[i].source = SOURCE_NONE;
		return;
	}

	IpsSettings base, want;
	s->game[i].settings_ok = ipscfg_read(copy, size, &base);
	if (!s->game[i].settings_ok)
	{
		s->game[i].catalog = CATALOG_SPOTPASS; /* old patch: always the boss.sarc week */
		return;
	}
	want = wanted_settings(s, i, &base);
	ipscfg_write(copy, size, &want);
	s->game[i].catalog = (CatalogMode)want.mode;
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

	IpsSettings sd_set;
	bool sd_has_settings = sd && ipscfg_read(sd, sd_size, &sd_set);
	s->game[i].sd_catalog = sd_has_settings ? (int)sd_set.mode : -1;
	PatchState st = s->game[i].sd_patch;
	if (st != PATCH_SAME && st != PATCH_NOT_ON_SD && chosen[i].data &&
	    ipscfg_same_except_settings(sd, sd_size, chosen[i].data, chosen[i].size))
	{
		/* Our very patch, written for another week or catalog mode. */
		s->game[i].sd_patch = PATCH_SETTINGS;
	}
	else if (st == PATCH_NEWER && sd_has_settings && s->game[i].source != SOURCE_NONE)
	{
		/* Install keeps a newer patch of ours; its settings still follow
		 * this console (e.g. the user switched the catalog). */
		IpsSettings want = wanted_settings(s, i, &sd_set);
		if (!same_settings(&want, &sd_set))
		{
			set_chosen_with_settings(s, i, sd, sd_size);
			snprintf(s->game[i].patch_release, sizeof(s->game[i].patch_release), "%s",
			         s->game[i].sd_release);
			s->game[i].sd_patch = PATCH_SETTINGS;
		}
		else
			s->game[i].catalog = (CatalogMode)sd_set.mode;
	}
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
		s->game[i].settings_ok = false;
		s->game[i].catalog = CATALOG_SPOTPASS;
		if (s->game[i].installed && done)
			choose_patch(s, job, i);
		if (chosen[i].data)
			set_chosen_with_settings(s, i, chosen[i].data, chosen[i].size);
		classify_sd_patch(s, i, sd_path);
	}
}
