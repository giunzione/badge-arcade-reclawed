/*
 * luma_system - see luma_system.h.
 *
 * Info types, from Luma3DS (sysmodules/rosalina/source/luma_config.c):
 *   0x10000, 0     version, packed as SYSTEM_VERSION(major, minor, build)
 *   0x10000, 3     single-option bit field (configData.config)
 *   0x10000, 0x203 1 if Luma runs from the SD ("/luma"), 0 from CTRNAND
 * In that bit field, bit 2 is PATCHGAMES ("Enable game patching").
 */
#include "luma_system.h"

#include <string.h>
#include <3ds.h>

#define LUMA_INFO_TYPE       0x10000
#define LUMA_INFO_VERSION    0
#define LUMA_INFO_CONFIG     3
#define LUMA_INFO_SD_MODE    0x203
#define LUMA_CFG_PATCHGAMES  2

void luma_system_query(LumaSystem *out)
{
	memset(out, 0, sizeof(*out));

	/* A real Luma never reports version 0. Emulators (Azahar) answer unknown
	 * info types with success and 0, so both conditions are needed. */
	s64 v = 0;
	if (R_FAILED(svcGetSystemInfo(&v, LUMA_INFO_TYPE, LUMA_INFO_VERSION)) || v == 0)
		return;
	out->detected = true;
	out->major = GET_VERSION_MAJOR((u32)v);
	out->minor = GET_VERSION_MINOR((u32)v);
	out->build = GET_VERSION_REVISION((u32)v);

	if (R_SUCCEEDED(svcGetSystemInfo(&v, LUMA_INFO_TYPE, LUMA_INFO_CONFIG)))
		out->game_patching_active = ((u64)v >> LUMA_CFG_PATCHGAMES) & 1;

	/* Older Luma versions don't know 0x203; they always used the SD. */
	out->config_on_sd = true;
	if (R_SUCCEEDED(svcGetSystemInfo(&v, LUMA_INFO_TYPE, LUMA_INFO_SD_MODE)))
		out->config_on_sd = v != 0;
}
