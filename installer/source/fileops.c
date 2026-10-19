/*
 * fileops - see fileops.h.
 */
#include "fileops.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir((p), 0777)
#endif

#define PATH_MAX_LEN 256

bool fops_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

int fops_mkdirs(const char *dir)
{
	char buf[PATH_MAX_LEN];
	size_t n = strlen(dir);
	if (n == 0 || n >= sizeof(buf))
		return -1;
	memcpy(buf, dir, n + 1);

	/* Create every prefix that ends at a '/', then the full path. The start
	 * of the scan skips a device prefix like "sdmc:/" and a leading '/'. */
	char *p = strchr(buf, ':');
	p = p ? p + 1 : buf;
	while (*p == '/')
		p++;
	for (; *p; p++)
	{
		if (*p != '/')
			continue;
		*p = '\0';
		if (MKDIR(buf) != 0 && errno != EEXIST)
			return -1;
		*p = '/';
	}
	if (MKDIR(buf) != 0 && errno != EEXIST)
		return -1;
	return 0;
}

int fops_mkdirs_for(const char *file_path)
{
	char buf[PATH_MAX_LEN];
	size_t n = strlen(file_path);
	if (n >= sizeof(buf))
		return -1;
	memcpy(buf, file_path, n + 1);
	char *slash = strrchr(buf, '/');
	if (!slash || slash == buf)
		return 0; /* no parent to create */
	*slash = '\0';
	return fops_mkdirs(buf);
}

int fops_read(const char *path, unsigned char **data, size_t *size, size_t max)
{
	*data = NULL;
	*size = 0;
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;

	int ret = -1;
	unsigned char *buf = NULL;
	if (fseek(f, 0, SEEK_END) != 0)
		goto out;
	long len = ftell(f);
	if (len < 0 || (size_t)len > max || fseek(f, 0, SEEK_SET) != 0)
		goto out;
	buf = malloc((size_t)len + 1);
	if (!buf)
		goto out;
	if (fread(buf, 1, (size_t)len, f) != (size_t)len)
		goto out;
	buf[len] = '\0';
	*data = buf;
	*size = (size_t)len;
	buf = NULL;
	ret = 0;
out:
	free(buf);
	fclose(f);
	return ret;
}

bool fops_same_content(const char *path, const void *data, size_t size)
{
	unsigned char *cur;
	size_t cur_size;
	if (fops_read(path, &cur, &cur_size, FOPS_MAX_READ) != 0)
		return false;
	bool same = cur_size == size && memcmp(cur, data, size) == 0;
	free(cur);
	return same;
}

int fops_write_replace(const char *path, const void *data, size_t size)
{
	char tmp[PATH_MAX_LEN];
	if ((size_t)snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= sizeof(tmp))
		return -1;

	FILE *f = fopen(tmp, "wb");
	if (!f)
		return -1;
	size_t written = fwrite(data, 1, size, f);
	if (fclose(f) != 0 || written != size || !fops_same_content(tmp, data, size))
	{
		remove(tmp);
		return -1;
	}

	/* rename() doesn't overwrite on every platform (the 3DS SD included),
	 * so remove the old file first. */
	if (fops_exists(path) && remove(path) != 0)
	{
		remove(tmp);
		return -1;
	}
	return rename(tmp, path) == 0 ? 0 : -1;
}

int fops_patch_byte(const char *path, size_t offset, unsigned char value)
{
	FILE *f = fopen(path, "r+b");
	if (!f)
		return -1;
	int ok = fseek(f, (long)offset, SEEK_SET) == 0 && fputc(value, f) != EOF;
	if (fclose(f) != 0)
		ok = 0;
	return ok ? 0 : -1;
}
