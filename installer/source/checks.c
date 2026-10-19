/*
 * checks - see checks.h.
 *
 * Order of the lines on screen: Luma3DS, the game, its update, the patch on
 * the SD, the Luma config. A CHECK_STOP anywhere blocks the install; the
 * remaining lines are still shown so the user sees everything at once.
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

/* Pick the installed game to patch: the first installed region with a bundled
 * patch. Returns -1 and explains on screen when there is none. */
static int pick_target(const SystemState *s, CheckReport *r)
{
	int installed = -1;
	for (int i = 0; i < GAME_TITLE_COUNT; i++)
	{
		if (!s->game[i].installed)
			continue;
		if (s->game[i].patch_bundled)
			return i;
		if (installed < 0)
			installed = i;
	}
	if (installed >= 0)
		add(r, CHECK_STOP, "Badge Arcade %s: no patch for it yet",
		    GAME_TITLES[installed].region);
	else
		add(r, CHECK_STOP, "Nintendo Badge Arcade is not installed");
	return -1;
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

static void check_update(const SystemState *s, int t, CheckReport *r)
{
	const GameTitle *g = &GAME_TITLES[t];
	if (!s->game[t].update_installed)
		add(r, CHECK_STOP, "Update v1.3.1 is not installed");
	else if (s->game[t].update_version != g->update_version)
		add(r, CHECK_STOP, "Update is v%u, the patch needs v%u",
		    s->game[t].update_version, (unsigned)g->update_version);
	else
		add(r, CHECK_OK, "Update v1.3.1 installed");
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

	check_luma(s, out);

	int t = pick_target(s, out);
	bool patch_needed = false;
	if (t >= 0)
	{
		out->target = t;
		add(out, CHECK_OK, "Badge Arcade %s installed", GAME_TITLES[t].region);
		check_update(s, t, out);
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

	bool config_needed = check_config(s, out);

	if (t < 0)
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
