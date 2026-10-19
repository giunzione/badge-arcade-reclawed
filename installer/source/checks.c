/*
 * checks - see checks.h.
 *
 * Order of the lines on screen: Luma3DS, the game and its version, where the
 * patch comes from, the patch on the SD, the Luma config. A CHECK_STOP
 * anywhere blocks the install; the remaining lines are still shown so the
 * user sees everything at once.
 */
#include "checks.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void add(CheckReport *r, CheckLevel level, const char *fmt, ...)
{
	if (r->count >= CHECK_MAX_ITEMS)
		return;
	CheckItem *it = &r->items[r->count++];
	it->level = level;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(it->text, sizeof(it->text), fmt, ap);
	va_end(ap);
	if (level == CHECK_STOP)
		r->can_install = false;
}

/* The installed game to work on: the first one a patch exists for, else the
 * first installed one (it will get the "no patch" line). -1 if none. */
static int pick_game(const SystemState *s)
{
	int first = -1;
	for (int i = 0; i < GAME_TITLE_COUNT; i++)
	{
		if (!s->game[i].installed)
			continue;
		if (s->game[i].source != SOURCE_NONE)
			return i;
		if (first < 0)
			first = i;
	}
	return first;
}

static void check_luma(const SystemState *s, CheckReport *r)
{
	if (!s->luma_detected)
		add(r, CHECK_WARN, "Luma3DS not detected");
	else if (!s->luma_on_sd)
		add(r, CHECK_STOP, "Luma3DS runs from internal memory");
	else
		add(r, CHECK_OK, "Luma3DS v%u.%u.%u", s->luma_version[0], s->luma_version[1],
		    s->luma_version[2]);
}

/* "Badge Arcade EUR v1.3.1", or the raw title version when it isn't the known final update. */
static void check_game(const SystemState *s, int t, CheckReport *r)
{
	const GameTitle *g = &GAME_TITLES[t];
	if (s->game[t].code == CODE_UPDATE && s->game[t].code_version == g->update_version)
		add(r, CHECK_OK, "Badge Arcade %s v1.3.1", g->region);
	else if (s->game[t].code == CODE_UPDATE)
		add(r, CHECK_OK, "Badge Arcade %s, update v%u", g->region, s->game[t].code_version);
	else
		add(r, CHECK_OK, "Badge Arcade %s, no update (v%u)", g->region, s->game[t].code_version);
}

/* Where the patch comes from. Returns true if there is one to install. */
static bool check_source(const SystemState *s, int t, CheckReport *r)
{
	if (s->online == ONLINE_CHECKING)
	{
		add(r, CHECK_INFO, "Checking online for patches...");
		r->checking = true;
		r->can_install = false;
		return false;
	}
	if (s->online == ONLINE_FAILED)
		add(r, CHECK_WARN, "Online check failed: %s", net_status_str(s->online_error));

	/* Why the latest release's patch isn't used, when it is listed or should be.
	 * Without a built-in copy to fall back on, these block the install. */
	bool fallback = s->game[t].source == SOURCE_BUILTIN;
	switch (s->game[t].fetch)
	{
	case FETCH_BAD_HASH:
		/* Corrupted or tampered with: the download was discarded. */
		add(r, fallback ? CHECK_WARN : CHECK_STOP, "Online patch failed its security check");
		break;
	case FETCH_FAILED:
		add(r, fallback ? CHECK_WARN : CHECK_STOP, "Online patch download failed");
		break;
	case FETCH_NOT_LISTED:
		if (fallback)
			add(r, CHECK_INFO, "Not in the latest release");
		break;
	case FETCH_NOT_TRIED:
	case FETCH_OK:
		break;
	}

	switch (s->game[t].source)
	{
	case SOURCE_ONLINE:
		add(r, CHECK_OK, "Patch from %s (online)", s->game[t].patch_release);
		return true;
	case SOURCE_BUILTIN:
		add(r, CHECK_OK, "Using the built-in patch (%s)", s->game[t].patch_release);
		return true;
	case SOURCE_NONE:
		break;
	}
	/* A patch exists but couldn't be fetched: already explained above, and
	 * nothing to report. */
	if (s->game[t].fetch == FETCH_BAD_HASH || s->game[t].fetch == FETCH_FAILED)
		return false;
	add(r, CHECK_STOP, "No patch for this game version yet");
	r->report_target = t;
	return false;
}

/* Returns true if the config still needs game patching turned on. */
static bool check_config(const SystemState *s, CheckReport *r)
{
	if (!s->config_found)
	{
		add(r, CHECK_STOP, "Luma3DS config.ini not found");
		return false;
	}
	if (s->config_status != LUMACFG_OK)
	{
		add(r, CHECK_STOP, "config.ini: %s", lumacfg_status_str(s->config_status));
		return false;
	}
	if (!s->config_game_patching)
	{
		add(r, CHECK_INFO, "Game patching will be enabled");
		return true;
	}
	if (s->luma_detected && !s->patching_active)
		add(r, CHECK_INFO, "Game patching on after a restart");
	else
		add(r, CHECK_OK, "Game patching enabled");
	return false;
}

void checks_run(const SystemState *s, CheckReport *out)
{
	memset(out, 0, sizeof(*out));
	out->can_install = true;
	out->target = -1;
	out->report_target = -1;

	check_luma(s, out);

	int t = pick_game(s);
	bool patch_needed = false, have_patch = false;
	if (t < 0)
		add(out, CHECK_STOP, "Nintendo Badge Arcade is not installed");
	else
	{
		check_game(s, t, out);
		have_patch = check_source(s, t, out);
		if (have_patch)
		{
			out->target = t;
			switch (s->game[t].sd_patch)
			{
			case PATCH_SAME:
				add(out, CHECK_OK, "Patch installed and up to date");
				break;
			case PATCH_DIFFERENT:
				add(out, CHECK_WARN, "Other patch on SD, will be replaced");
				patch_needed = true;
				break;
			case PATCH_NOT_ON_SD:
				add(out, CHECK_INFO, "Patch will be installed");
				patch_needed = true;
				break;
			}
		}
	}

	bool config_needed = check_config(s, out);

	if (!have_patch)
		out->can_install = false;
	out->nothing_to_do = out->can_install && !patch_needed && !config_needed;

	/* Uninstall: the target's patch if it has one on the SD, else any Badge
	 * Arcade patch left there (e.g. the game was deleted). Not blocked by the
	 * checks above: removing the file is always safe. */
	out->uninstall_target = -1;
	if (t >= 0 && s->game[t].sd_patch != PATCH_NOT_ON_SD)
		out->uninstall_target = t;
	for (int i = 0; i < GAME_TITLE_COUNT && out->uninstall_target < 0; i++)
		if (s->game[i].sd_patch != PATCH_NOT_ON_SD)
			out->uninstall_target = i;
}
