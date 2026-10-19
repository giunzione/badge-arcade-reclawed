/*
 * installed - see installed.h.
 */
#include "installed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "manifest.h"

#define HEADER "reclawed-installed"

/* A release tag must be one printable word that fits the record. */
static bool valid_release(const char *r)
{
	size_t n = strlen(r);
	if (n == 0 || n >= sizeof(((InstalledRecord *)0)->release))
		return false;
	for (size_t i = 0; i < n; i++)
		if (r[i] <= ' ' || r[i] > '~')
			return false;
	return true;
}

int installed_format(char *out, size_t len, const char *release, char code, unsigned version,
                     const void *ips, size_t size)
{
	if (!release || !valid_release(release) || (code != CODE_UPDATE && code != CODE_BASE) ||
	    version > 0xFFFF)
		return -1;
	char hex[SHA256_HEX_LEN + 1];
	sha256_hex(ips, size, hex);
	int n = snprintf(out, len,
	                 "# Badge Arcade Reclawed: written by the installer app, do not edit.\n"
	                 HEADER " %d %s %c%u %s\n",
	                 INSTALLED_FORMAT, release, code, version, hex);
	return n > 0 && (size_t)n < len ? n : -1;
}

bool installed_parse(const char *text, size_t len, InstalledRecord *out)
{
	memset(out, 0, sizeof(*out));
	if (!text || len == 0 || len > INSTALLED_MAX_SIZE)
		return false;
	char buf[INSTALLED_MAX_SIZE + 1];
	memcpy(buf, text, len);
	buf[len] = '\0';

	/* The first line that isn't empty or a comment is the record. */
	for (char *line = strtok(buf, "\r\n"); line; line = strtok(NULL, "\r\n"))
	{
		while (*line == ' ' || *line == '\t')
			line++;
		if (*line == '\0' || *line == '#')
			continue;

		char header[24], release[40], code_ver[12], hash[80], extra[2];
		int format;
		if (sscanf(line, "%23s %d %39s %11s %79s %1s", header, &format, release, code_ver, hash,
		           extra) != 5)
			return false;
		if (strcmp(header, HEADER) != 0 || format != INSTALLED_FORMAT || !valid_release(release))
			return false;

		/* "u5136" / "b4096" */
		char code = code_ver[0];
		char *end;
		unsigned long version = strtoul(code_ver + 1, &end, 10);
		if ((code != CODE_UPDATE && code != CODE_BASE) || end == code_ver + 1 || *end ||
		    version > 0xFFFF)
			return false;

		if (strlen(hash) != SHA256_HEX_LEN)
			return false;
		for (int i = 0; i < SHA256_HEX_LEN; i++)
			if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f')))
				return false;

		snprintf(out->release, sizeof(out->release), "%s", release);
		out->code = code;
		out->version = (unsigned)version;
		memcpy(out->sha256, hash, SHA256_HEX_LEN + 1);
		return true;
	}
	return false;
}

/* "[v]X[.Y[.Z]][-suffix]" -> numbers and suffix ("" if none). */
static bool parse_release(const char *s, unsigned long num[3], const char **suffix)
{
	if (*s == 'v' || *s == 'V')
		s++;
	num[0] = num[1] = num[2] = 0;
	for (int i = 0; i < 3; i++)
	{
		if (*s < '0' || *s > '9')
			return false;
		char *end;
		num[i] = strtoul(s, &end, 10);
		s = end;
		if (*s != '.')
			break;
		s++;
	}
	if (*s == '\0')
		*suffix = "";
	else if (*s == '-' && s[1])
		*suffix = s + 1;
	else
		return false;
	return true;
}

ReleaseOrder release_compare(const char *a, const char *b)
{
	unsigned long na[3], nb[3];
	const char *sa, *sb;
	if (!a || !b || !parse_release(a, na, &sa) || !parse_release(b, nb, &sb))
		return RELEASE_UNKNOWN;
	for (int i = 0; i < 3; i++)
		if (na[i] != nb[i])
			return na[i] < nb[i] ? RELEASE_OLDER : RELEASE_NEWER;
	/* Same numbers: a pre-release ("-beta") comes before the release. */
	if (!*sa != !*sb)
		return *sa ? RELEASE_OLDER : RELEASE_NEWER;
	int c = strcmp(sa, sb);
	return c < 0 ? RELEASE_OLDER : c > 0 ? RELEASE_NEWER : RELEASE_SAME;
}

PatchState installed_classify(bool on_sd, bool same_as_chosen, const InstalledRecord *rec,
                              const char *sd_sha256, const char *chosen_release, char code,
                              unsigned version)
{
	if (!on_sd)
		return PATCH_NOT_ON_SD;
	if (same_as_chosen)
		return PATCH_SAME;
	/* Only trust the record if it describes this very file. */
	if (!rec || !sd_sha256 || strcmp(rec->sha256, sd_sha256) != 0)
		return PATCH_FOREIGN;
	if (rec->code != code || rec->version != version)
		return PATCH_OTHER_VERSION;
	switch (release_compare(rec->release, chosen_release ? chosen_release : ""))
	{
	case RELEASE_OLDER: return PATCH_OLDER;
	case RELEASE_NEWER: return PATCH_NEWER;
	default:            return PATCH_OTHER_RELEASE;
	}
}
