/*
 * installer - the two actions of the installer, on plain file paths.
 *
 * Nothing 3DS-specific happens here, so the tests run them on the PC against
 * temporary folders (tests/test_installer.c). On the console the paths are
 * "romfs:/..." (bundled patches) and "sdmc:/..." (SD card).
 *
 * Every function writes a one-line outcome for the UI into `msg` (at most
 * `msg_len` bytes, NUL-terminated; `msg` may be NULL).
 */
#ifndef INSTALLER_H
#define INSTALLER_H

#include <stddef.h>

typedef enum
{
	INST_DONE,    /* the change was made and verified */
	INST_ALREADY, /* nothing to do, it was already in place */
	INST_FAILED,  /* nothing was changed; see the message */
} InstResult;

/* Writes the IPS of `size` bytes at `ips` to `dst`, creating folders as needed.
 *
 * The data is checked to be a valid IPS first, and the written file is read
 * back. A file already holding the same bytes gives INST_ALREADY. */
InstResult inst_write_patch(const void *ips, size_t size, const char *dst, char *msg,
                            size_t msg_len);

/* Same as inst_write_patch(), reading the IPS from the file `src`. */
InstResult inst_copy_patch(const char *src, const char *dst, char *msg, size_t msg_len);

/* Writes the record (installed.h) for the patch `ips` to `path`, next to the
 * code.ips: which release and game code (`code`, `version`) it came from, and
 * its SHA-256. */
InstResult inst_write_record(const char *path, const char *release, char code, unsigned version,
                             const void *ips, size_t size, char *msg, size_t msg_len);

/* Deletes the installed patch at `path` (uninstall), and its record at
 * `record_path` if there is one (NULL: none).
 *
 * Game patching is left on: other mods may rely on it, and without a code.ips
 * it changes nothing for Badge Arcade. */
InstResult inst_remove_patch(const char *path, const char *record_path, char *msg, size_t msg_len);

/* Sets enable_game_patching = 1 in the Luma3DS config.ini at `cfg_path`.
 *
 * The current file is first copied to `backup_path`. Only the one value
 * character is rewritten in place, then the file is parsed again; if that
 * check fails, the change is undone. */
InstResult inst_enable_game_patching(const char *cfg_path, const char *backup_path,
                                     char *msg, size_t msg_len);

#endif
