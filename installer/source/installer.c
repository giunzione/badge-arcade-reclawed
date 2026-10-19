/*
 * installer - see installer.h.
 */
#include "installer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "fileops.h"
#include "ips.h"
#include "luma_config.h"

/* Luma3DS reads config.ini into a 0x2400-byte buffer; a bigger file can't be
 * a real config, so refuse it rather than guess. */
#define LUMA_CONFIG_MAX_SIZE 0x2400

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

InstResult inst_copy_patch(const char *src, const char *dst, char *msg, size_t msg_len)
{
	unsigned char *ips;
	size_t size;
	unsigned records;
	InstResult r;

	if (fops_read(src, &ips, &size, FOPS_MAX_READ) != 0)
		return report(INST_FAILED, msg, msg_len, "can't read the bundled patch");
	if (!ips_is_valid(ips, size, &records))
		r = report(INST_FAILED, msg, msg_len, "bundled patch is damaged");
	else if (fops_same_content(dst, ips, size))
		r = report(INST_ALREADY, msg, msg_len, "patch already installed");
	else if (fops_mkdirs_for(dst) != 0)
		r = report(INST_FAILED, msg, msg_len, "can't create the patch folder");
	else if (fops_write_replace(dst, ips, size) != 0)
		r = report(INST_FAILED, msg, msg_len, "can't write the patch");
	else
		r = report(INST_DONE, msg, msg_len, "patch installed (%u changes)", records);
	free(ips);
	return r;
}

InstResult inst_remove_patch(const char *path, char *msg, size_t msg_len)
{
	if (!fops_exists(path))
		return report(INST_ALREADY, msg, msg_len, "no patch installed");
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
