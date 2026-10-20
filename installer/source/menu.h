/*
 * menu - the bottom-screen menu: which items are usable, D-pad movement and
 * the touch layout.
 *
 * Pure logic (no drawing, no 3DS calls), unit-tested in tests/test_menu.c.
 * ui.c draws the items at menu_item_rect(); main.c acts on the selection.
 *
 * Layout on the 320x240 bottom screen:
 *
 *     [ Install   ] [ Uninstall ]     row 0
 *     [ Restart   ] [ Exit      ]     row 1
 *     [ x Restart after install ]     row 2 (one item, both columns)
 *     [ Machines: SpotPass week ]     row 3 (one item: SpotPass week / built-in)
 */
#ifndef MENU_H
#define MENU_H

#include <stdbool.h>

#include "checks.h"

typedef enum
{
	ITEM_INSTALL,
	ITEM_UNINSTALL,
	ITEM_RESTART,
	ITEM_EXIT,
	ITEM_RESTART_AFTER, /* toggle */
	ITEM_CATALOG,       /* toggle: SpotPass week / built-in machines */
	ITEM_COUNT,
	ITEM_NONE = -1,
} MenuItem;

typedef struct
{
	bool enabled[ITEM_COUNT];
	MenuItem selected;
	bool restart_after_install; /* state of the toggle */
	bool builtin_catalog;       /* the user picked the built-in machines over the
	                             * SpotPass week (only offered when boss.sarc is usable) */
	bool left_install;          /* selection moved off Install only because it was
	                             * disabled (e.g. during the online check) */
} Menu;

typedef struct
{
	int x, y, w, h;
} MenuRect;

/* Initialises `m`: every item enabled (menu_update() turns some off), Install
 * selected, "Restart after install" on, the SpotPass week as the catalog. */
void menu_init(Menu *m);

/* Enables the items the checks `r` allow: Install when there is something to
 * install (or, labelled "Report", when the game's version has no patch),
 * Uninstall when a patch is on the SD, Restart when `can_restart`, Machines
 * when there is a choice (boss.sarc is usable).
 *
 * Moves the selection off a disabled item, and back to Install when it was
 * only moved off it because Install was disabled for a while (the user
 * hasn't picked something else since). */
void menu_update(Menu *m, const CheckReport *r, bool can_restart);

/* Moves the selection one cell left/right (`dx`) or up/down (`dy`), as the
 * D-pad does, skipping disabled items. Stays put at the edges. */
void menu_move(Menu *m, int dx, int dy);

/* Returns the bottom-screen rectangle of `item` (all zero for ITEM_NONE). */
MenuRect menu_item_rect(MenuItem item);

/* Returns the item under the touch point (`x`, `y`), or ITEM_NONE. */
MenuItem menu_item_at(int x, int y);

#endif
