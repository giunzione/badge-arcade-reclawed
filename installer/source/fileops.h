/*
 * fileops - small file helpers on top of stdio.
 *
 * Only standard C plus mkdir(), so the same code runs on the 3DS (paths like
 * "sdmc:/luma/...") and on the PC for the unit tests (tests/test_fileops.c).
 * Functions that return int return 0 on success and -1 on failure.
 */
#ifndef FILEOPS_H
#define FILEOPS_H

#include <stdbool.h>
#include <stddef.h>

/* Largest file fops_read() will load. Patches and config.ini are a few KB. */
#define FOPS_MAX_READ (1024 * 1024)

/* Returns true if `path` exists (file or directory). */
bool fops_exists(const char *path);

/* Creates the directory `dir` and all its missing parents, like "mkdir -p".
 * Directories that already exist are fine. */
int fops_mkdirs(const char *dir);

/* Creates the parent directories of the file path `file_path`. */
int fops_mkdirs_for(const char *file_path);

/* Reads the whole file at `path` into a new buffer.
 *
 * On success *data is a malloc'd buffer the caller frees, and *size the file
 * size. A NUL byte follows the data (not counted in *size), so text can be
 * scanned safely. Fails for files larger than `max` bytes. */
int fops_read(const char *path, unsigned char **data, size_t *size, size_t max);

/* Writes `size` bytes at `data` to `path`, replacing the file if present.
 *
 * The data goes to "<path>.tmp" first and is read back; only then is the old
 * file swapped out, so a failed write never leaves a half-written file at
 * `path`. */
int fops_write_replace(const char *path, const void *data, size_t size);

/* Returns true if the file at `path` exists and holds exactly `size` bytes
 * equal to `data`. */
bool fops_same_content(const char *path, const void *data, size_t size);

/* Overwrites the byte at `offset` of the file at `path` with `value`, without
 * changing anything else in the file. */
int fops_patch_byte(const char *path, size_t offset, unsigned char value);

#endif
