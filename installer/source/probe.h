/*
 * probe - collect the SystemState that checks.c judges (3DS only).
 *
 * Asks Luma3DS (luma_system.c), the title manager (am) and the SD card.
 * Needs amInit() and romfsInit() to have been called.
 */
#ifndef PROBE_H
#define PROBE_H

#include <stddef.h>

#include "checks.h"

#define BUNDLED_DIR   "romfs:/luma/titles"
#define SD_TITLES_DIR "sdmc:/luma/titles"
#define LUMA_CONFIG   "sdmc:/luma/config.ini"
#define CONFIG_BACKUP "sdmc:/3ds/badge-arcade-reclawed/config.ini.bak"

void probe_system(SystemState *s);

/* Paths of the bundled and the installed code.ips of GAME_TITLES[index].
 * Luma3DS names title folders with 16 upper-case hex digits. */
void probe_patch_paths(int index, char *bundled, char *on_sd, size_t len);

#endif
