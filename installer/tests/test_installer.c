/*
 * Tests for source/installer.c: the two install actions on a fake SD card
 * inside the temporary folder.
 */
#include <stdlib.h>

#include "fileops.h"
#include "installer.h"
#include "test.h"

/* A tiny valid IPS: one 2-byte record. */
static const char IPS_A[] = "PATCH\x00\x00\x10\x00\x02" "ab" "EOF";
static const char IPS_B[] = "PATCH\x00\x00\x10\x00\x02" "xy" "EOF";

static const char CONFIG[] =
	"[meta]\nconfig_version_major = 3\nconfig_version_minor = 13\n"
	"[boot]\nautoboot_emunand = 0\nenable_game_patching = 0\n"
	"[rosalina]\nplugin_loader_enabled = 1\n";

static int write_bytes(const char *path, const char *data, size_t size)
{
	return fops_mkdirs_for(path) == 0 && fops_write_replace(path, data, size) == 0 ? 0 : -1;
}

static void copy_patch(void)
{
	char msg[96];
	const char *src = tmp_path(0, "inst/romfs/code.ips");
	const char *dst = tmp_path(1, "inst/sd/luma/titles/0004000000153600/code.ips");
	CHECK(write_bytes(src, IPS_A, sizeof(IPS_A) - 1) == 0);

	/* Fresh install creates the folders. */
	CHECK(inst_copy_patch(src, dst, msg, sizeof(msg)) == INST_DONE);
	CHECK(fops_same_content(dst, IPS_A, sizeof(IPS_A) - 1));
	CHECK_STR(msg, "patch installed (1 changes)");

	/* Same file again: nothing to do. */
	CHECK(inst_copy_patch(src, dst, msg, sizeof(msg)) == INST_ALREADY);

	/* A different (older) patch on the SD gets replaced. */
	CHECK(write_bytes(dst, IPS_B, sizeof(IPS_B) - 1) == 0);
	CHECK(inst_copy_patch(src, dst, msg, sizeof(msg)) == INST_DONE);
	CHECK(fops_same_content(dst, IPS_A, sizeof(IPS_A) - 1));

	/* Damaged or missing source: refused, SD untouched. */
	const char *bad = tmp_path(2, "inst/romfs/bad.ips");
	CHECK(write_bytes(bad, "PATCHjunk", 9) == 0);
	CHECK(inst_copy_patch(bad, dst, msg, sizeof(msg)) == INST_FAILED);
	CHECK(fops_same_content(dst, IPS_A, sizeof(IPS_A) - 1));
	CHECK(inst_copy_patch(tmp_path(2, "inst/romfs/none.ips"), dst, msg, sizeof(msg)) == INST_FAILED);

	/* NULL message buffer is allowed. */
	CHECK(inst_copy_patch(src, dst, NULL, 0) == INST_ALREADY);
}

static void enable_game_patching(void)
{
	char msg[96];
	const char *cfg = tmp_path(0, "inst/sd/luma/config.ini");
	const char *bak = tmp_path(1, "inst/sd/3ds/reclawed/config.ini.bak");
	CHECK(write_text(cfg, CONFIG) == 0);

	CHECK(inst_enable_game_patching(cfg, bak, msg, sizeof(msg)) == INST_DONE);
	CHECK_STR(msg, "game patching enabled");

	/* The backup is the original file; the config differs in one byte only. */
	CHECK(fops_same_content(bak, CONFIG, sizeof(CONFIG) - 1));
	unsigned char *now;
	size_t size;
	CHECK(fops_read(cfg, &now, &size, FOPS_MAX_READ) == 0);
	CHECK(size == sizeof(CONFIG) - 1);
	CHECK(strstr((char *)now, "enable_game_patching = 1\n") != NULL);
	CHECK(strstr((char *)now, "plugin_loader_enabled = 1\n") != NULL);
	free(now);

	/* Second run: already on, backup not rewritten. */
	CHECK(write_text(bak, "untouched") == 0);
	CHECK(inst_enable_game_patching(cfg, bak, msg, sizeof(msg)) == INST_ALREADY);
	CHECK(fops_same_content(bak, "untouched", 9));
}

static void refuses_unknown_configs(void)
{
	char msg[96];
	const char *cfg = tmp_path(0, "inst/sd2/luma/config.ini");
	const char *bak = tmp_path(1, "inst/sd2/3ds/reclawed/config.ini.bak");

	CHECK(inst_enable_game_patching(cfg, bak, msg, sizeof(msg)) == INST_FAILED);
	CHECK_STR(msg, "Luma3DS config.ini not found");

	const char *v4 = "[meta]\nconfig_version_major = 4\n[boot]\nenable_game_patching = 0\n";
	CHECK(write_text(cfg, v4) == 0);
	CHECK(inst_enable_game_patching(cfg, bak, msg, sizeof(msg)) == INST_FAILED);
	CHECK_STR(msg, "config.ini: unsupported config version");
	CHECK(fops_same_content(cfg, v4, strlen(v4))); /* left alone */
	CHECK(!fops_exists(bak));

	/* Bigger than Luma could ever read: not a real config. */
	char *big = malloc(0x3000);
	memset(big, ';', 0x3000 - 1);
	big[0x3000 - 1] = '\0';
	CHECK(write_text(cfg, big) == 0);
	CHECK(inst_enable_game_patching(cfg, bak, msg, sizeof(msg)) == INST_FAILED);
	CHECK_STR(msg, "can't read config.ini");
	free(big);
}

void test_installer(void)
{
	copy_patch();
	enable_game_patching();
	refuses_unknown_configs();
}
