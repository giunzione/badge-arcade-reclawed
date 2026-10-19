/*
 * probe - collect the SystemState that checks.c judges (3DS only).
 *
 * Asks Luma3DS (luma_system.c), the title manager (am), the SD card and the
 * app's own RomFS, then - once the online check is over - picks the patch for
 * each installed game (manifest_choose(): online if possible, else built-in)
 * and compares it with what is on the SD.
 *
 * Needs amInit() and romfsInit() to have been called.
 */
#ifndef PROBE_H
#define PROBE_H

#include <stddef.h>

#include "checks.h"
#include "online.h"

#define BUILTIN_DIR   "romfs:/patches"         /* patches.txt + patch-*.ips */
#define SD_TITLES_DIR "sdmc:/luma/titles"
#define LUMA_CONFIG   "sdmc:/luma/config.ini"
#define CONFIG_BACKUP "sdmc:/3ds/badge-arcade-reclawed/config.ini.bak"

/* Local facts: Luma, config, installed games, built-in patches.
 * Leaves s->online at ONLINE_CHECKING and no patch chosen. */
void probe_system(SystemState *s);

/* Tell the online job which game codes to download a patch for. */
void probe_online_wants(const SystemState *s, OnlineJob *job);

/* Choose a patch for every installed game, from `job` (NULL or unfinished:
 * still checking) and the built-in patches, and compare it with the SD. */
void probe_resolve(SystemState *s, OnlineJob *job);

/* The bytes of the patch chosen for GAME_TITLES[index] by the last
 * probe_resolve(), or NULL. Owned by the probe (or the online job). */
const unsigned char *probe_patch_data(int index, size_t *size);

/* sdmc:/luma/titles/<title id>/code.ips for GAME_TITLES[index]. Luma3DS names
 * title folders with 16 upper-case hex digits. */
void probe_sd_patch_path(int index, char *out, size_t len);

/* sdmc:/luma/titles/<title id>/reclawed.txt: the record of the patch the
 * app installed there (installed.h). */
void probe_sd_record_path(int index, char *out, size_t len);

/* Free the built-in patch copies. */
void probe_free(void);

#endif
