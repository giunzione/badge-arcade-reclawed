/*
 * Badge Arcade Reclawed installer - entry point and text UI.
 *
 * Top screen: the result of every check (checks.c) on the current state of
 * the console (probe.c). Bottom screen: controls, then the install report.
 * Pressing A, when the checks allow it, installs for the detected game:
 *   1. copy romfs:/luma/titles/<title id>/code.ips to the same path on the SD;
 *   2. set enable_game_patching = 1 in sdmc:/luma/config.ini.
 */
#include <stdio.h>
#include <3ds.h>

#include "checks.h"
#include "installer.h"
#include "probe.h"

#ifndef APP_VERSION
#define APP_VERSION "dev" /* normally set by the Makefile from VERSION */
#endif

/* ANSI colors understood by the libctru console. */
#define C_RESET  "\x1b[0m"
#define C_GREEN  "\x1b[32;1m"
#define C_RED    "\x1b[31;1m"
#define C_YELLOW "\x1b[33;1m"
#define C_CYAN   "\x1b[36;1m"

static PrintConsole top, bottom;

static void draw_checks(const CheckReport *r)
{
	static const char *const tags[] = {
		[CHECK_OK] = C_GREEN "[ ok ]" C_RESET,
		[CHECK_INFO] = C_CYAN "[info]" C_RESET,
		[CHECK_WARN] = C_YELLOW "[warn]" C_RESET,
		[CHECK_STOP] = C_RED "[STOP]" C_RESET,
	};

	consoleSelect(&top);
	consoleClear();
	printf("Badge Arcade Reclawed installer v%s\n\n", APP_VERSION);
	for (int i = 0; i < r->count; i++)
		printf("%s %s\n", tags[r->items[i].level], r->items[i].text);

	printf("\n");
	if (r->nothing_to_do)
		printf(C_GREEN "Everything is installed." C_RESET "\n");
	else if (r->can_install)
		printf("Ready to install.\n");
	else
		printf(C_RED "Can't install: see the [STOP] lines." C_RESET "\n");
}

static void draw_controls(const CheckReport *r)
{
	if (r->can_install && !r->nothing_to_do)
		printf("\n(A) install    (START) exit\n");
	else
		printf("\n(START) exit\n");
}

static void print_result(const char *what, InstResult res, const char *msg)
{
	const char *tag = res == INST_DONE    ? C_GREEN "[done]" C_RESET
	                : res == INST_ALREADY ? C_GREEN "[ ok ]" C_RESET
	                                      : C_RED "[fail]" C_RESET;
	printf("%s %s:\n       %s\n", tag, what, msg);
}

/* Bottom screen: run both install steps for the target game and report. */
static void run_install(const CheckReport *r)
{
	consoleSelect(&bottom);
	consoleClear();

	char bundled[96], on_sd[96], msg[96];
	probe_patch_paths(r->target, bundled, on_sd, sizeof(bundled));

	InstResult patch = inst_copy_patch(bundled, on_sd, msg, sizeof(msg));
	print_result("Patch", patch, msg);
	if (patch == INST_FAILED)
	{
		printf("\n" C_RED "Install stopped." C_RESET "\n");
		return;
	}

	InstResult cfg = inst_enable_game_patching(LUMA_CONFIG, CONFIG_BACKUP, msg, sizeof(msg));
	print_result("Luma3DS config", cfg, msg);
	if (cfg == INST_FAILED)
	{
		printf("\n" C_RED "Install incomplete." C_RESET "\n");
		return;
	}

	printf("\n" C_GREEN "Done." C_RESET " Restart the console to\n"
	       "apply. Don't save Rosalina settings\n"
	       "before restarting: that would turn\n"
	       "game patching off again.\n");
}

/* Probe the console, run the checks and redraw the top screen. */
static void refresh(CheckReport *r)
{
	SystemState state;
	probe_system(&state);
	checks_run(&state, r);
	draw_checks(r);
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, &top);
	consoleInit(GFX_BOTTOM, &bottom);

	/* RomFS holds the patches; am lists the installed titles. */
	bool romfs_ok = R_SUCCEEDED(romfsInit());
	bool am_ok = R_SUCCEEDED(amInit());

	CheckReport report;
	refresh(&report);

	consoleSelect(&bottom);
	if (!romfs_ok)
		printf(C_RED "Can't open the app's RomFS." C_RESET "\n");
	if (!am_ok)
		printf(C_RED "Can't list installed titles (am)." C_RESET "\n");
	draw_controls(&report);

	while (aptMainLoop())
	{
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_START)
			break;
		if ((down & KEY_A) && report.can_install && !report.nothing_to_do)
		{
			run_install(&report);
			refresh(&report); /* show the new state */
			consoleSelect(&bottom);
			draw_controls(&report);
		}

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	if (am_ok)
		amExit();
	if (romfs_ok)
		romfsExit();
	gfxExit();
	return 0;
}
