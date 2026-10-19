/*
 * Tests for source/installed.c: the patch record (reclawed.txt), release
 * ordering and the classification of the code.ips on the SD.
 */
#include "installed.h"
#include "manifest.h"
#include "test.h"

static const char IPS[] = "PATCH\x00\x00\x10\x00\x02" "ab" "EOF";

static void format_and_parse(void)
{
	char text[INSTALLED_MAX_SIZE], hex[SHA256_HEX_LEN + 1];
	InstalledRecord r;
	sha256_hex(IPS, sizeof(IPS) - 1, hex);

	int n = installed_format(text, sizeof(text), "v0.4.0", CODE_UPDATE, 5136, IPS, sizeof(IPS) - 1);
	CHECK(n > 0 && (size_t)n == strlen(text));
	CHECK(strstr(text, "reclawed-installed 1 v0.4.0 u5136 ") != NULL);
	CHECK(installed_parse(text, (size_t)n, &r));
	CHECK_STR(r.release, "v0.4.0");
	CHECK(r.code == CODE_UPDATE && r.version == 5136);
	CHECK_STR(r.sha256, hex);

	n = installed_format(text, sizeof(text), "v1.0.0-beta", CODE_BASE, 4096, IPS, sizeof(IPS) - 1);
	CHECK(n > 0 && installed_parse(text, (size_t)n, &r));
	CHECK(r.code == CODE_BASE && r.version == 4096);

	/* Refused: no or odd release names, unknown code, too small a buffer. */
	CHECK(installed_format(text, sizeof(text), "", CODE_UPDATE, 5136, IPS, 3) == -1);
	CHECK(installed_format(text, sizeof(text), "v0 4", CODE_UPDATE, 5136, IPS, 3) == -1);
	CHECK(installed_format(text, sizeof(text), "v0.4.0", 'x', 5136, IPS, 3) == -1);
	CHECK(installed_format(text, sizeof(text), "v0.4.0", CODE_UPDATE, 70000, IPS, 3) == -1);
	CHECK(installed_format(text, 40, "v0.4.0", CODE_UPDATE, 5136, IPS, 3) == -1);

	/* CRLF line ends, comments and blank lines are fine. */
	char crlf[256];
	snprintf(crlf, sizeof(crlf), "# comment\r\n\r\nreclawed-installed 1 v0.4.0 b4096 %s\r\n", hex);
	CHECK(installed_parse(crlf, strlen(crlf), &r) && r.code == CODE_BASE);
}

static void parse_rejects(void)
{
	char hex[SHA256_HEX_LEN + 1], line[256];
	InstalledRecord r;
	sha256_hex(IPS, sizeof(IPS) - 1, hex);
	const char *bad[] = {
		"reclawed-installed 2 v0.4.0 u5136 %s",     /* future format */
		"reclawed-patches 1 v0.4.0 u5136 %s",       /* another file */
		"reclawed-installed 1 v0.4.0 x5136 %s",     /* unknown code */
		"reclawed-installed 1 v0.4.0 u %s",         /* no version */
		"reclawed-installed 1 v0.4.0 u5136x %s",    /* junk after the version */
		"reclawed-installed 1 v0.4.0 u70000 %s",    /* version > 16 bits */
		"reclawed-installed 1 v0.4.0 u5136 %s 1",   /* extra field */
		"reclawed-installed 1 v0.4.0 u5136",        /* missing hash */
	};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
	{
		snprintf(line, sizeof(line), bad[i], hex);
		CHECK(!installed_parse(line, strlen(line), &r));
	}
	/* Short or upper-case hashes. */
	snprintf(line, sizeof(line), "reclawed-installed 1 v0.4.0 u5136 %.63s", hex);
	CHECK(!installed_parse(line, strlen(line), &r));
	snprintf(line, sizeof(line), "reclawed-installed 1 v0.4.0 u5136 %s", hex);
	for (char *c = line + strlen(line) - SHA256_HEX_LEN; *c; c++)
		if (*c >= 'a' && *c <= 'f')
			*c = (char)(*c - 'a' + 'A');
	CHECK(!installed_parse(line, strlen(line), &r));
	/* Empty, only comments, too big. */
	CHECK(!installed_parse("", 0, &r));
	CHECK(!installed_parse("# nothing\n", 10, &r));
	CHECK(!installed_parse(line, INSTALLED_MAX_SIZE + 1, &r));
}

static void releases(void)
{
	CHECK(release_compare("v0.4.0", "v0.5.0") == RELEASE_OLDER);
	CHECK(release_compare("v0.5.0", "v0.4.0") == RELEASE_NEWER);
	CHECK(release_compare("v0.4.0", "v0.4.0") == RELEASE_SAME);
	CHECK(release_compare("v0.10.0", "v0.9.9") == RELEASE_NEWER);  /* numbers, not text */
	CHECK(release_compare("0.4", "v0.4.0") == RELEASE_SAME);       /* missing parts are 0 */
	CHECK(release_compare("v1", "v0.9") == RELEASE_NEWER);
	CHECK(release_compare("v0.5.0-beta", "v0.5.0") == RELEASE_OLDER);
	CHECK(release_compare("v0.5.0", "v0.5.0-rc1") == RELEASE_NEWER);
	CHECK(release_compare("v0.5.0-beta", "v0.5.0-beta2") == RELEASE_OLDER);
	CHECK(release_compare("v0.5.0-beta", "v0.4.9") == RELEASE_NEWER);
	CHECK(release_compare("dev", "v0.4.0") == RELEASE_UNKNOWN);
	CHECK(release_compare("v0.4.0", "") == RELEASE_UNKNOWN);
	CHECK(release_compare("v0.4.0x", "v0.4.0") == RELEASE_UNKNOWN);
	CHECK(release_compare("v0.4.0-", "v0.4.0") == RELEASE_UNKNOWN);
}

static void classify(void)
{
	char hex[SHA256_HEX_LEN + 1], other[SHA256_HEX_LEN + 1];
	sha256_hex(IPS, sizeof(IPS) - 1, hex);
	sha256_hex("x", 1, other);
	InstalledRecord rec = {"v0.4.0", CODE_UPDATE, 5136, ""};
	memcpy(rec.sha256, hex, sizeof(hex));

	CHECK(installed_classify(false, false, &rec, hex, "v0.5.0", 'u', 5136) == PATCH_NOT_ON_SD);
	/* Byte-identical wins, record or not (e.g. a manual copy of the same release). */
	CHECK(installed_classify(true, true, NULL, hex, "v0.5.0", 'u', 5136) == PATCH_SAME);
	CHECK(installed_classify(true, false, &rec, hex, "v0.5.0", 'u', 5136) == PATCH_OLDER);
	CHECK(installed_classify(true, false, &rec, hex, "v0.3.0", 'u', 5136) == PATCH_NEWER);
	CHECK(installed_classify(true, false, &rec, hex, "v0.4.0", 'u', 5136) == PATCH_OTHER_RELEASE);
	CHECK(installed_classify(true, false, &rec, hex, "dev", 'u', 5136) == PATCH_OTHER_RELEASE);
	CHECK(installed_classify(true, false, &rec, hex, "", 'u', 5136) == PATCH_OTHER_RELEASE);
	/* Made for the base game, but the update is installed now. */
	CHECK(installed_classify(true, false, &rec, hex, "v0.5.0", 'b', 4096) == PATCH_OTHER_VERSION);
	/* No record, or a record that describes another file: not ours. */
	CHECK(installed_classify(true, false, NULL, hex, "v0.5.0", 'u', 5136) == PATCH_FOREIGN);
	CHECK(installed_classify(true, false, &rec, other, "v0.5.0", 'u', 5136) == PATCH_FOREIGN);
	CHECK(installed_classify(true, false, &rec, "", "v0.5.0", 'u', 5136) == PATCH_FOREIGN);
}

void test_installed(void)
{
	format_and_parse();
	parse_rejects();
	releases();
	classify();
}
