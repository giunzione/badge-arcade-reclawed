/*
 * Tests for source/fileops.c, inside the temporary folder given to run_tests.
 */
#include <stdlib.h>

#include "fileops.h"
#include "test.h"

static void mkdirs_and_exists(void)
{
	const char *deep = tmp_path(0, "fileops/a/b/c");
	CHECK(fops_mkdirs(deep) == 0);
	CHECK(fops_exists(deep));
	CHECK(fops_mkdirs(deep) == 0); /* already there: still fine */

	CHECK(fops_mkdirs_for(tmp_path(0, "fileops/d/e/file.txt")) == 0);
	CHECK(fops_exists(tmp_path(0, "fileops/d/e")));
	CHECK(!fops_exists(tmp_path(0, "fileops/d/e/file.txt")));
}

static void read_and_compare(void)
{
	const char *p = tmp_path(0, "fileops/read.txt");
	CHECK(write_text(p, "hello") == 0);

	unsigned char *data;
	size_t size;
	CHECK(fops_read(p, &data, &size, 100) == 0);
	CHECK(size == 5 && memcmp(data, "hello", 5) == 0 && data[5] == '\0');
	free(data);

	CHECK(fops_read(p, &data, &size, 4) == -1); /* over the size limit */
	CHECK(data == NULL && size == 0);
	CHECK(fops_read(tmp_path(1, "fileops/missing"), &data, &size, 100) == -1);

	CHECK(fops_same_content(p, "hello", 5));
	CHECK(!fops_same_content(p, "hellO", 5));
	CHECK(!fops_same_content(p, "hell", 4));
	CHECK(!fops_same_content(tmp_path(1, "fileops/missing"), "", 0));
}

static void write_replace(void)
{
	const char *p = tmp_path(0, "fileops/replace.bin");
	CHECK(fops_write_replace(p, "first", 5) == 0);
	CHECK(fops_same_content(p, "first", 5));
	CHECK(fops_write_replace(p, "2nd", 3) == 0);
	CHECK(fops_same_content(p, "2nd", 3));
	CHECK(!fops_exists(tmp_path(1, "fileops/replace.bin.tmp")));

	/* Missing parent folder: fails and creates nothing. */
	CHECK(fops_write_replace(tmp_path(0, "fileops/nodir/x.bin"), "x", 1) == -1);
}

static void patch_byte(void)
{
	const char *p = tmp_path(0, "fileops/patch.txt");
	CHECK(write_text(p, "abc=0\n") == 0);
	CHECK(fops_patch_byte(p, 4, '1') == 0);
	CHECK(fops_same_content(p, "abc=1\n", 6));
	CHECK(fops_patch_byte(tmp_path(1, "fileops/missing"), 0, 'x') == -1);
}

void test_fileops(void)
{
	mkdirs_and_exists();
	read_and_compare();
	write_replace();
	patch_byte();
}
