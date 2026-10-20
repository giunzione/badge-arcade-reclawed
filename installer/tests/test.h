/*
 * Minimal test helpers for the installer's PC-side unit tests.
 *
 * CHECK() records a failure and keeps going, so one run reports every broken
 * expectation. run_tests.c prints the totals and sets the exit code.
 */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

extern int test_checks, test_failures;

/* Folder (with trailing '/') where file tests may create files. */
extern const char *test_tmp;

#define CHECK(cond)                                                          \
	do {                                                                     \
		test_checks++;                                                       \
		if (!(cond)) {                                                       \
			test_failures++;                                                 \
			printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
		}                                                                    \
	} while (0)

#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

/* Each test file exposes one entry point. */
void test_luma_config(void);
void test_ips(void);
void test_fileops(void);
void test_installer(void);
void test_checks_run(void);
void test_menu(void);
void test_manifest(void);
void test_installed(void);
void test_ipscfg(void);
void test_spotpass(void);

/* Build "<test_tmp><name>" into a static buffer (one at a time per slot). */
const char *tmp_path(int slot, const char *name);

/* Write a whole file for test setup; returns 0 on success. */
int write_text(const char *path, const char *text);

#endif
