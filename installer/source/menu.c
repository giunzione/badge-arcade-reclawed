/*
 * menu - see menu.h.
 */
#include "menu.h"

#include <string.h>

#define COLS 2
#define ROWS 3

/* Which item fills each cell; the toggle spans the last row. */
static const MenuItem GRID[ROWS][COLS] = {
	{ITEM_INSTALL, ITEM_UNINSTALL},
	{ITEM_RESTART, ITEM_EXIT},
	{ITEM_RESTART_AFTER, ITEM_RESTART_AFTER},
};

/* Bottom screen geometry (pixels). */
#define MARGIN  12
#define GAP     12
#define BTN_W   ((320 - 2 * MARGIN - GAP) / 2)
#define BTN_H   52
#define ROW0_Y  48
#define ROW1_Y  (ROW0_Y + BTN_H + 10)
#define TOGGLE_Y (ROW1_Y + BTN_H + 10)
#define TOGGLE_H 30

/* When the selected item gets disabled, go to the first usable of these. */
static const MenuItem FALLBACK[] = {ITEM_INSTALL, ITEM_RESTART, ITEM_EXIT, ITEM_UNINSTALL};

void menu_init(Menu *m)
{
	memset(m, 0, sizeof(*m));
	for (int i = 0; i < ITEM_COUNT; i++)
		m->enabled[i] = true;
	m->selected = ITEM_INSTALL;
	m->restart_after_install = true;
}

void menu_update(Menu *m, const CheckReport *r, bool can_restart)
{
	bool install = r->can_install && !r->nothing_to_do;
	/* The first button is "Report" when the game has no patch for its version. */
	m->enabled[ITEM_INSTALL] = install || r->report_target >= 0;
	m->enabled[ITEM_RESTART_AFTER] = install;
	m->enabled[ITEM_UNINSTALL] = r->uninstall_target >= 0;
	m->enabled[ITEM_RESTART] = can_restart;
	m->enabled[ITEM_EXIT] = true;

	/* Install was only skipped while disabled: go back to it, so (A) does the
	 * expected thing once the online check is over. */
	if (m->left_install && m->enabled[ITEM_INSTALL])
	{
		m->selected = ITEM_INSTALL;
		m->left_install = false;
		return;
	}
	if (m->selected >= 0 && m->enabled[m->selected])
		return;
	if (m->selected == ITEM_INSTALL)
		m->left_install = true;
	for (size_t i = 0; i < sizeof(FALLBACK) / sizeof(FALLBACK[0]); i++)
		if (m->enabled[FALLBACK[i]])
		{
			m->selected = FALLBACK[i];
			return;
		}
}

/* Cell of an item; the toggle reports column 0. */
static void cell_of(MenuItem item, int *col, int *row)
{
	for (int r = 0; r < ROWS; r++)
		for (int c = 0; c < COLS; c++)
			if (GRID[r][c] == item)
			{
				*col = c;
				*row = r;
				return;
			}
	*col = *row = 0;
}

void menu_move(Menu *m, int dx, int dy)
{
	if (dx == 0 && dy == 0)
		return;
	int col, row;
	cell_of(m->selected, &col, &row);
	for (;;)
	{
		col += dx;
		row += dy;
		if (col < 0 || col >= COLS || row < 0 || row >= ROWS)
			return; /* edge: keep the current selection */
		MenuItem it = GRID[row][col];
		if (it != m->selected && m->enabled[it])
		{
			m->selected = it;
			m->left_install = false; /* the user's choice from now on */
			return;
		}
	}
}

MenuRect menu_item_rect(MenuItem item)
{
	MenuRect r = {0, 0, 0, 0};
	switch (item)
	{
	case ITEM_INSTALL:   r = (MenuRect){MARGIN, ROW0_Y, BTN_W, BTN_H}; break;
	case ITEM_UNINSTALL: r = (MenuRect){MARGIN + BTN_W + GAP, ROW0_Y, BTN_W, BTN_H}; break;
	case ITEM_RESTART:   r = (MenuRect){MARGIN, ROW1_Y, BTN_W, BTN_H}; break;
	case ITEM_EXIT:      r = (MenuRect){MARGIN + BTN_W + GAP, ROW1_Y, BTN_W, BTN_H}; break;
	case ITEM_RESTART_AFTER: r = (MenuRect){MARGIN, TOGGLE_Y, 2 * BTN_W + GAP, TOGGLE_H}; break;
	default: break;
	}
	return r;
}

MenuItem menu_item_at(int x, int y)
{
	for (int i = 0; i < ITEM_COUNT; i++)
	{
		MenuRect r = menu_item_rect((MenuItem)i);
		if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
			return (MenuItem)i;
	}
	return ITEM_NONE;
}
