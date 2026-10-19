/*
 * Badge Arcade Reclawed installer - entry point, input and actions.
 *
 * At start the console is probed (probe.c) and a background thread looks for
 * the latest patches on GitHub (online.c). Until it is done the checks say
 * "Checking online"; then the patch for the installed game is chosen: the
 * latest release's, or the app's built-in copy when offline.
 *
 * Every frame: read the buttons and the touch screen, act on them, draw
 * (ui.c). The state is probed again after every action, so the screens always
 * show what is on the SD.
 *
 * Actions:
 *   Install   write the chosen patch for the detected game, enable game
 *             patching, then restart after a 3-second countdown if the toggle is on
 *   Report    (instead of Install, when the game's version has no patch)
 *             QR code to a pre-filled GitHub issue
 *   Uninstall delete the patch from the SD (after a confirmation)
 *   Restart   restart the console, so Luma applies the new settings
 *   Exit      back to the HOME Menu / Homebrew Launcher (also START)
 */
#include <stdio.h>
#include <3ds.h>

#include "checks.h"
#include "installer.h"
#include "menu.h"
#include "net.h"
#include "online.h"
#include "probe.h"
#include "report.h"
#include "ui.h"

#ifndef APP_VERSION
#define APP_VERSION "dev" /* normally set by the Makefile from VERSION */
#endif

#define COUNTDOWN_FRAMES (3 * 60)

static SystemState state;
static CheckReport report;
static Menu menu;
static UiState ui;
static OnlineJob job;
static bool job_started;

#ifdef RECLAWED_TEST_LOG
/* Test builds (FAKE_GAME / ONLINE_BASE) append every check result to a file on
 * the SD, so the simulated cases can be verified without reading the screen. */
static void test_log(void)
{
	FILE *f = fopen("sdmc:/3ds/badge-arcade-reclawed/test-log.txt", "a");
	if (!f)
		return;
	static const char *const levels[] = {"ok", "info", "warn", "STOP"};
	fprintf(f, "--- v%s checking=%d can_install=%d nothing_to_do=%d target=%d report=%d uninstall=%d\n",
	        APP_VERSION, report.checking, report.can_install, report.nothing_to_do, report.target,
	        report.report_target, report.uninstall_target);
	for (int i = 0; i < report.count; i++)
		fprintf(f, "[%s] %s\n", levels[report.items[i].level], report.items[i].text);
	fclose(f);
}
#endif

/* Probe the console again, pick the patches and update what the menu allows. */
static void refresh(void)
{
	probe_system(&state);
	probe_resolve(&state, job_started ? &job : NULL);
	if (!job_started) /* no network at all: decide with the built-in patches now */
	{
		job.finished = true;
		job.status = NET_ERR_INIT;
		probe_resolve(&state, &job);
	}
	checks_run(&state, &report);
	menu_update(&menu, &report, true);
#ifdef RECLAWED_TEST_LOG
	test_log();
#endif
}

static void add_line(const char *label, InstResult r, const char *msg)
{
	if (ui.line_count >= UI_RESULT_LINES)
		return;
	UiLine *l = &ui.lines[ui.line_count++];
	l->level = r == INST_FAILED ? CHECK_STOP : CHECK_OK;
	snprintf(l->label, sizeof(l->label), "%s", label);
	snprintf(l->text, sizeof(l->text), "%.63s", msg); /* one dialog line; longer text is cut */
}

static void open_result(const char *title, bool failed)
{
	ui.dialog = DIALOG_RESULT;
	snprintf(ui.title, sizeof(ui.title), "%s", title);
	ui.failed = failed;
	ui.countdown_frames = 0;
	ui.restart_failed = false;
}

/* Ask the system to restart. Luma3DS reads config.ini and code.ips again at
 * boot, so this is what makes the install take effect. */
static bool restart_console(void)
{
	if (R_SUCCEEDED(APT_HardwareResetAsync()))
		return true;
	/* Fallback: the power manager (ptm:sysm, allowed in meta/app.rsf). */
	if (R_SUCCEEDED(ptmSysmInit()))
	{
		Result r = PTMSYSM_RebootAsync(0);
		ptmSysmExit();
		return R_SUCCEEDED(r);
	}
	return false;
}

static void do_install(void)
{
	int t = report.target;
	char on_sd[96], record[96], msg[96];
	size_t size;
	const unsigned char *ips = probe_patch_data(t, &size);
	ui.line_count = 0;
	probe_sd_patch_path(t, on_sd, sizeof(on_sd));
	probe_sd_record_path(t, record, sizeof(record));

	InstResult patch;
	if (report.keep_sd_patch)
	{
		/* The SD has our patch from a newer release: never downgrade it. */
		patch = INST_ALREADY;
		add_line("Patch", patch, "newer patch on the SD kept");
	}
	else
	{
		patch = inst_write_patch(ips, size, on_sd, msg, sizeof(msg));
		add_line("Patch", patch, msg);
		/* Remember which release this is, so the next start can offer updates.
		 * A failure here doesn't undo the install: the patch itself works. */
		if (patch != INST_FAILED &&
		    inst_write_record(record, state.game[t].patch_release, state.game[t].code,
		                      state.game[t].code_version, ips, size, msg, sizeof(msg)) == INST_FAILED)
			add_line("Patch record", INST_FAILED, msg);
	}
	InstResult cfg = INST_FAILED;
	if (patch != INST_FAILED)
	{
		cfg = inst_enable_game_patching(LUMA_CONFIG, CONFIG_BACKUP, msg, sizeof(msg));
		add_line("Luma3DS config", cfg, msg);
	}

	bool ok = patch != INST_FAILED && cfg != INST_FAILED;
	open_result(ok ? "Installed" : "Install failed", !ok);
	if (ok && menu.restart_after_install)
		ui.countdown_frames = COUNTDOWN_FRAMES;
	refresh();
}

/* Show the QR code for a GitHub issue about this game version. */
static void open_report(void)
{
	int t = report.report_target;
	char luma[24], url[REPORT_URL_MAX];
	snprintf(luma, sizeof(luma), "%u.%u.%u", state.luma_version[0], state.luma_version[1],
	         state.luma_version[2]);
	if (report_issue_url(url, sizeof(url), GAME_TITLES[t].region, GAME_TITLES[t].base_tid,
	                     state.game[t].code, state.game[t].code_version, APP_VERSION,
	                     state.luma_detected ? luma : NULL) != 0)
		snprintf(url, sizeof(url), REPORT_REPO_URL "/issues/new");
	ui_set_report_url(url);
	ui.dialog = DIALOG_REPORT;
}

static void do_uninstall(void)
{
	char on_sd[96], record[96], msg[96];
	ui.line_count = 0;
	probe_sd_patch_path(report.uninstall_target, on_sd, sizeof(on_sd));
	probe_sd_record_path(report.uninstall_target, record, sizeof(record));
	InstResult r = inst_remove_patch(on_sd, record, msg, sizeof(msg));
	add_line("Patch", r, msg);
	open_result(r == INST_FAILED ? "Uninstall failed" : "Uninstalled", r == INST_FAILED);
	refresh();
}

static void do_restart(void)
{
	if (restart_console())
		return; /* the system closes the app; aptMainLoop() will return false */
	if (ui.dialog == DIALOG_NONE)
	{
		ui.line_count = 0;
		open_result("Restart", true);
	}
	ui.countdown_frames = 0;
	ui.restart_failed = true;
}

/* Run the menu item; returns false to quit the app. */
static bool activate(MenuItem item)
{
	if (item == ITEM_NONE || !menu.enabled[item])
		return true;
	menu.selected = item;
	menu.left_install = false; /* picked by touch: keep it */
	switch (item)
	{
	case ITEM_INSTALL:
		if (report.can_install && report.confirm_replace)
			ui.dialog = DIALOG_CONFIRM_REPLACE; /* a code.ips the app didn't write */
		else if (report.can_install)
			do_install();
		else if (report.report_target >= 0)
			open_report();
		break;
	case ITEM_UNINSTALL: ui.dialog = DIALOG_CONFIRM_UNINSTALL; break;
	case ITEM_RESTART: do_restart(); break;
	case ITEM_EXIT: return false;
	case ITEM_RESTART_AFTER: menu.restart_after_install = !menu.restart_after_install; break;
	default: break;
	}
	return true;
}

/* Input while a dialog is open. `button` comes from a touch: 1 = A, 2 = B. */
static void dialog_input(u32 down, int button)
{
	bool yes = (down & KEY_A) || button == 1;
	bool no = (down & KEY_B) || button == 2;

	if (ui.dialog == DIALOG_REPORT)
	{
		if (yes || no)
			ui.dialog = DIALOG_NONE;
		return;
	}
	if (ui.dialog == DIALOG_CONFIRM_UNINSTALL || ui.dialog == DIALOG_CONFIRM_REPLACE)
	{
		if (yes && ui.dialog == DIALOG_CONFIRM_UNINSTALL)
			do_uninstall();
		else if (yes)
			do_install();
		else if (no)
			ui.dialog = DIALOG_NONE;
		return;
	}

	/* Result dialog, maybe with the restart countdown running. */
	if (ui.countdown_frames > 0)
	{
		if (yes)
			do_restart();
		else if (no)
			ui.countdown_frames = 0;
		else if (--ui.countdown_frames == 0)
			do_restart();
		return;
	}
	if (yes || no)
		ui.dialog = DIALOG_NONE;
}

int main(void)
{
	romfsInit(); /* the built-in patches and certificates */
	amInit();    /* installed titles; without it the game shows as not installed */
	if (!ui_init())
	{
		ui_exit();
		return 1;
	}

	/* Local state first, then the online check in the background. */
	menu_init(&menu);
	probe_system(&state);
	probe_online_wants(&state, &job);
	job_started = net_init() && online_start(&job);
	refresh();

	while (aptMainLoop())
	{
		if (report.checking && online_done(&job))
			refresh(); /* the online check just ended */

		hidScanInput();
		u32 down = hidKeysDown();

		/* A tap: the item or dialog button under the stylus. */
		MenuItem tapped = ITEM_NONE;
		int dlg_button = 0;
		if (down & KEY_TOUCH)
		{
			touchPosition t;
			hidTouchRead(&t);
			if (ui.dialog != DIALOG_NONE)
				dlg_button = ui_dialog_button_at(&ui, t.px, t.py);
			else
				tapped = menu_item_at(t.px, t.py);
		}

		if (ui.dialog != DIALOG_NONE)
			dialog_input(down, dlg_button);
		else
		{
			if (down & KEY_START)
				break;
			if (down & KEY_DLEFT) menu_move(&menu, -1, 0);
			if (down & KEY_DRIGHT) menu_move(&menu, 1, 0);
			if (down & KEY_DUP) menu_move(&menu, 0, -1);
			if (down & KEY_DDOWN) menu_move(&menu, 0, 1);
			if ((down & KEY_A) && !activate(menu.selected))
				break;
			if (tapped != ITEM_NONE && !activate(tapped))
				break;
		}

		ui_draw(&report, &menu, &ui, APP_VERSION);
	}

	/* Stop a download that may still be running before tearing things down. */
	net_cancel();
	if (job_started)
		while (!online_done(&job))
			svcSleepThread(10 * 1000 * 1000);
	online_free(&job);
	probe_free();
	net_exit();
	ui_exit();
	amExit();
	romfsExit();
	return 0;
}
