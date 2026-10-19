/*
 * installer - the two actions of the installer, on plain file paths.
 *
 * Nothing 3DS-specific happens here, so the tests run them on the PC against
 * temporary folders (tests/test_installer.c). On the console the paths are
 * "romfs:/..." (bundled patches) and "sdmc:/..." (SD card).
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

/* Copy the IPS at `src` to `dst`, creating folders as needed. The source is
 * checked to be a valid IPS first, and the copy is read back. */
InstResult inst_copy_patch(const char *src, const char *dst, char *msg, size_t msg_len);

/* Set enable_game_patching = 1 in the Luma3DS config.ini at `cfg_path`.
 * Before the edit the current file is copied to `backup_path`. Only the one
 * value character is rewritten in place, then the file is parsed again. */
InstResult inst_enable_game_patching(const char *cfg_path, const char *backup_path,
                                     char *msg, size_t msg_len);

#endif
