/*
 * manifest - the list of available patches (patches.txt).
 *
 * The same file is attached to every GitHub release and bundled in the app's
 * RomFS, written by tools/make_manifest.py from the TOMLs in patches/:
 *
 *     # comment
 *     reclawed-patches 1 v0.4.0
 *     0004000000153600 u5136 EUR patch-EUR-u5136.ips 1186 9f86d0...(64 hex)
 *
 * The header gives the format (1) and the release it belongs to. Each patch
 * line: base title ID, which code it patches ("u" + version of the update,
 * or "b" + version of the base game when no update is installed), region,
 * file name (next to patches.txt), size and SHA-256 of the file.
 *
 * Pure parsing and lookup, unit-tested in tests/test_manifest.c.
 */
#ifndef MANIFEST_H
#define MANIFEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sha256.h"

#define MANIFEST_FORMAT 1
#define MANIFEST_MAX_ENTRIES 32
#define MANIFEST_MAX_PATCH_SIZE (256 * 1024) /* a real patch is ~1 KB */

/* Which code of the game a patch applies to. */
#define CODE_UPDATE 'u' /* the update's code (runs when the update is installed) */
#define CODE_BASE   'b' /* the base game's code (no update installed) */

typedef struct
{
	uint64_t base_tid;
	char code;              /* CODE_UPDATE or CODE_BASE */
	unsigned version;       /* title version of that code */
	char region[8];
	char file[48];          /* only [A-Za-z0-9._-]: safe in a URL and a path */
	size_t size;
	char sha256[SHA256_HEX_LEN + 1];
} PatchEntry;

typedef struct
{
	char release[32];       /* e.g. "v0.4.0" */
	int count;
	PatchEntry entries[MANIFEST_MAX_ENTRIES];
} Manifest;

typedef enum
{
	MANIFEST_OK = 0,
	MANIFEST_ERR_HEADER,   /* first real line isn't "reclawed-patches <n> <release>" */
	MANIFEST_ERR_FORMAT,   /* a format newer than this app understands */
	MANIFEST_ERR_LINE,     /* a patch line is malformed */
	MANIFEST_ERR_TOO_MANY, /* more than MANIFEST_MAX_ENTRIES patches */
} ManifestStatus;

/* Parse `len` bytes of patches.txt. On error `m` holds no entries. */
ManifestStatus manifest_parse(const char *text, size_t len, Manifest *m);

/* The entry for this game code, or NULL. */
const PatchEntry *manifest_find(const Manifest *m, uint64_t base_tid, char code, unsigned version);

/* True if `data` is exactly the file the entry describes (size and SHA-256). */
bool manifest_entry_matches(const PatchEntry *e, const void *data, size_t size);

/* What happened to the online copy of one game's patch. */
typedef enum
{
	FETCH_NOT_TRIED,  /* no manifest online, or the game isn't installed */
	FETCH_NOT_LISTED, /* the latest release has no patch for this game code */
	FETCH_OK,         /* downloaded, size and SHA-256 match */
	FETCH_FAILED,     /* listed, but the download failed */
	FETCH_BAD_HASH,   /* downloaded, but it isn't the file the manifest describes:
	                   * corrupted or tampered with, never installed */
} FetchResult;

/* Where a patch comes from. */
typedef enum
{
	SOURCE_NONE,    /* no patch for this game code anywhere */
	SOURCE_BUILTIN, /* bundled in the app */
	SOURCE_ONLINE,  /* downloaded from the latest release */
} PatchSource;

/* The hybrid rule: the latest release online when its manifest was fetched
 * (`online` not NULL), lists this game code and its file downloaded and
 * verified (`online_file_ok`); otherwise the app's built-in copy, if it has
 * one. `*out` gets the chosen entry (NULL for SOURCE_NONE). */
PatchSource manifest_choose(const Manifest *online, bool online_file_ok, const Manifest *builtin,
                            uint64_t base_tid, char code, unsigned version, const PatchEntry **out);

#endif
