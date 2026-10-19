/*
 * Tests for source/checks.c and source/titles.c, on hand-made SystemState
 * snapshots (what probe.c would find on a console).
 */
#include "checks.h"
#include "test.h"
#include "titles.h"

enum { EUR = 0, USA = 1, JPN = 2 }; /* GAME_TITLES rows */

/* A console ready for a first install: Luma on the SD, game patching off,
 * Badge Arcade EUR v1.3.1, patch bundled but not on the SD yet. */
static SystemState ready(void)
{
	SystemState s;
	memset(&s, 0, sizeof(s));
	s.luma_detected = true;
	s.luma_on_sd = true;
	s.luma_version[0] = 13;
	s.luma_version[1] = 3;
	s.luma_version[2] = 2;
	s.config_found = true;
	s.config_status = LUMACFG_OK;
	s.config_game_patching = 0;
	s.game[EUR].installed = true;
	s.game[EUR].update_installed = true;
	s.game[EUR].update_version = GAME_TITLES[EUR].update_version;
	s.game[EUR].patch_bundled = true;
	s.game[EUR].sd_patch = PATCH_NOT_ON_SD;
	return s;
}

/* True if the report has a line with this level and exactly this text. */
static bool has(const CheckReport *r, CheckLevel level, const char *text)
{
	for (int i = 0; i < r->count; i++)
		if (r->items[i].level == level && strcmp(r->items[i].text, text) == 0)
			return true;
	return false;
}

static int count_level(const CheckReport *r, CheckLevel level)
{
	int n = 0;
	for (int i = 0; i < r->count; i++)
		n += r->items[i].level == level;
	return n;
}

static void first_install(void)
{
	SystemState s = ready();
	CheckReport r;
	checks_run(&s, &r);
	CHECK(r.can_install);
	CHECK(!r.nothing_to_do);
	CHECK(r.target == EUR);
	CHECK(count_level(&r, CHECK_STOP) == 0);
	CHECK(has(&r, CHECK_OK, "Luma3DS v13.3.2"));
	CHECK(has(&r, CHECK_OK, "Badge Arcade EUR installed"));
	CHECK(has(&r, CHECK_OK, "Update v1.3.1 installed"));
	CHECK(has(&r, CHECK_INFO, "Patch will be installed"));
	CHECK(has(&r, CHECK_INFO, "Game patching will be enabled"));
}

static void already_installed(void)
{
	SystemState s = ready();
	s.game[EUR].sd_patch = PATCH_SAME;
	s.config_game_patching = 1;
	s.patching_active = true;
	CheckReport r;
	checks_run(&s, &r);
	CHECK(r.can_install && r.nothing_to_do);
	CHECK(has(&r, CHECK_OK, "Patch installed and up to date"));
	CHECK(has(&r, CHECK_OK, "Game patching enabled"));

	/* Enabled in the file but Luma booted without it: restart pending. */
	s.patching_active = false;
	checks_run(&s, &r);
	CHECK(r.nothing_to_do);
	CHECK(has(&r, CHECK_INFO, "Game patching on after a restart"));

	/* Only the config left to do. */
	s.config_game_patching = 0;
	checks_run(&s, &r);
	CHECK(r.can_install && !r.nothing_to_do);
}

static void replaces_other_patch(void)
{
	SystemState s = ready();
	s.game[EUR].sd_patch = PATCH_DIFFERENT;
	CheckReport r;
	checks_run(&s, &r);
	CHECK(r.can_install && !r.nothing_to_do);
	CHECK(has(&r, CHECK_WARN, "Other patch on SD, will be replaced"));
}

static void game_missing_or_unsupported(void)
{
	SystemState s = ready();
	s.game[EUR].installed = false;
	CheckReport r;
	checks_run(&s, &r);
	CHECK(!r.can_install && r.target == -1);
	CHECK(has(&r, CHECK_STOP, "Nintendo Badge Arcade is not installed"));
	/* The config line is still shown. */
	CHECK(has(&r, CHECK_INFO, "Game patching will be enabled"));

	/* USA installed, but only the EUR patch is bundled. */
	s.game[USA].installed = true;
	checks_run(&s, &r);
	CHECK(!r.can_install);
	CHECK(has(&r, CHECK_STOP, "Badge Arcade USA: no patch for it yet"));

	/* Once a USA patch is bundled, USA becomes the target. */
	s.game[USA].patch_bundled = true;
	s.game[USA].update_installed = true;
	s.game[USA].update_version = GAME_TITLES[USA].update_version;
	checks_run(&s, &r);
	CHECK(r.can_install && r.target == USA);
}

static void picks_the_supported_region(void)
{
	/* JPN without a patch and EUR with one: EUR wins, no STOP for JPN. */
	SystemState s = ready();
	s.game[JPN].installed = true;
	CheckReport r;
	checks_run(&s, &r);
	CHECK(r.can_install && r.target == EUR);
}

static void wrong_update(void)
{
	SystemState s = ready();
	s.game[EUR].update_installed = false;
	CheckReport r;
	checks_run(&s, &r);
	CHECK(!r.can_install);
	CHECK(has(&r, CHECK_STOP, "Update v1.3.1 is not installed"));

	s.game[EUR].update_installed = true;
	s.game[EUR].update_version = 4112;
	checks_run(&s, &r);
	CHECK(!r.can_install);
	CHECK(has(&r, CHECK_STOP, "Update is v4112, the patch needs v5136"));
}

static void luma_states(void)
{
	SystemState s = ready();
	CheckReport r;

	/* Emulator / unknown CFW: warn but allow (the files are still valid). */
	s.luma_detected = false;
	checks_run(&s, &r);
	CHECK(r.can_install);
	CHECK(has(&r, CHECK_WARN, "Luma3DS not detected"));

	/* Luma on CTRNAND reads its files from the NAND: refuse. */
	s.luma_detected = true;
	s.luma_on_sd = false;
	checks_run(&s, &r);
	CHECK(!r.can_install);
	CHECK(has(&r, CHECK_STOP, "Luma3DS runs from internal memory"));
}

static void config_problems(void)
{
	SystemState s = ready();
	CheckReport r;

	s.config_found = false;
	checks_run(&s, &r);
	CHECK(!r.can_install);
	CHECK(has(&r, CHECK_STOP, "Luma3DS config.ini not found"));

	s.config_found = true;
	s.config_status = LUMACFG_ERR_DUPLICATE_KEY;
	checks_run(&s, &r);
	CHECK(!r.can_install);
	CHECK(has(&r, CHECK_STOP, "config.ini: enable_game_patching repeated"));
}

static void lines_fit_the_screen(void)
{
	/* Every possible config error must fit without being cut. */
	for (int st = LUMACFG_ERR_NO_VERSION; st <= LUMACFG_ERR_BAD_VALUE; st++)
	{
		SystemState s = ready();
		s.config_status = (LumaCfgStatus)st;
		CheckReport r;
		checks_run(&s, &r);
		for (int i = 0; i < r.count; i++)
			CHECK(strlen(r.items[i].text) < CHECK_TEXT_LEN - 1);
	}
}

static void uninstall_target(void)
{
	SystemState s = ready();
	CheckReport r;
	checks_run(&s, &r);
	CHECK(r.uninstall_target == -1); /* nothing on the SD */

	s.game[EUR].sd_patch = PATCH_SAME;
	checks_run(&s, &r);
	CHECK(r.uninstall_target == EUR);

	/* Game deleted but its patch left behind: removable, though not installable. */
	s.game[EUR].installed = false;
	checks_run(&s, &r);
	CHECK(!r.can_install && r.uninstall_target == EUR);

	/* A leftover patch for a region without a bundled one. */
	s = ready();
	s.game[USA].sd_patch = PATCH_DIFFERENT;
	checks_run(&s, &r);
	CHECK(r.target == EUR && r.uninstall_target == USA);

	/* The detected game's patch comes first. */
	s.game[EUR].sd_patch = PATCH_DIFFERENT;
	checks_run(&s, &r);
	CHECK(r.uninstall_target == EUR);
}

static void titles_lookup(void)
{
	CHECK(titles_find("0004000000153600") == EUR);
	CHECK(titles_find("0004000000153500") == USA);
	CHECK(titles_find("0004000000134600") == JPN);
	CHECK(titles_find("0004000E00153600") == -1); /* the update, not the game */
	CHECK(titles_find("000400000015360") == -1);
	CHECK(titles_find("+004000000153600") == -1);
	CHECK(titles_find("0x04000000153600") == -1);
	CHECK(titles_find("") == -1);
}

void test_checks_run(void)
{
	first_install();
	already_installed();
	replaces_other_patch();
	game_missing_or_unsupported();
	picks_the_supported_region();
	wrong_update();
	luma_states();
	config_problems();
	lines_fit_the_screen();
	uninstall_target();
	titles_lookup();
}
