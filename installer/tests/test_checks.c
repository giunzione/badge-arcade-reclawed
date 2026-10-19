/*
 * Tests for source/checks.c and source/titles.c, on hand-made SystemState
 * snapshots (what probe.c would find on a console).
 */
#include "checks.h"
#include "test.h"
#include "titles.h"

enum { EUR = 0, USA = 1, JPN = 2 }; /* GAME_TITLES rows */

/* Give game `i` an installed update of `version`, and a patch from `source`. */
static void set_game(SystemState *s, int i, unsigned version, PatchSource source)
{
	s->game[i].installed = true;
	s->game[i].update_installed = true;
	s->game[i].update_version = version;
	s->game[i].base_version = 4096;
	s->game[i].code = CODE_UPDATE;
	s->game[i].code_version = version;
	s->game[i].source = source;
	s->game[i].fetch = source == SOURCE_ONLINE ? FETCH_OK : FETCH_NOT_TRIED;
	snprintf(s->game[i].patch_release, sizeof(s->game[i].patch_release), "%s",
	         source == SOURCE_ONLINE ? "v0.4.1" : source == SOURCE_BUILTIN ? "v0.4.0" : "");
	s->game[i].sd_patch = PATCH_NOT_ON_SD;
}

/* A console ready for a first install: Luma on the SD, game patching off,
 * Badge Arcade EUR v1.3.1, its patch found online, nothing on the SD yet. */
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
	s.online = ONLINE_OK;
	snprintf(s.builtin_release, sizeof(s.builtin_release), "v0.4.0");
	set_game(&s, EUR, GAME_TITLES[EUR].update_version, SOURCE_ONLINE);
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
	CHECK(!r.nothing_to_do && !r.checking);
	CHECK(r.target == EUR && r.report_target == -1);
	CHECK(count_level(&r, CHECK_STOP) == 0);
	CHECK(has(&r, CHECK_OK, "Luma3DS v13.3.2"));
	CHECK(has(&r, CHECK_OK, "Badge Arcade EUR v1.3.1"));
	CHECK(has(&r, CHECK_OK, "Patch from v0.4.1 (online)"));
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

/* The code.ips on the SD vs the chosen patch (v0.4.1 online), with the
 * record the app left next to it (installed.h). */
static void sd_patch_states(void)
{
	SystemState s = ready();
	CheckReport r;

	/* Ours from an older release: an update, offered as such. */
	s.game[EUR].sd_patch = PATCH_OLDER;
	snprintf(s.game[EUR].sd_release, sizeof(s.game[EUR].sd_release), "v0.4.0");
	checks_run(&s, &r);
	CHECK(r.can_install && !r.nothing_to_do && r.update_available);
	CHECK(!r.keep_sd_patch && !r.confirm_replace);
	CHECK(has(&r, CHECK_INFO, "Update available: v0.4.0 -> v0.4.1"));

	/* Ours from a newer release (e.g. offline with an older app): kept, so
	 * with game patching already on there is nothing to do. */
	s.game[EUR].sd_patch = PATCH_NEWER;
	snprintf(s.game[EUR].sd_release, sizeof(s.game[EUR].sd_release), "v0.5.0");
	s.config_game_patching = 1;
	checks_run(&s, &r);
	CHECK(r.can_install && r.nothing_to_do && r.keep_sd_patch && !r.update_available);
	CHECK(has(&r, CHECK_OK, "Newer patch on SD (v0.5.0), kept"));
	/* ...and if only the config is left, Install must not touch the patch. */
	s.config_game_patching = 0;
	checks_run(&s, &r);
	CHECK(r.can_install && !r.nothing_to_do && r.keep_sd_patch);

	/* Ours, but the order of the releases is unknown (e.g. "dev"). */
	s.game[EUR].sd_patch = PATCH_OTHER_RELEASE;
	snprintf(s.game[EUR].sd_release, sizeof(s.game[EUR].sd_release), "dev");
	checks_run(&s, &r);
	CHECK(r.can_install && !r.update_available && !r.confirm_replace);
	CHECK(has(&r, CHECK_INFO, "Patch dev on SD, will be replaced"));

	/* Ours, made for another game version (installed before the update). */
	s.game[EUR].sd_patch = PATCH_OTHER_VERSION;
	checks_run(&s, &r);
	CHECK(r.can_install && !r.confirm_replace);
	CHECK(has(&r, CHECK_WARN, "Patch for another game version on SD"));

	/* Not written by the app (custom build, manual copy): ask first. */
	s.game[EUR].sd_patch = PATCH_FOREIGN;
	s.game[EUR].sd_release[0] = '\0';
	checks_run(&s, &r);
	CHECK(r.can_install && !r.nothing_to_do && r.confirm_replace && !r.update_available);
	CHECK(has(&r, CHECK_WARN, "Unknown patch on SD (asks to replace)"));
}

static void online_states(void)
{
	SystemState s = ready();
	CheckReport r;

	/* Still checking: nothing can be installed yet, no report either. */
	s.online = ONLINE_CHECKING;
	s.game[EUR].source = SOURCE_NONE;
	checks_run(&s, &r);
	CHECK(r.checking && !r.can_install && !r.nothing_to_do);
	CHECK(r.report_target == -1);
	CHECK(has(&r, CHECK_INFO, "Checking online for patches..."));
	CHECK(count_level(&r, CHECK_STOP) == 0);

	/* Offline, built-in patch: install allowed, with a warning. */
	s.online = ONLINE_FAILED;
	s.online_error = NET_ERR_OFFLINE;
	set_game(&s, EUR, GAME_TITLES[EUR].update_version, SOURCE_BUILTIN);
	checks_run(&s, &r);
	CHECK(r.can_install && r.target == EUR);
	CHECK(has(&r, CHECK_WARN, "Online check failed: no internet"));
	CHECK(has(&r, CHECK_OK, "Using the built-in patch (v0.4.0)"));

	/* A wrong clock breaks HTTPS: the hint says so. */
	s.online_error = NET_ERR_TLS;
	checks_run(&s, &r);
	CHECK(has(&r, CHECK_WARN, "Online check failed: check date and time"));
}

static void online_patch_problems(void)
{
	SystemState s = ready();
	CheckReport r;

	/* Tampered or corrupted download, built-in copy available: never use the
	 * download, say so, install the built-in patch. */
	set_game(&s, EUR, GAME_TITLES[EUR].update_version, SOURCE_BUILTIN);
	s.game[EUR].fetch = FETCH_BAD_HASH;
	checks_run(&s, &r);
	CHECK(r.can_install && r.target == EUR);
	CHECK(has(&r, CHECK_WARN, "Online patch failed its security check"));
	CHECK(has(&r, CHECK_OK, "Using the built-in patch (v0.4.0)"));

	/* Same without a built-in copy: blocked, and not reported as a missing version. */
	s.game[EUR].source = SOURCE_NONE;
	checks_run(&s, &r);
	CHECK(!r.can_install && r.target == -1 && r.report_target == -1);
	CHECK(has(&r, CHECK_STOP, "Online patch failed its security check"));
	CHECK(!has(&r, CHECK_STOP, "No patch for this game version yet"));

	/* Download failed, with and without a built-in copy. */
	s.game[EUR].fetch = FETCH_FAILED;
	checks_run(&s, &r);
	CHECK(!r.can_install && r.report_target == -1);
	CHECK(has(&r, CHECK_STOP, "Online patch download failed"));
	s.game[EUR].source = SOURCE_BUILTIN;
	checks_run(&s, &r);
	CHECK(r.can_install);
	CHECK(has(&r, CHECK_WARN, "Online patch download failed"));
	CHECK(has(&r, CHECK_OK, "Using the built-in patch (v0.4.0)"));

	/* The latest release doesn't list this version, the app does. */
	s.game[EUR].fetch = FETCH_NOT_LISTED;
	checks_run(&s, &r);
	CHECK(r.can_install);
	CHECK(has(&r, CHECK_INFO, "Not in the latest release"));
	CHECK(has(&r, CHECK_OK, "Using the built-in patch (v0.4.0)"));

	/* Listed nowhere: a missing version, to report. */
	s.game[EUR].source = SOURCE_NONE;
	checks_run(&s, &r);
	CHECK(r.report_target == EUR);
	CHECK(!has(&r, CHECK_INFO, "Not in the latest release"));
	CHECK(has(&r, CHECK_STOP, "No patch for this game version yet"));
}

static void no_patch_for_version(void)
{
	SystemState s = ready();
	CheckReport r;

	/* An update version nobody has made a patch for. */
	set_game(&s, EUR, 4112, SOURCE_NONE);
	checks_run(&s, &r);
	CHECK(!r.can_install && r.target == -1);
	CHECK(r.report_target == EUR);
	CHECK(has(&r, CHECK_OK, "Badge Arcade EUR, update v4112"));
	CHECK(has(&r, CHECK_STOP, "No patch for this game version yet"));

	/* No update installed: the base game's code runs. */
	s.game[EUR].update_installed = false;
	s.game[EUR].code = CODE_BASE;
	s.game[EUR].code_version = 4096;
	checks_run(&s, &r);
	CHECK(r.report_target == EUR);
	CHECK(has(&r, CHECK_OK, "Badge Arcade EUR, no update (v4096)"));

	/* No update, but a patch exists for the base game (the "-noupdate" TOMLs):
	 * installable like the updated game, nothing to report. */
	s.game[EUR].source = SOURCE_BUILTIN;
	checks_run(&s, &r);
	CHECK(r.can_install && r.target == EUR && r.report_target == -1);
	CHECK(count_level(&r, CHECK_STOP) == 0);
	CHECK(has(&r, CHECK_OK, "Badge Arcade EUR, no update (v4096)"));
	CHECK(has(&r, CHECK_INFO, "Patch will be installed"));
	s.game[EUR].source = SOURCE_NONE;

	/* Offline and no built-in patch: still reported, with the offline hint. */
	s.online = ONLINE_FAILED;
	s.online_error = NET_ERR_OFFLINE;
	checks_run(&s, &r);
	CHECK(r.report_target == EUR);
	CHECK(has(&r, CHECK_WARN, "Online check failed: no internet"));
}

static void game_missing_or_other_region(void)
{
	SystemState s = ready();
	s.game[EUR].installed = false;
	CheckReport r;
	checks_run(&s, &r);
	CHECK(!r.can_install && r.target == -1 && r.report_target == -1);
	CHECK(has(&r, CHECK_STOP, "Nintendo Badge Arcade is not installed"));
	/* The config line is still shown. */
	CHECK(has(&r, CHECK_INFO, "Game patching will be enabled"));

	/* USA installed, no patch for it: reported. */
	set_game(&s, USA, GAME_TITLES[USA].update_version, SOURCE_NONE);
	checks_run(&s, &r);
	CHECK(!r.can_install && r.report_target == USA);
	CHECK(has(&r, CHECK_OK, "Badge Arcade USA v1.3.1"));

	/* Once a USA patch exists (online), USA is the target. */
	s.game[USA].source = SOURCE_ONLINE;
	checks_run(&s, &r);
	CHECK(r.can_install && r.target == USA);
}

static void picks_the_patched_region(void)
{
	/* JPN without a patch and EUR with one: EUR wins, nothing to report. */
	SystemState s = ready();
	set_game(&s, JPN, GAME_TITLES[JPN].update_version, SOURCE_NONE);
	CheckReport r;
	checks_run(&s, &r);
	CHECK(r.can_install && r.target == EUR && r.report_target == -1);
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
	/* The longest texts: every config error, every network error, the
	 * longest version numbers. None may be cut. */
	for (int st = LUMACFG_ERR_NO_VERSION; st <= LUMACFG_ERR_BAD_VALUE; st++)
		for (int net = NET_OK; net <= NET_ERR_OTHER; net++)
		{
			SystemState s = ready();
			s.config_status = (LumaCfgStatus)st;
			s.online = ONLINE_FAILED;
			s.online_error = (NetStatus)net;
			set_game(&s, EUR, 65535, SOURCE_BUILTIN);
			s.game[EUR].fetch = (FetchResult)(net % (FETCH_BAD_HASH + 1));
			s.game[EUR].code = st % 2 ? CODE_BASE : CODE_UPDATE;
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

	/* A leftover patch for a region that has no patch any more. */
	s = ready();
	s.game[USA].sd_patch = PATCH_FOREIGN;
	checks_run(&s, &r);
	CHECK(r.target == EUR && r.uninstall_target == USA);

	/* The detected game's patch comes first. */
	s.game[EUR].sd_patch = PATCH_OLDER;
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
	sd_patch_states();
	online_states();
	online_patch_problems();
	no_patch_for_version();
	game_missing_or_other_region();
	picks_the_patched_region();
	luma_states();
	config_problems();
	lines_fit_the_screen();
	uninstall_target();
	titles_lookup();
}
