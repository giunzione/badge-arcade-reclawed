/*
 * ui - see ui.h.
 *
 * Colors follow the logo: dark navy background, its gold as the accent.
 * Text uses the console's system font, so no font file is bundled.
 */
#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <3ds.h>
#include <citro2d.h>

#include "qrcodegen.h"
#include "sprites.h"     /* tex3ds: sprite indices */
#include "sprites_t3x.h" /* the sprite sheet, linked into the app */

#define TOP_W 400
#define BOTTOM_W 320
#define SCREEN_H 240

/* Palette */
#define COL_BG        C2D_Color32(0x18, 0x1A, 0x26, 0xFF)
#define COL_PANEL     C2D_Color32(0x26, 0x29, 0x3B, 0xFF)
#define COL_PANEL_HI  C2D_Color32(0x34, 0x38, 0x50, 0xFF)
#define COL_PANEL_OFF C2D_Color32(0x1F, 0x21, 0x2F, 0xFF)
#define COL_TEXT      C2D_Color32(0xEC, 0xEA, 0xF2, 0xFF)
#define COL_DIM       C2D_Color32(0x8D, 0x90, 0xA6, 0xFF)
#define COL_OFF       C2D_Color32(0x55, 0x58, 0x6C, 0xFF)
#define COL_GOLD      C2D_Color32(0xC5, 0x92, 0x16, 0xFF)
#define COL_GOLD_HI   C2D_Color32(0xE2, 0xAC, 0x2A, 0xFF)
#define COL_INK       C2D_Color32(0x1A, 0x14, 0x06, 0xFF) /* text on gold */
#define COL_GREEN     C2D_Color32(0x4C, 0xC3, 0x8A, 0xFF)
#define COL_CYAN      C2D_Color32(0x4F, 0xB3, 0xD9, 0xFF)
#define COL_YELLOW    C2D_Color32(0xE8, 0xB9, 0x31, 0xFF)
#define COL_RED       C2D_Color32(0xE5, 0x53, 0x4B, 0xFF)
#define COL_SHADE     C2D_Color32(0x00, 0x00, 0x00, 0xB0)

/* Dialog geometry (bottom screen) */
#define DLG_X 16
#define DLG_Y 20
#define DLG_W (BOTTOM_W - 2 * DLG_X)
#define DLG_H 200
#define DLG_BTN_H 36
#define DLG_BTN_Y (DLG_Y + DLG_H - DLG_BTN_H - 12)
#define DLG_BTN_W ((DLG_W - 36) / 2)

/* Report dialog: QR code area and its close button (bottom screen) */
#define QR_MAX_VERSION 12 /* 65 modules: still 3 px each on the screen */
#define QR_BOX 196
#define QR_X 6
#define QR_Y 38
#define CLOSE_X 210
#define CLOSE_Y 192
#define CLOSE_W 102
#define CLOSE_H 36

static C3D_RenderTarget *top_target, *bottom_target;
static C2D_TextBuf text_buf;
static C2D_SpriteSheet sprites;
static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
static bool qr_ok;
static unsigned frame; /* for the "checking" animation */

bool ui_init(void)
{
	gfxInitDefault();
	if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE) || !C2D_Init(C2D_DEFAULT_MAX_OBJECTS))
		return false;
	C2D_Prepare();
	top_target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	bottom_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	text_buf = C2D_TextBufNew(4096);
	sprites = C2D_SpriteSheetLoadFromMem(sprites_t3x, sprites_t3x_size);
	return top_target && bottom_target && text_buf;
}

void ui_exit(void)
{
	if (sprites)
		C2D_SpriteSheetFree(sprites);
	if (text_buf)
		C2D_TextBufDelete(text_buf);
	C2D_Fini();
	C3D_Fini();
	gfxExit();
}

/* ---- drawing helpers ------------------------------------------------- */

/* Draw a string; `flags` may add C2D_AlignCenter / C2D_AlignRight. */
static void text(const char *s, float x, float y, float scale, u32 color, u32 flags)
{
	C2D_Text t;
	C2D_TextParse(&t, text_buf, s);
	C2D_TextOptimize(&t);
	C2D_DrawText(&t, C2D_WithColor | flags, x, y, 0.5f, scale, scale, color);
}

/* Draw a string word-wrapped to `width` pixels. */
static void text_wrap(const char *s, float x, float y, float scale, u32 color, float width)
{
	C2D_Text t;
	C2D_TextParse(&t, text_buf, s);
	C2D_TextOptimize(&t);
	C2D_DrawText(&t, C2D_WithColor | C2D_WordWrap, x, y, 0.5f, scale, scale, color, width);
}

/* Draw a string centered inside a box, both ways. */
static void text_in_box(const char *s, float x, float y, float w, float h, float scale, u32 color)
{
	C2D_Text t;
	float tw, th;
	C2D_TextParse(&t, text_buf, s);
	C2D_TextOptimize(&t);
	C2D_TextGetDimensions(&t, scale, scale, &tw, &th);
	C2D_DrawText(&t, C2D_WithColor, x + (w - tw) / 2, y + (h - th) / 2, 0.5f, scale, scale, color);
}

/* Rectangle with round corners: two rectangles plus four circles. */
static void round_rect(float x, float y, float w, float h, float r, u32 color)
{
	C2D_DrawRectSolid(x + r, y, 0.5f, w - 2 * r, h, color);
	C2D_DrawRectSolid(x, y + r, 0.5f, w, h - 2 * r, color);
	C2D_DrawCircleSolid(x + r, y + r, 0.5f, r, color);
	C2D_DrawCircleSolid(x + w - r, y + r, 0.5f, r, color);
	C2D_DrawCircleSolid(x + r, y + h - r, 0.5f, r, color);
	C2D_DrawCircleSolid(x + w - r, y + h - r, 0.5f, r, color);
}

static u32 level_color(CheckLevel level)
{
	switch (level)
	{
	case CHECK_OK: return COL_GREEN;
	case CHECK_INFO: return COL_CYAN;
	case CHECK_WARN: return COL_YELLOW;
	case CHECK_STOP: return COL_RED;
	}
	return COL_DIM;
}

/* ---- top screen ---------------------------------------------------- */

static void draw_top(const CheckReport *r, const char *version)
{
	C2D_TargetClear(top_target, COL_BG);
	C2D_SceneBegin(top_target);

	if (sprites)
	{
		C2D_Image logo = C2D_SpriteSheetGetImage(sprites, sprites_logo_idx);
		C2D_DrawImageAt(logo, (TOP_W - logo.subtex->width) / 2.0f, 8, 0.5f, NULL, 1, 1);
	}

	char v[40];
	snprintf(v, sizeof(v), "v%s", version);
	text(v, TOP_W - 8, 4, 0.42f, COL_DIM, C2D_AlignRight);
	if (strstr(version, "fake") || strstr(version, "test"))
		text("TEST BUILD", 8, 4, 0.42f, COL_RED, 0);

	/* "powered by" + the Divisione3DSAntimog logo, bottom-right corner. */
	if (sprites)
	{
		C2D_Image d = C2D_SpriteSheetGetImage(sprites, sprites_divisione_idx);
		float lx = TOP_W - 6 - d.subtex->width, ly = SCREEN_H - 6 - d.subtex->height;
		C2D_DrawImageAt(d, lx, ly, 0.5f, NULL, 1, 1);
		text("powered by", lx - 4, ly + 5, 0.4f, COL_DIM, C2D_AlignRight);
	}

	/* One row per check, under a thin divider; stops above the corner logo.
	 * Five rows fit at full size; with six or seven (e.g. offline, with a
	 * patch on the SD) the rows get tighter and the text a bit smaller. */
	C2D_DrawRectSolid(24, 100, 0.5f, TOP_W - 48, 1, COL_PANEL_HI);
	float step = r->count <= 5 ? 21 : r->count == 6 ? 17 : 14;
	float size = r->count <= 5 ? 0.55f : r->count == 6 ? 0.5f : 0.45f;
	float y = 108;
	for (int i = 0; i < r->count && y < SCREEN_H - 34; i++, y += step)
	{
		C2D_DrawCircleSolid(34, y + step / 2.6f, 0.5f, r->count <= 5 ? 5 : 4,
		                    level_color(r->items[i].level));
		text(r->items[i].text, 48, y, size, COL_TEXT, 0);
	}
}

/* ---- bottom screen ------------------------------------------------- */

static const char *const ITEM_LABELS[ITEM_COUNT] = {
	[ITEM_INSTALL] = "Install",
	[ITEM_UNINSTALL] = "Uninstall",
	[ITEM_RESTART] = "Restart",
	[ITEM_EXIT] = "Exit",
	[ITEM_RESTART_AFTER] = "Restart after install",
};

static void draw_status(const CheckReport *r)
{
	const char *s;
	u32 color;
	static const char *const checking[] = {"Checking online   ", "Checking online.  ",
	                                       "Checking online.. ", "Checking online..."};
	if (r->checking)
	{
		s = checking[(frame / 20) % 4];
		color = COL_CYAN;
	}
	else if (r->report_target >= 0)
	{
		s = "No patch for your game version";
		color = COL_RED;
	}
	else if (r->nothing_to_do)
	{
		s = "Everything is installed";
		color = COL_GREEN;
	}
	else if (r->can_install && r->update_available)
	{
		s = "Update available";
		color = COL_GOLD_HI;
	}
	else if (r->can_install)
	{
		s = "Ready to install";
		color = COL_GOLD_HI;
	}
	else
	{
		s = "Can't install: see the top screen";
		color = COL_RED;
	}
	C2D_DrawRectSolid(0, 0, 0.5f, BOTTOM_W, 36, COL_PANEL);
	text_in_box(s, 0, 0, BOTTOM_W, 36, 0.6f, color);
}

static void draw_button(const CheckReport *r, const Menu *m, MenuItem item)
{
	MenuRect b = menu_item_rect(item);
	bool on = m->enabled[item];
	bool sel = on && m->selected == item;
	bool primary = item == ITEM_INSTALL && on;
	/* No patch for this version: the first button reports it instead. An
	 * older patch of ours on the SD: it updates it. */
	const char *label = ITEM_LABELS[item];
	if (item == ITEM_INSTALL && !r->can_install && r->report_target >= 0)
		label = "Report";
	else if (item == ITEM_INSTALL && r->can_install && r->update_available)
		label = "Update";

	if (sel) /* selection ring */
		round_rect(b.x - 3, b.y - 3, b.w + 6, b.h + 6, 9, primary ? COL_TEXT : COL_GOLD);
	u32 fill = !on ? COL_PANEL_OFF : primary ? (sel ? COL_GOLD_HI : COL_GOLD) : sel ? COL_PANEL_HI : COL_PANEL;
	round_rect(b.x, b.y, b.w, b.h, 7, fill);
	text_in_box(label, b.x, b.y, b.w, b.h, 0.65f,
	            !on ? COL_OFF : primary ? COL_INK : COL_TEXT);
}

static void draw_toggle(const Menu *m)
{
	MenuRect b = menu_item_rect(ITEM_RESTART_AFTER);
	bool on = m->enabled[ITEM_RESTART_AFTER];
	bool sel = on && m->selected == ITEM_RESTART_AFTER;

	if (sel)
		round_rect(b.x - 3, b.y - 3, b.w + 6, b.h + 6, 8, COL_GOLD);
	round_rect(b.x, b.y, b.w, b.h, 6, on ? (sel ? COL_PANEL_HI : COL_PANEL) : COL_PANEL_OFF);

	/* Checkbox */
	float cx = b.x + 10, cy = b.y + (b.h - 16) / 2.0f;
	C2D_DrawRectSolid(cx, cy, 0.5f, 16, 16, on ? COL_DIM : COL_OFF);
	C2D_DrawRectSolid(cx + 2, cy + 2, 0.5f, 12, 12, COL_PANEL_OFF);
	if (m->restart_after_install)
		C2D_DrawRectSolid(cx + 4, cy + 4, 0.5f, 8, 8, on ? COL_GOLD_HI : COL_OFF);

	text(ITEM_LABELS[ITEM_RESTART_AFTER], cx + 26, b.y + 7, 0.55f, on ? COL_TEXT : COL_OFF, 0);
}

static void dialog_button(float x, const char *label, bool primary)
{
	round_rect(x, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H, 6, primary ? COL_GOLD : COL_PANEL_HI);
	text_in_box(label, x, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H, 0.55f, primary ? COL_INK : COL_TEXT);
}

bool ui_set_report_url(const char *url)
{
	uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
	qr_ok = qrcodegen_encodeText(url, temp, qr, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
	                             QR_MAX_VERSION, qrcodegen_Mask_AUTO, true);
	return qr_ok;
}

/* The QR code in a white square, dark modules drawn as one rectangle per run. */
static void draw_qr(float x, float y, float box)
{
	C2D_DrawRectSolid(x, y, 0.5f, box, box, COL_TEXT);
	if (!qr_ok)
		return;
	int n = qrcodegen_getSize(qr);
	int px = (int)box / (n + 8); /* 4 modules of quiet zone on each side */
	float ox = x + (box - px * n) / 2.0f, oy = y + (box - px * n) / 2.0f;
	for (int row = 0; row < n; row++)
		for (int col = 0; col < n;)
		{
			if (!qrcodegen_getModule(qr, col, row))
			{
				col++;
				continue;
			}
			int run = col;
			while (run < n && qrcodegen_getModule(qr, run, row))
				run++;
			C2D_DrawRectSolid(ox + col * px, oy + row * px, 0.5f, (run - col) * px, px, COL_INK);
			col = run;
		}
}

static void draw_report(void)
{
	C2D_DrawRectSolid(0, 0, 0.5f, BOTTOM_W, SCREEN_H, COL_BG);
	text_in_box("No patch for your game version yet", 0, 4, BOTTOM_W, 28, 0.55f, COL_GOLD_HI);
	draw_qr(QR_X, QR_Y, QR_BOX);
	text_wrap("Scan this with your phone: it opens a GitHub issue already filled in "
	          "with your game's version. Just submit it.\n\n"
	          "Please don't upload any game files.",
	          CLOSE_X, QR_Y, 0.42f, COL_TEXT, CLOSE_W);
	round_rect(CLOSE_X, CLOSE_Y, CLOSE_W, CLOSE_H, 6, COL_GOLD);
	text_in_box("(B) Close", CLOSE_X, CLOSE_Y, CLOSE_W, CLOSE_H, 0.55f, COL_INK);
}

static void draw_dialog(const UiState *u)
{
	if (u->dialog == DIALOG_REPORT)
	{
		draw_report();
		return;
	}
	C2D_DrawRectSolid(0, 0, 0.5f, BOTTOM_W, SCREEN_H, COL_SHADE);
	round_rect(DLG_X, DLG_Y, DLG_W, DLG_H, 10, COL_PANEL);
	float left = DLG_X + 12, right = DLG_X + DLG_W - 12 - DLG_BTN_W;

	if (u->dialog == DIALOG_CONFIRM_UNINSTALL)
	{
		text_in_box("Remove the patch?", DLG_X, DLG_Y + 8, DLG_W, 30, 0.65f, COL_TEXT);
		text_wrap("Badge Arcade will start without it, and "
		          "need the servers again. Game patching "
		          "stays on in Luma3DS, for other mods.",
		          DLG_X + 16, DLG_Y + 46, 0.5f, COL_DIM, DLG_W - 32);
		dialog_button(left, "(B) Cancel", false);
		dialog_button(right, "(A) Remove", true);
		return;
	}
	if (u->dialog == DIALOG_CONFIRM_REPLACE)
	{
		text_in_box("Replace the patch?", DLG_X, DLG_Y + 8, DLG_W, 30, 0.65f, COL_TEXT);
		text_wrap("The code.ips on the SD wasn't installed "
		          "by this app (a custom build, a manual "
		          "copy or an edited file). It will be lost.",
		          DLG_X + 16, DLG_Y + 46, 0.5f, COL_DIM, DLG_W - 32);
		dialog_button(left, "(B) Cancel", false);
		dialog_button(right, "(A) Replace", true);
		return;
	}

	text_in_box(u->title, DLG_X, DLG_Y + 8, DLG_W, 30, 0.65f, u->failed ? COL_RED : COL_GREEN);
	float y = DLG_Y + 44;
	for (int i = 0; i < u->line_count; i++, y += 34)
	{
		C2D_DrawCircleSolid(left + 5, y + 14, 0.5f, 4, level_color(u->lines[i].level));
		text(u->lines[i].label, left + 16, y, 0.42f, COL_DIM, 0);
		text(u->lines[i].text, left + 16, y + 13, 0.5f, COL_TEXT, 0);
	}

	if (u->countdown_frames > 0)
	{
		char s[40];
		snprintf(s, sizeof(s), "Restarting in %d...", (u->countdown_frames + 59) / 60);
		text_in_box(s, DLG_X, DLG_BTN_Y - 28, DLG_W, 22, 0.55f, COL_GOLD_HI);
		dialog_button(left, "(B) Cancel", false);
		dialog_button(right, "(A) Restart now", true);
	}
	else
	{
		if (u->restart_failed)
			text_in_box("Restart failed: restart manually", DLG_X, DLG_BTN_Y - 28, DLG_W, 22,
			            0.5f, COL_YELLOW);
		round_rect(left, DLG_BTN_Y, DLG_W - 24, DLG_BTN_H, 6, COL_GOLD);
		text_in_box("(A) OK", left, DLG_BTN_Y, DLG_W - 24, DLG_BTN_H, 0.55f, COL_INK);
	}
}

static void draw_bottom(const CheckReport *r, const Menu *m, const UiState *u)
{
	C2D_TargetClear(bottom_target, COL_BG);
	C2D_SceneBegin(bottom_target);

	draw_status(r);
	draw_button(r, m, ITEM_INSTALL);
	draw_button(r, m, ITEM_UNINSTALL);
	draw_button(r, m, ITEM_RESTART);
	draw_button(r, m, ITEM_EXIT);
	draw_toggle(m);
	text("D-Pad / touch: choose    (A) select    START: exit", BOTTOM_W / 2.0f, 222, 0.4f,
	     COL_DIM, C2D_AlignCenter);

	if (u->dialog != DIALOG_NONE)
		draw_dialog(u);
}

void ui_draw(const CheckReport *r, const Menu *m, const UiState *u, const char *version)
{
	frame++;
	C2D_TextBufClear(text_buf);
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	draw_top(r, version);
	draw_bottom(r, m, u);
	C3D_FrameEnd(0);
}

int ui_dialog_button_at(const UiState *u, int x, int y)
{
	if (u->dialog == DIALOG_REPORT)
		return x >= CLOSE_X && x < CLOSE_X + CLOSE_W && y >= CLOSE_Y && y < CLOSE_Y + CLOSE_H ? 2 : 0;
	if (u->dialog == DIALOG_NONE || y < DLG_BTN_Y || y >= DLG_BTN_Y + DLG_BTN_H)
		return 0;
	int left = DLG_X + 12, right = DLG_X + DLG_W - 12 - DLG_BTN_W;
	bool two_buttons = u->dialog == DIALOG_CONFIRM_UNINSTALL ||
	                   u->dialog == DIALOG_CONFIRM_REPLACE || u->countdown_frames > 0;
	if (!two_buttons)
		return x >= left && x < DLG_X + DLG_W - 12 ? 1 : 0;
	if (x >= left && x < left + DLG_BTN_W)
		return 2; /* left: cancel */
	if (x >= right && x < right + DLG_BTN_W)
		return 1; /* right: confirm */
	return 0;
}
