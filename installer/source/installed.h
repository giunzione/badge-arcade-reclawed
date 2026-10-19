/*
 * installed - the record the installer leaves next to the patch it wrote.
 *
 * sdmc:/luma/titles/<title id>/reclawed.txt (Luma3DS ignores it):
 *
 *     # Badge Arcade Reclawed: written by the installer app, do not edit.
 *     reclawed-installed 1 v0.4.0 u5136 9f86d0...(64 hex)
 *
 * Format (1), the release the patch came from, the game code it was made for
 * ("u"/"b" + version, as in patches.txt) and the SHA-256 of the code.ips that
 * was written. At start the app compares it with the code.ips on the SD and
 * the patch it would install now, to tell apart:
 *   - its own patch from an older release      -> "Update available"
 *   - its own patch from a newer release        -> kept (never downgraded)
 *   - its own patch for another game version   -> replaced
 *   - a code.ips it didn't write (custom build, manual copy, edited by hand:
 *     the hash doesn't match)                  -> replaced only after asking
 *
 * Pure text handling and logic, unit-tested in tests/test_installed.c.
 */
#ifndef INSTALLED_H
#define INSTALLED_H

#include <stdbool.h>
#include <stddef.h>

#include "sha256.h"

#define INSTALLED_FORMAT 1
#define INSTALLED_FILE "reclawed.txt"
#define INSTALLED_MAX_SIZE 1024 /* a real record is ~150 bytes */

typedef struct
{
	char release[32];                 /* e.g. "v0.4.0" */
	char code;                        /* CODE_UPDATE / CODE_BASE (manifest.h) */
	unsigned version;                 /* title version of that code */
	char sha256[SHA256_HEX_LEN + 1];  /* of the code.ips written next to it */
} InstalledRecord;

/* Write the record for the patch `ips` into `out`. Returns the text length,
 * or -1 if it doesn't fit or `release` isn't a single printable word. */
int installed_format(char *out, size_t len, const char *release, char code, unsigned version,
                     const void *ips, size_t size);

/* Parse a record file. False if it isn't one this app understands. */
bool installed_parse(const char *text, size_t len, InstalledRecord *out);

typedef enum
{
	RELEASE_OLDER,   /* a is older than b */
	RELEASE_SAME,
	RELEASE_NEWER,   /* a is newer than b */
	RELEASE_UNKNOWN, /* not both of the form [v]X[.Y[.Z]][-suffix], e.g. "dev" */
} ReleaseOrder;

/* Compare two release tags. Numbers compare numerically; a "-suffix" marks a
 * pre-release, older than the same numbers without one (v0.5.0-beta < v0.5.0);
 * two suffixes compare as text (beta < beta2 < rc1). */
ReleaseOrder release_compare(const char *a, const char *b);

/* The patch on the SD compared with the one the app would install now. */
typedef enum
{
	PATCH_NOT_ON_SD,     /* no code.ips on the SD */
	PATCH_SAME,          /* exactly the patch the app would install */
	PATCH_OLDER,         /* the app's own patch from an older release */
	PATCH_NEWER,         /* the app's own patch from a newer release: keep it */
	PATCH_OTHER_RELEASE, /* the app's own patch, release order unknown or same tag
	                      * with other bytes (e.g. dev builds): replace */
	PATCH_OTHER_VERSION, /* the app's own patch, made for another game version
	                      * (e.g. installed before the game's update): replace */
	PATCH_FOREIGN,       /* not written by the app, or changed since: ask first */
} PatchState;

/* Classify the code.ips on the SD.
 *   on_sd           a code.ips is there
 *   same_as_chosen  it is byte for byte the patch the app would install
 *   rec             the record next to it, or NULL if missing / unreadable
 *   sd_sha256       hash of the code.ips on the SD
 *   chosen_release  release of the patch the app would install ("" if none)
 *   code, version   the game code that runs now */
PatchState installed_classify(bool on_sd, bool same_as_chosen, const InstalledRecord *rec,
                              const char *sd_sha256, const char *chosen_release, char code,
                              unsigned version);

#endif
