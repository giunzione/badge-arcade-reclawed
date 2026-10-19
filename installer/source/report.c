/*
 * report - see report.h.
 */
#include "report.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int report_url_encode(const char *in, char *out, size_t len)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t o = 0;
	for (const unsigned char *p = (const unsigned char *)in; *p; p++)
	{
		bool plain = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
		             (*p >= '0' && *p <= '9') || *p == '-' || *p == '.' || *p == '_' || *p == '~';
		size_t need = plain ? 1 : 3;
		if (o + need >= len)
			return -1;
		if (plain)
			out[o++] = (char)*p;
		else
		{
			out[o++] = '%';
			out[o++] = hex[*p >> 4];
			out[o++] = hex[*p & 0xF];
		}
	}
	if (o >= len)
		return -1;
	out[o] = '\0';
	return 0;
}

/* Append "&name=<encoded value>" to the URL being built. */
static int add_param(char *out, size_t len, const char *name, const char *value)
{
	char enc[96];
	if (report_url_encode(value, enc, sizeof(enc)) != 0)
		return -1;
	size_t used = strlen(out);
	int n = snprintf(out + used, len - used, "&%s=%s", name, enc);
	return n < 0 || (size_t)n >= len - used ? -1 : 0;
}

int report_issue_url(char *out, size_t len, const char *region, uint64_t base_tid, char code,
                     unsigned version, const char *app_version, const char *luma)
{
	char tid[17], ver[12], title[48];
	snprintf(tid, sizeof(tid), "%016llX", (unsigned long long)base_tid);
	snprintf(ver, sizeof(ver), "%c%u", code, version);
	snprintf(title, sizeof(title), "Patch request: %s %s", region, ver);

	int n = snprintf(out, len, REPORT_REPO_URL "/issues/new?template=game-version.yml");
	if (n < 0 || (size_t)n >= len)
		return -1;
	if (add_param(out, len, "title", title) || add_param(out, len, "region", region) ||
	    add_param(out, len, "title_id", tid) || add_param(out, len, "game_version", ver) ||
	    add_param(out, len, "app_version", app_version) ||
	    add_param(out, len, "luma_version", luma ? luma : "not detected"))
		return -1;
	return 0;
}
