/*
 * Tests for source/menu.c: enabled items, D-pad movement, touch layout.
 */
#include "menu.h"
#include "test.h"

/* A CheckReport with just the fields the menu reads. */
static CheckReport report(bool can_install, bool nothing_to_do, int uninstall_target)
{
	CheckReport r;
	memset(&r, 0, sizeof(r));
	r.can_install = can_install;
	r.nothing_to_do = nothing_to_do;
	r.uninstall_target = uninstall_target;
	r.target = can_install ? 0 : -1;
	r.report_target = -1;
	return r;
}

static void report_mode(void)
{
	/* No patch for the game's version: the first button reports it. */
	Menu m;
	menu_init(&m);
	CheckReport r = report(false, false, -1);
	r.report_target = 0;
	menu_update(&m, &r, true);
	CHECK(m.enabled[ITEM_INSTALL]);
	CHECK(!m.enabled[ITEM_RESTART_AFTER]); /* nothing will be installed */
	CHECK(m.selected == ITEM_INSTALL);
}

static void enabled_items(void)
{
	Menu m;
	menu_init(&m);
	CHECK(m.restart_after_install);
	CHECK(m.selected == ITEM_INSTALL);

	CheckReport r = report(true, false, -1); /* first install */
	menu_update(&m, &r, true);
	CHECK(m.enabled[ITEM_INSTALL] && m.enabled[ITEM_RESTART_AFTER]);
	CHECK(!m.enabled[ITEM_UNINSTALL]);
	CHECK(m.enabled[ITEM_RESTART] && m.enabled[ITEM_EXIT]);

	r = report(true, true, 0); /* all done: only uninstall/restart/exit */
	menu_update(&m, &r, true);
	CHECK(!m.enabled[ITEM_INSTALL] && !m.enabled[ITEM_RESTART_AFTER]);
	CHECK(m.enabled[ITEM_UNINSTALL]);

	r = report(false, false, -1); /* blocked */
	menu_update(&m, &r, false);
	CHECK(!m.enabled[ITEM_INSTALL] && !m.enabled[ITEM_RESTART]);
	CHECK(m.enabled[ITEM_EXIT]);
}

static void selection_leaves_disabled_items(void)
{
	Menu m;
	menu_init(&m);
	CheckReport r = report(true, false, -1);
	menu_update(&m, &r, true);
	CHECK(m.selected == ITEM_INSTALL);

	/* After installing, Install turns off: the selection moves to Restart. */
	r = report(true, true, 0);
	menu_update(&m, &r, true);
	CHECK(m.selected == ITEM_RESTART);

	/* A still-enabled selection is kept. */
	m.selected = ITEM_UNINSTALL;
	menu_update(&m, &r, true);
	CHECK(m.selected == ITEM_UNINSTALL);

	/* Nothing but Exit left. */
	r = report(false, false, -1);
	menu_update(&m, &r, false);
	CHECK(m.selected == ITEM_EXIT);
}

static void selection_returns_to_install(void)
{
	/* At start the online check disables Install: the selection falls back
	 * to Restart, and comes back once Install (or Update) is possible, so (A)
	 * doesn't restart the console by surprise. */
	Menu m;
	menu_init(&m);
	CheckReport checking = report(false, false, -1);
	menu_update(&m, &checking, true);
	CHECK(m.selected == ITEM_RESTART);
	CheckReport ready = report(true, false, -1);
	menu_update(&m, &ready, true);
	CHECK(m.selected == ITEM_INSTALL);

	/* But a selection the user made in the meantime is kept. */
	menu_update(&m, &checking, true);
	CHECK(m.selected == ITEM_RESTART);
	menu_move(&m, 1, 0);
	CHECK(m.selected == ITEM_EXIT);
	menu_update(&m, &ready, true);
	CHECK(m.selected == ITEM_EXIT);
}

static void dpad_moves(void)
{
	Menu m;
	menu_init(&m); /* everything enabled */
	m.selected = ITEM_INSTALL;
	menu_move(&m, 1, 0);
	CHECK(m.selected == ITEM_UNINSTALL);
	menu_move(&m, 1, 0); /* right edge */
	CHECK(m.selected == ITEM_UNINSTALL);
	menu_move(&m, 0, 1);
	CHECK(m.selected == ITEM_EXIT);
	menu_move(&m, 0, 1);
	CHECK(m.selected == ITEM_RESTART_AFTER);
	menu_move(&m, 1, 0); /* the toggle spans the row: no change */
	CHECK(m.selected == ITEM_RESTART_AFTER);
	menu_move(&m, 0, -1); /* up from the toggle: left column */
	CHECK(m.selected == ITEM_RESTART);
	menu_move(&m, 0, -1);
	CHECK(m.selected == ITEM_INSTALL);
	menu_move(&m, 0, -1); /* top edge */
	CHECK(m.selected == ITEM_INSTALL);
	menu_move(&m, 0, 0);
	CHECK(m.selected == ITEM_INSTALL);
}

static void dpad_skips_disabled(void)
{
	Menu m;
	menu_init(&m);
	m.enabled[ITEM_RESTART] = false;
	m.enabled[ITEM_RESTART_AFTER] = false;
	m.selected = ITEM_INSTALL;
	menu_move(&m, 0, 1); /* Restart and the toggle are off: nowhere to go */
	CHECK(m.selected == ITEM_INSTALL);

	m.enabled[ITEM_RESTART_AFTER] = true;
	menu_move(&m, 0, 1); /* jumps over Restart */
	CHECK(m.selected == ITEM_RESTART_AFTER);
}

static void touch_layout(void)
{
	/* Every item's center maps back to the item; rectangles don't overlap. */
	for (int i = 0; i < ITEM_COUNT; i++)
	{
		MenuRect r = menu_item_rect((MenuItem)i);
		CHECK(r.w > 0 && r.h > 0);
		CHECK(r.x >= 0 && r.y >= 0 && r.x + r.w <= 320 && r.y + r.h <= 240);
		CHECK(menu_item_at(r.x + r.w / 2, r.y + r.h / 2) == (MenuItem)i);
		CHECK(menu_item_at(r.x, r.y) == (MenuItem)i);
		CHECK(menu_item_at(r.x + r.w - 1, r.y + r.h - 1) == (MenuItem)i);
	}
	CHECK(menu_item_at(0, 0) == ITEM_NONE);  /* status bar */
	CHECK(menu_item_at(160, 230) == ITEM_NONE); /* hint line */
	CHECK(menu_item_rect(ITEM_NONE).w == 0);
}

void test_menu(void)
{
	enabled_items();
	report_mode();
	selection_leaves_disabled_items();
	selection_returns_to_install();
	dpad_moves();
	dpad_skips_disabled();
	touch_layout();
}
