/*
 * installer - see installer.h.
 */
#include "installer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "fileops.h"
#include "installed.h"
#include "ips.h"
#include "luma_config.h"

/* Luma3DS reads config.ini into a 0x2400-byte buffer; a bigger file can't be
 * a real config, so refuse it rather than guess. */
#define LUMA_CONFIG_MAX_SIZE 0x2400

/* Writes the outcome message (printf-style) into `msg`, if there is one, and
 * returns `r`, so actions can end with `return report(...)`. */
static InstResult report(InstResult r, char *msg, size_t msg_len, const char *fmt, ...)
	PRINTF_LIKE(4, 5);
static InstResult report(InstResult r, char *msg, size_t msg_len, const char *fmt, ...)
{
	if (msg && msg_len)
	{
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(msg, msg_len, fmt, ap);
		va_end(ap);
	}
	return r;
}

InstResult inst_write_patch(const void *ips, size_t size, const char *dst, char *msg,
                            size_t msg_len)
{
	unsigned records;
	if (!ips || !ips_is_valid(ips, size, &records))
		return report(INST_FAILED, msg, msg_len, "patch file is damaged");
	if (fops_same_content(dst, ips, size))
		return report(INST_ALREADY, msg, msg_len, "patch already installed");
	if (fops_mkdirs_for(dst) != 0)
		return report(INST_FAILED, msg, msg_len, "can't create the patch folder");
	if (fops_write_replace(dst, ips, size) != 0)
		return report(INST_FAILED, msg, msg_len, "can't write the patch");
	return report(INST_DONE, msg, msg_len, "patch installed (%u changes)", records);
}

InstResult inst_copy_patch(const char *src, const char *dst, char *msg, size_t msg_len)
{
	unsigned char *ips;
	size_t size;
	if (fops_read(src, &ips, &size, FOPS_MAX_READ) != 0)
		return report(INST_FAILED, msg, msg_len, "can't read the bundled patch");
	InstResult r = inst_write_patch(ips, size, dst, msg, msg_len);
	free(ips);
	return r;
}

InstResult inst_write_record(const char *path, const char *release, char code, unsigned version,
                             const void *ips, size_t size, char *msg, size_t msg_len)
{
	char text[INSTALLED_MAX_SIZE];
	int n = installed_format(text, sizeof(text), release, code, version, ips, size);
	if (n < 0)
		return report(INST_FAILED, msg, msg_len, "can't describe the patch");
	if (fops_same_content(path, text, (size_t)n))
		return report(INST_ALREADY, msg, msg_len, "record already up to date");
	if (fops_mkdirs_for(path) != 0 || fops_write_replace(path, text, (size_t)n) != 0)
		return report(INST_FAILED, msg, msg_len, "can't write " INSTALLED_FILE);
	return report(INST_DONE, msg, msg_len, "%s recorded", release);
}

InstResult inst_remove_patch(const char *path, const char *record_path, char *msg, size_t msg_len)
{
	/* The record goes first: a record without its patch would be harmless,
	 * but never let one describe a code.ips that is still there. */
	bool had_record = record_path && fops_exists(record_path);
	if (had_record && (remove(record_path) != 0 || fops_exists(record_path)))
		return report(INST_FAILED, msg, msg_len, "can't delete " INSTALLED_FILE);
	if (!fops_exists(path))
		return had_record ? report(INST_DONE, msg, msg_len, "leftover " INSTALLED_FILE " removed")
		                  : report(INST_ALREADY, msg, msg_len, "no patch installed");
	if (remove(path) != 0 || fops_exists(path))
		return report(INST_FAILED, msg, msg_len, "can't delete the patch");
	return report(INST_DONE, msg, msg_len, "patch removed");
}

InstResult inst_enable_game_patching(const char *cfg_path, const char *backup_path,
                                     char *msg, size_t msg_len)
{
	unsigned char *text;
	size_t size;
	LumaCfgInfo info;

	if (!fops_exists(cfg_path))
		return report(INST_FAILED, msg, msg_len, "Luma3DS config.ini not found");
	if (fops_read(cfg_path, &text, &size, LUMA_CONFIG_MAX_SIZE) != 0)
		return report(INST_FAILED, msg, msg_len, "can't read config.ini");

	InstResult r;
	LumaCfgStatus st = lumacfg_parse((const char *)text, size, &info);
	if (st != LUMACFG_OK)
		r = report(INST_FAILED, msg, msg_len, "config.ini: %s", lumacfg_status_str(st));
	else if (info.game_patching == 1)
		r = report(INST_ALREADY, msg, msg_len, "game patching already enabled");
	else if (fops_mkdirs_for(backup_path) != 0 ||
	         fops_write_replace(backup_path, text, size) != 0)
		r = report(INST_FAILED, msg, msg_len, "can't back up config.ini");
	else if (fops_patch_byte(cfg_path, info.value_offset, '1') != 0)
		r = report(INST_FAILED, msg, msg_len, "can't write config.ini");
	else
	{
		/* Read back: the file must parse the same, now with the option on. */
		unsigned char *after;
		size_t after_size;
		LumaCfgInfo check;
		if (fops_read(cfg_path, &after, &after_size, LUMA_CONFIG_MAX_SIZE) == 0 &&
		    after_size == size &&
		    lumacfg_parse((const char *)after, after_size, &check) == LUMACFG_OK &&
		    check.game_patching == 1)
			r = report(INST_DONE, msg, msg_len, "game patching enabled");
		else if (fops_patch_byte(cfg_path, info.value_offset, '0') == 0)
			r = report(INST_FAILED, msg, msg_len, "config.ini check failed, change undone");
		else
			r = report(INST_FAILED, msg, msg_len, "config.ini check failed, restore the backup");
		free(after);
	}
	free(text);
	return r;
}
