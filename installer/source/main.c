/*
 * Badge Arcade Reclawed installer - entry point, input and actions.
 *
 * Every frame: read the buttons and the touch screen, act on them, draw
 * (ui.c). The state of the console is probed at start and again after every
 * action (probe.c -> checks.c), so the screens always show what is on the SD.
 *
 * Actions:
 *   Install   copy the patch for the detected game, enable game patching,
 *             then restart after a 3-second countdown if the toggle is on
 *   Uninstall delete the patch from the SD (after a confirmation)
 *   Restart   restart the console, so Luma applies the new settings
 *   Exit      back to the HOME Menu / Homebrew Launcher (also START)
 */
#include <stdio.h>
#include <3ds.h>

#include "checks.h"
#include "installer.h"
#include "menu.h"
#include "probe.h"
#include "ui.h"

#ifndef APP_VERSION
#define APP_VERSION "dev" /* normally set by the Makefile from VERSION */
#endif

#define COUNTDOWN_FRAMES (3 * 60)

static CheckReport report;
static Menu menu;
static UiState ui;

/* Probe the console again and update what the menu allows. */
static void refresh(void)
{
	SystemState state;
	probe_system(&state);
	checks_run(&state, &report);
	menu_update(&menu, &report, true);
}

static void add_line(const char *label, InstResult r, const char *msg)
{
	if (ui.line_count >= UI_RESULT_LINES)
		return;
	UiLine *l = &ui.lines[ui.line_count++];
	l->level = r == INST_FAILED ? CHECK_STOP : CHECK_OK;
	snprintf(l->label, sizeof(l->label), "%s", label);
	snprintf(l->text, sizeof(l->text), "%s", msg);
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
	char bundled[96], on_sd[96], msg[96];
	ui.line_count = 0;
	probe_patch_paths(report.target, bundled, on_sd, sizeof(bundled));

	InstResult patch = inst_copy_patch(bundled, on_sd, msg, sizeof(msg));
	add_line("Patch", patch, msg);
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

static void do_uninstall(void)
{
	char bundled[96], on_sd[96], msg[96];
	ui.line_count = 0;
	probe_patch_paths(report.uninstall_target, bundled, on_sd, sizeof(bundled));
	InstResult r = inst_remove_patch(on_sd, msg, sizeof(msg));
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
	switch (item)
	{
	case ITEM_INSTALL: do_install(); break;
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

	if (ui.dialog == DIALOG_CONFIRM_UNINSTALL)
	{
		if (yes)
			do_uninstall();
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
	romfsInit(); /* the bundled patches; probe reports them missing if this fails */
	amInit();    /* installed titles; without it the game shows as not installed */
	if (!ui_init())
	{
		ui_exit();
		return 1;
	}

	menu_init(&menu);
	refresh();

	while (aptMainLoop())
	{
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

	ui_exit();
	amExit();
	romfsExit();
	return 0;
}
