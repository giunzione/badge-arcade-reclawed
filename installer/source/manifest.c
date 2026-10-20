/*
 * manifest - see manifest.h.
 */
#include "manifest.h"

#include <stdlib.h>
#include <string.h>

/* Split a line into whitespace-separated fields, in place. Returns the count. */
static int split(char *line, char **fields, int max)
{
	int n = 0;
	char *p = line;
	while (*p)
	{
		while (*p == ' ' || *p == '\t')
			*p++ = '\0';
		if (!*p)
			break;
		if (n == max)
			return max + 1; /* too many fields */
		fields[n++] = p;
		while (*p && *p != ' ' && *p != '\t')
			p++;
	}
	return n;
}

/* Returns true if `s` is not empty and made only of characters in `allowed`. */
static bool all_chars(const char *s, const char *allowed)
{
	return *s && strspn(s, allowed) == strlen(s);
}

/* Unsigned decimal of at most 9 digits. */
static bool parse_uint(const char *s, unsigned long *out)
{
	if (!all_chars(s, "0123456789") || strlen(s) > 9)
		return false;
	*out = strtoul(s, NULL, 10);
	return true;
}

/* Parses the six fields `f` of a patch line into `e`. Returns false if any of
 * them is malformed. */
static bool parse_entry(char **f, PatchEntry *e)
{
	/* base title ID */
	if (strlen(f[0]) != 16 || !all_chars(f[0], "0123456789abcdefABCDEF"))
		return false;
	e->base_tid = strtoull(f[0], NULL, 16);

	/* code: "u5136" / "b4096" */
	unsigned long v;
	if ((f[1][0] != CODE_UPDATE && f[1][0] != CODE_BASE) || !parse_uint(f[1] + 1, &v) || v > 0xFFFF)
		return false;
	e->code = f[1][0];
	e->version = (unsigned)v;

	/* region: upper-case letters */
	if (strlen(f[2]) >= sizeof(e->region) || !all_chars(f[2], "ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
		return false;
	strcpy(e->region, f[2]);

	/* file name: no path separators or anything a URL would need to escape */
	if (strlen(f[3]) >= sizeof(e->file) || f[3][0] == '.' ||
	    !all_chars(f[3], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-"))
		return false;
	strcpy(e->file, f[3]);

	/* size and hash */
	if (!parse_uint(f[4], &v) || v == 0 || v > MANIFEST_MAX_PATCH_SIZE)
		return false;
	e->size = (size_t)v;
	if (strlen(f[5]) != SHA256_HEX_LEN || !all_chars(f[5], "0123456789abcdef"))
		return false;
	strcpy(e->sha256, f[5]);
	return true;
}

ManifestStatus manifest_parse(const char *text, size_t len, Manifest *m)
{
	memset(m, 0, sizeof(*m));
	bool header = false;
	ManifestStatus st = MANIFEST_OK;

	size_t pos = 0;
	while (pos < len && st == MANIFEST_OK)
	{
		size_t start = pos, eol = pos;
		while (eol < len && text[eol] != '\n')
			eol++;
		char line[256];
		size_t n = eol - start;
		pos = eol + 1;
		if (n >= sizeof(line))
		{
			st = header ? MANIFEST_ERR_LINE : MANIFEST_ERR_HEADER;
			break;
		}
		memcpy(line, text + start, n);
		line[n] = '\0';
		if (n && line[n - 1] == '\r')
			line[n - 1] = '\0';

		char *f[7];
		int count = split(line, f, 6);
		if (count == 0 || f[0][0] == '#')
			continue;

		if (!header)
		{
			unsigned long format;
			if (count != 3 || strcmp(f[0], "reclawed-patches") != 0 || !parse_uint(f[1], &format) ||
			    strlen(f[2]) >= sizeof(m->release))
				st = MANIFEST_ERR_HEADER;
			else if (format != MANIFEST_FORMAT)
				st = MANIFEST_ERR_FORMAT;
			else
			{
				strcpy(m->release, f[2]);
				header = true;
			}
			continue;
		}

		if (count != 6)
			st = MANIFEST_ERR_LINE;
		else if (m->count == MANIFEST_MAX_ENTRIES)
			st = MANIFEST_ERR_TOO_MANY;
		else if (!parse_entry(f, &m->entries[m->count]))
			st = MANIFEST_ERR_LINE;
		else
			m->count++;
	}

	if (st == MANIFEST_OK && !header)
		st = MANIFEST_ERR_HEADER;
	if (st != MANIFEST_OK)
		m->count = 0;
	return st;
}

const PatchEntry *manifest_find(const Manifest *m, uint64_t base_tid, char code, unsigned version)
{
	if (!m)
		return NULL;
	for (int i = 0; i < m->count; i++)
	{
		const PatchEntry *e = &m->entries[i];
		if (e->base_tid == base_tid && e->code == code && e->version == version)
			return e;
	}
	return NULL;
}

bool manifest_entry_matches(const PatchEntry *e, const void *data, size_t size)
{
	if (!e || !data || size != e->size)
		return false;
	char hex[SHA256_HEX_LEN + 1];
	sha256_hex(data, size, hex);
	return strcmp(hex, e->sha256) == 0;
}

PatchSource manifest_choose(const Manifest *online, bool online_file_ok, const Manifest *builtin,
                            uint64_t base_tid, char code, unsigned version, const PatchEntry **out)
{
	const PatchEntry *e = manifest_find(online, base_tid, code, version);
	if (e && online_file_ok)
	{
		*out = e;
		return SOURCE_ONLINE;
	}
	e = manifest_find(builtin, base_tid, code, version);
	*out = e;
	return e ? SOURCE_BUILTIN : SOURCE_NONE;
}
