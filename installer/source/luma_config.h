/*
 * luma_config - read and edit the text of Luma3DS's config.ini.
 *
 * Pure string handling, no file or 3DS calls, so it is unit-tested on the PC
 * (see tests/test_luma_config.c).
 *
 * Why so careful: when Luma3DS can't parse config.ini (unknown value, wrong
 * format version, ...) it silently resets *every* option to its default. So we
 * never rewrite the file: we only locate the single character holding the
 * value of [boot] enable_game_patching and flip it from '0' to '1'. The file
 * keeps its size, comments, line endings and every other option.
 */
#ifndef LUMA_CONFIG_H
#define LUMA_CONFIG_H

#include <stddef.h>

/* config.ini format version (the [meta] section) this code understands.
 * Luma v13+ writes 3.x; any minor version is accepted because the edit only
 * touches one existing value. */
#define LUMACFG_SUPPORTED_MAJOR 3

typedef enum
{
	LUMACFG_OK = 0,
	LUMACFG_ERR_NO_VERSION,          /* [meta] config_version_major missing */
	LUMACFG_ERR_UNSUPPORTED_VERSION, /* format major version is not 3 */
	LUMACFG_ERR_NO_KEY,              /* [boot] enable_game_patching missing */
	LUMACFG_ERR_DUPLICATE_KEY,       /* the key appears more than once */
	LUMACFG_ERR_BAD_VALUE,           /* the value is not exactly 0 or 1 */
} LumaCfgStatus;

typedef struct
{
	int version_major;   /* -1 when missing */
	int version_minor;   /* -1 when missing */
	int game_patching;   /* 0 or 1; -1 when unknown */
	size_t value_offset; /* offset in the text of the '0'/'1' character */
} LumaCfgInfo;

/* Parses `len` bytes of config.ini text (no NUL terminator needed).
 *
 * Fills `info` as far as possible, also on error. */
LumaCfgStatus lumacfg_parse(const char *text, size_t len, LumaCfgInfo *info);

/* Sets enable_game_patching to `enabled` (0/1) in the config.ini text, in place.
 *
 * The text length never changes. On error the text is left untouched. */
LumaCfgStatus lumacfg_set_game_patching(char *text, size_t len, int enabled);

/* Returns a short English description of `status` for the UI (at most 29
 * characters). */
const char *lumacfg_status_str(LumaCfgStatus status);

#endif
