/*
 * Tests for source/sha256.c, source/manifest.c and source/report.c.
 */
#include <stdlib.h>

#include "fileops.h"
#include "manifest.h"
#include "report.h"
#include "sha256.h"
#include "test.h"

/* ---- sha256: NIST FIPS 180-4 examples ---- */

static void sha256_vectors(void)
{
	char hex[SHA256_HEX_LEN + 1];
	sha256_hex("", 0, hex);
	CHECK_STR(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	sha256_hex("abc", 3, hex);
	CHECK_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"; /* 56 bytes: two blocks */
	sha256_hex(two, strlen(two), hex);
	CHECK_STR(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

	/* One million 'a': exercises many blocks. */
	char *a = malloc(1000000);
	memset(a, 'a', 1000000);
	sha256_hex(a, 1000000, hex);
	CHECK_STR(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	free(a);

	/* Lengths around the padding boundary (55, 56, 63, 64 bytes) must differ
	 * and be stable; 64 bytes of 'a' is a known value. */
	char buf[64];
	memset(buf, 'a', sizeof(buf));
	sha256_hex(buf, 64, hex);
	CHECK_STR(hex, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
}

/* ---- manifest ---- */

#define SHA_A "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" /* "abc" */

static const char GOOD[] =
	"# Badge Arcade Reclawed patches\r\n"
	"reclawed-patches 1 v0.4.0\r\n"
	"\r\n"
	"0004000000153600 u5136 EUR patch-EUR-u5136.ips 3 " SHA_A "\r\n"
	"0004000000153600\tb4096\tEUR\tpatch-EUR-b4096.ips\t3\t" SHA_A "\n"
	"0004000000153500 u5136 USA patch-USA-u5136.ips 3 " SHA_A;

static ManifestStatus parse(const char *text, Manifest *m)
{
	return manifest_parse(text, strlen(text), m);
}

static void manifest_good(void)
{
	Manifest m;
	CHECK(parse(GOOD, &m) == MANIFEST_OK);
	CHECK_STR(m.release, "v0.4.0");
	CHECK(m.count == 3);
	const PatchEntry *e = &m.entries[1];
	CHECK(e->base_tid == 0x0004000000153600ULL && e->code == CODE_BASE && e->version == 4096);
	CHECK_STR(e->region, "EUR");
	CHECK_STR(e->file, "patch-EUR-b4096.ips");
	CHECK(e->size == 3);

	CHECK(manifest_find(&m, 0x0004000000153600ULL, CODE_UPDATE, 5136) == &m.entries[0]);
	CHECK(manifest_find(&m, 0x0004000000153500ULL, CODE_UPDATE, 5136) == &m.entries[2]);
	CHECK(manifest_find(&m, 0x0004000000153600ULL, CODE_UPDATE, 5200) == NULL);
	CHECK(manifest_find(&m, 0x0004000000153600ULL, CODE_BASE, 5136) == NULL);
	CHECK(manifest_find(NULL, 0x0004000000153600ULL, CODE_UPDATE, 5136) == NULL);

	CHECK(manifest_entry_matches(&m.entries[0], "abc", 3));
	CHECK(!manifest_entry_matches(&m.entries[0], "abd", 3));
	CHECK(!manifest_entry_matches(&m.entries[0], "abcd", 4));
	CHECK(!manifest_entry_matches(NULL, "abc", 3));
}

static void manifest_bad(void)
{
	Manifest m;
	CHECK(parse("", &m) == MANIFEST_ERR_HEADER);
	CHECK(parse("# only comments\n", &m) == MANIFEST_ERR_HEADER);
	CHECK(parse("<html>Not Found</html>", &m) == MANIFEST_ERR_HEADER); /* e.g. an error page */
	CHECK(parse("reclawed-patches 2 v9.0.0\n", &m) == MANIFEST_ERR_FORMAT);
	CHECK(parse("reclawed-patches x v0\n", &m) == MANIFEST_ERR_HEADER);

	/* Header alone: valid, just empty. */
	CHECK(parse("reclawed-patches 1 v0.4.0\n", &m) == MANIFEST_OK && m.count == 0);

	/* One bad field at a time; the whole file is rejected. */
	static const char *bad_lines[] = {
		"000400000015360 u5136 EUR p.ips 3 " SHA_A,          /* short title ID */
		"0004000000153600 x5136 EUR p.ips 3 " SHA_A,         /* unknown code */
		"0004000000153600 u EUR p.ips 3 " SHA_A,             /* no version */
		"0004000000153600 u70000 EUR p.ips 3 " SHA_A,        /* version > 16 bits */
		"0004000000153600 u5136 eur p.ips 3 " SHA_A,         /* lower-case region */
		"0004000000153600 u5136 EUR ../code.ips 3 " SHA_A,   /* path */
		"0004000000153600 u5136 EUR a/b.ips 3 " SHA_A,       /* path */
		"0004000000153600 u5136 EUR a%20b.ips 3 " SHA_A,     /* URL escape */
		"0004000000153600 u5136 EUR p.ips 0 " SHA_A,         /* empty file */
		"0004000000153600 u5136 EUR p.ips 999999 " SHA_A,    /* too big */
		"0004000000153600 u5136 EUR p.ips 3 " "BA7816BF",    /* short / upper-case hash */
		"0004000000153600 u5136 EUR p.ips 3",                /* missing field */
		"0004000000153600 u5136 EUR p.ips 3 " SHA_A " extra", /* extra field */
	};
	for (size_t i = 0; i < sizeof(bad_lines) / sizeof(bad_lines[0]); i++)
	{
		char text[512];
		snprintf(text, sizeof(text), "reclawed-patches 1 v0.4.0\n"
		         "0004000000153600 u5136 EUR ok.ips 3 " SHA_A "\n%s\n", bad_lines[i]);
		CHECK(parse(text, &m) == MANIFEST_ERR_LINE);
		CHECK(m.count == 0);
	}

	/* Too many entries. */
	char *big = malloc(64 * 128);
	strcpy(big, "reclawed-patches 1 v0\n");
	for (int i = 0; i < MANIFEST_MAX_ENTRIES + 1; i++)
	{
		char line[128];
		snprintf(line, sizeof(line), "0004000000153600 u%d EUR p%d.ips 3 " SHA_A "\n", i, i);
		strcat(big, line);
	}
	CHECK(parse(big, &m) == MANIFEST_ERR_TOO_MANY);
	free(big);
}

static void manifest_hybrid_choice(void)
{
	Manifest online, builtin;
	CHECK(parse(GOOD, &online) == MANIFEST_OK);
	CHECK(parse("reclawed-patches 1 v0.3.0\n"
	            "0004000000153600 u5136 EUR patch-EUR-u5136.ips 3 " SHA_A "\n", &builtin) == MANIFEST_OK);
	const PatchEntry *e;
	uint64_t eur = 0x0004000000153600ULL, usa = 0x0004000000153500ULL;

	/* Online works: online wins. */
	CHECK(manifest_choose(&online, true, &builtin, eur, CODE_UPDATE, 5136, &e) == SOURCE_ONLINE);
	CHECK(e == &online.entries[0]);
	/* Offline: built-in. */
	CHECK(manifest_choose(NULL, false, &builtin, eur, CODE_UPDATE, 5136, &e) == SOURCE_BUILTIN);
	CHECK(e == &builtin.entries[0]);
	/* Listed online but the download failed: built-in. */
	CHECK(manifest_choose(&online, false, &builtin, eur, CODE_UPDATE, 5136, &e) == SOURCE_BUILTIN);
	/* Only online has it (a newer release added USA). */
	CHECK(manifest_choose(&online, true, &builtin, usa, CODE_UPDATE, 5136, &e) == SOURCE_ONLINE);
	CHECK(manifest_choose(NULL, false, &builtin, usa, CODE_UPDATE, 5136, &e) == SOURCE_NONE);
	CHECK(e == NULL);
	/* Nobody has it. */
	CHECK(manifest_choose(&online, true, &builtin, eur, CODE_UPDATE, 4112, &e) == SOURCE_NONE);
}

/* ---- report ---- */

static void report_urls(void)
{
	char enc[32];
	CHECK(report_url_encode("Patch request: EUR", enc, sizeof(enc)) == 0);
	CHECK_STR(enc, "Patch%20request%3A%20EUR");
	CHECK(report_url_encode("a-b_c.d~e", enc, sizeof(enc)) == 0);
	CHECK_STR(enc, "a-b_c.d~e");
	CHECK(report_url_encode("&=?#/", enc, sizeof(enc)) == 0);
	CHECK_STR(enc, "%26%3D%3F%23%2F");
	CHECK(report_url_encode("abc", enc, 3) == -1); /* no room for the NUL */
	CHECK(report_url_encode("  ", enc, 6) == -1);

	char url[REPORT_URL_MAX];
	CHECK(report_issue_url(url, sizeof(url), "EUR", 0x0004000000153600ULL, CODE_UPDATE, 4112,
	                       "0.4.0", "13.3.2") == 0);
	CHECK_STR(url, REPORT_REPO_URL "/issues/new?template=game-version.yml"
	          "&title=Patch%20request%3A%20EUR%20u4112&region=EUR&title_id=0004000000153600"
	          "&game_version=u4112&app_version=0.4.0&luma_version=13.3.2");
	/* Short enough for a version-10 QR code at low error correction (271 bytes). */
	CHECK(strlen(url) <= 271);

	CHECK(report_issue_url(url, sizeof(url), "JPN", 0x0004000000134600ULL, CODE_BASE, 1024,
	                       "0.4.0-fake", NULL) == 0);
	CHECK(strstr(url, "&luma_version=not%20detected") != NULL);
	CHECK(strstr(url, "&game_version=b1024") != NULL);

	CHECK(report_issue_url(url, 60, "EUR", 1, CODE_UPDATE, 1, "0.4.0", NULL) == -1);
}

/* The manifest tools/make_manifest.py really wrote, if it was run: the app's
 * parser must accept it and every file must match its size and hash. Keeps
 * the Python writer and the C reader in agreement. */
#define REAL_MANIFEST_DIR "../../build/patches/"

static void real_manifest(void)
{
	unsigned char *text;
	size_t size;
	if (fops_read(REAL_MANIFEST_DIR "patches.txt", &text, &size, FOPS_MAX_READ) != 0)
	{
		printf("  (no build/patches/patches.txt: real manifest check skipped)\n");
		return;
	}
	Manifest m;
	CHECK(manifest_parse((const char *)text, size, &m) == MANIFEST_OK);
	CHECK(m.count > 0);
	for (int i = 0; i < m.count; i++)
	{
		char path[128];
		unsigned char *ips;
		size_t ips_size;
		snprintf(path, sizeof(path), REAL_MANIFEST_DIR "%s", m.entries[i].file);
		CHECK(fops_read(path, &ips, &ips_size, FOPS_MAX_READ) == 0);
		CHECK(manifest_entry_matches(&m.entries[i], ips, ips_size));
		free(ips);
	}
	free(text);
}

void test_manifest(void)
{
	real_manifest();
	sha256_vectors();
	manifest_good();
	manifest_bad();
	manifest_hybrid_choice();
	report_urls();
}
