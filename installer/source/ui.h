/*
 * ui - draws the installer with citro2d (3DS only).
 *
 * Top screen: logo, version and the list of checks.
 * Bottom screen: status line, the menu (menu.h) and, on top of it, the
 * current dialog: uninstall / replace confirmation or the result of an
 * action, with an optional restart countdown.
 *
 * Only drawing lives here; what happens on input is decided in main.c.
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>

#include "checks.h"
#include "menu.h"

typedef enum
{
	DIALOG_NONE,
	DIALOG_CONFIRM_UNINSTALL,
	DIALOG_CONFIRM_REPLACE, /* Install over a code.ips the app didn't write */
	DIALOG_RESULT,
	DIALOG_REPORT,  /* QR code to a pre-filled GitHub issue (see report.h) */
} Dialog;

#define UI_RESULT_LINES 4
#define UI_LINE_LEN 64

/* One step of an action in the result dialog: a small label ("Patch") and
 * the outcome below it. */
typedef struct
{
	CheckLevel level;     /* colors the dot: OK = done, STOP = failed */
	char label[24];
	char text[UI_LINE_LEN];
} UiLine;

typedef struct
{
	Dialog dialog;
	char title[32];
	bool failed;                    /* result title in red instead of green */
	UiLine lines[UI_RESULT_LINES];
	int line_count;
	int countdown_frames;           /* > 0: restart countdown running (60 per second) */
	bool restart_failed;            /* show "restart manually" in the result */
} UiState;

/* Sets up citro2d, the render targets, the system font and the logo.
 *
 * Returns false if the graphics can't be started. */
bool ui_init(void);

/* Frees what ui_init() set up; also safe after a failed ui_init(). */
void ui_exit(void);

/* Draws one frame of both screens: the checks `r`, the menu `m`, the dialog
 * in `u`, and the app `version` in the corner. */
void ui_draw(const CheckReport *r, const Menu *m, const UiState *u, const char *version);

/* Returns the dialog button under the touch point (`x`, `y`): 1 = confirm (A),
 * 2 = cancel (B), 0 = none. */
int ui_dialog_button_at(const UiState *u, int x, int y);

/* Encodes `url` as the QR code the report dialog shows.
 *
 * Returns false if it doesn't fit in a QR code small enough for the screen. */
bool ui_set_report_url(const char *url);

#endif
