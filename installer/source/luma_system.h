/*
 * luma_system - ask the running Luma3DS about itself (3DS only).
 *
 * Luma's kernel extension answers svcGetSystemInfo(type 0x10000, ...) with its
 * version and the options it booted with. On stock firmware the call fails;
 * an emulator may "succeed" with 0. Both are reported as not detected.
 */
#ifndef LUMA_SYSTEM_H
#define LUMA_SYSTEM_H

#include <stdbool.h>

typedef struct
{
	bool detected;
	unsigned major, minor, build; /* Luma3DS version, e.g. 13.3.2 */
	bool game_patching_active;    /* "Enable game patching" as booted */
	bool config_on_sd;            /* config and luma/titles live on the SD, not CTRNAND */
} LumaSystem;

void luma_system_query(LumaSystem *out);

#endif
