/*
 * fileops - small file helpers on top of stdio.
 *
 * Only standard C plus mkdir(), so the same code runs on the 3DS (paths like
 * "sdmc:/luma/...") and on the PC for the unit tests (tests/test_fileops.c).
 * Functions return 0 on success and -1 on failure.
 */
#ifndef FILEOPS_H
#define FILEOPS_H

#include <stdbool.h>
#include <stddef.h>

/* Largest file fops_read() will load. Patches and config.ini are a few KB. */
#define FOPS_MAX_READ (1024 * 1024)

bool fops_exists(const char *path);

/* Create a directory and all missing parents ("mkdir -p"). */
int fops_mkdirs(const char *dir);

/* Create the parent directories of a file path. */
int fops_mkdirs_for(const char *file_path);

/* Read a whole file into a malloc'd buffer (caller frees). A NUL byte is
 * appended after the data (not counted in *size) so text can be scanned
 * safely. Fails for files larger than `max`. */
int fops_read(const char *path, unsigned char **data, size_t *size, size_t max);

/* Write `data` to `path`, replacing it if present. The data goes to
 * "<path>.tmp" first and is read back; only then is the old file swapped out,
 * so a failed write never leaves a half-written file at `path`. */
int fops_write_replace(const char *path, const void *data, size_t size);

/* True if the file at `path` exists and holds exactly `data`. */
bool fops_same_content(const char *path, const void *data, size_t size);

/* Overwrite one byte in place, without changing anything else in the file. */
int fops_patch_byte(const char *path, size_t offset, unsigned char value);

#endif
