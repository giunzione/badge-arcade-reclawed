/*
 * Test runner: `run_tests <tmp dir>`. Exit code 0 when everything passes.
 */
#include <stdio.h>

#include "fileops.h"
#include "test.h"

int test_checks, test_failures;
const char *test_tmp;

const char *tmp_path(int slot, const char *name)
{
	static char buf[4][256];
	snprintf(buf[slot], sizeof(buf[slot]), "%s%s", test_tmp, name);
	return buf[slot];
}

int write_text(const char *path, const char *text)
{
	if (fops_mkdirs_for(path) != 0)
		return -1;
	FILE *f = fopen(path, "wb");
	if (!f)
		return -1;
	size_t n = strlen(text);
	int ok = fwrite(text, 1, n, f) == n;
	return fclose(f) == 0 && ok ? 0 : -1;
}

static void run(const char *name, void (*fn)(void))
{
	int before = test_failures;
	printf("%s\n", name);
	fn();
	if (test_failures == before)
		printf("  ok\n");
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: run_tests <empty tmp dir>/\n");
		return 2;
	}
	test_tmp = argv[1];

	run("luma_config", test_luma_config);
	run("ips", test_ips);
	run("fileops", test_fileops);
	run("installer", test_installer);
	run("checks", test_checks_run);
	run("menu", test_menu);
	run("manifest", test_manifest);

	printf("\n%d checks, %d failed\n", test_checks, test_failures);
	return test_failures ? 1 : 0;
}
