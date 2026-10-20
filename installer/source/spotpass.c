/*
 * spotpass - see spotpass.h.
 */
#include "spotpass.h"

#include <stdio.h>
#include <string.h>

#include "bytes.h"

/* SARC (little-endian, as Badge Arcade writes it):
 *   0x00 "SARC", 0x04 header size 0x14, 0x06 BOM FF FE, 0x08 file size,
 *   0x0C data offset, 0x10 version
 *   0x14 "SFAT", 0x18 header size 0x0C, 0x1A node count, 0x1C hash key
 *   0x20 nodes, 16 bytes each: name hash, attributes (0x01000000 | name
 *        offset / 4), data start, data end (relative to the data offset)
 *   then "SFNT", 0x08, 0, and the zero-terminated names, 4-byte aligned. */

bool sarc_header_size(const unsigned char *b, size_t n, uint32_t *header_size)
{
	if (!b || n < SARC_HEADER_MIN || memcmp(b, "SARC", 4) != 0 || le16(b + 4) != 0x14 ||
	    b[6] != 0xFF || b[7] != 0xFE)
		return false;
	uint32_t data = le32(b + 0x0C);
	if (data < 0x28 || data > SARC_HEADER_MAX || data > le32(b + 0x08))
		return false;
	*header_size = data;
	return true;
}

bool sarc_find(const unsigned char *hdr, size_t n, const char *name, uint32_t *start,
               uint32_t *end)
{
	uint32_t data;
	if (!sarc_header_size(hdr, n, &data) || n < data || memcmp(hdr + 0x14, "SFAT", 4) != 0 ||
	    le16(hdr + 0x18) != 0x0C)
		return false;
	uint32_t count = le16(hdr + 0x1A);
	size_t names = 0x20 + 16 * (size_t)count;
	if (names + 8 > data || memcmp(hdr + names, "SFNT", 4) != 0)
		return false;
	names += 8;

	size_t want = strlen(name);
	for (uint32_t i = 0; i < count; i++)
	{
		const unsigned char *node = hdr + 0x20 + 16 * (size_t)i;
		uint32_t attr = le32(node + 4);
		if (!(attr & 0x01000000))
			continue; /* member without a name */
		size_t at = names + (size_t)(attr & 0xFFFFFF) * 4;
		if (at + want + 1 > data || memcmp(hdr + at, name, want) != 0 || hdr[at + want] != '\0')
			continue;
		uint32_t s = le32(node + 8), e = le32(node + 12);
		if (s > e || e > 0xFFFFFFFFu - data)
			return false;
		*start = data + s;
		*end = data + e;
		return true;
	}
	return false;
}

/* ---- dates ----------------------------------------------------------- */

/* Returns true if `y` is a leap year (Gregorian). */
static bool leap(int y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

/* Returns the number of days of month `m` (1-12) of year `y`. */
static int month_days(int y, int m)
{
	static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	return m == 2 && leap(y) ? 29 : days[m - 1];
}

bool date_parse(const char *s, size_t len, Date *out)
{
	if (len != 8)
		return false;
	int v[8];
	for (int i = 0; i < 8; i++)
	{
		if (s[i] < '0' || s[i] > '9')
			return false;
		v[i] = s[i] - '0';
	}
	Date d = {v[0] * 1000 + v[1] * 100 + v[2] * 10 + v[3], v[4] * 10 + v[5], v[6] * 10 + v[7]};
	if (d.year < 2000 || d.year > 2099 || d.month < 1 || d.month > 12 || d.day < 1 ||
	    d.day > month_days(d.year, d.month))
		return false;
	*out = d;
	return true;
}

long date_days(Date d)
{
	long n = 0;
	for (int y = 2000; y < d.year; y++)
		n += leap(y) ? 366 : 365;
	for (int m = 1; m < d.month; m++)
		n += month_days(d.year, m);
	return n + d.day - 1;
}

uint64_t date_ms(Date d)
{
	return (uint64_t)date_days(d) * 86400000ull;
}

/* ---- Schedule.xml ---------------------------------------------------- */

/* memmem() for a bounded, not zero-terminated buffer. */
static const char *find(const char *from, const char *to, const char *what)
{
	size_t n = strlen(what);
	for (const char *p = from; p + n <= to; p++)
		if (memcmp(p, what, n) == 0)
			return p;
	return NULL;
}

/* The text of <tag>...</tag> inside [from, to), surrounding blanks trimmed. */
static bool element(const char *from, const char *to, const char *tag, const char **text,
                    size_t *len)
{
	char open[40], close[40];
	snprintf(open, sizeof(open), "<%s>", tag);
	snprintf(close, sizeof(close), "</%s>", tag);
	const char *a = find(from, to, open);
	if (!a)
		return false;
	a += strlen(open);
	const char *b = find(a, to, close);
	if (!b)
		return false;
	while (a < b && (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n'))
		a++;
	while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r' || b[-1] == '\n'))
		b--;
	*text = a;
	*len = (size_t)(b - a);
	return true;
}

/* Returns true if the `len` characters at `text` are exactly `what`. */
static bool is(const char *text, size_t len, const char *what)
{
	return len == strlen(what) && memcmp(text, what, len) == 0;
}

/* Start date of the FileItem in [from, to), if it is of set `set`. */
static bool item_start(const char *from, const char *to, const char *set, Date *start)
{
	const char *t;
	size_t n;
	return element(from, to, "RegexSetName", &t, &n) && is(t, n, set) &&
	       element(from, to, "DateStartText", &t, &n) && date_parse(t, n, start);
}

void schedule_week(const char *xml, size_t len, SpotPassWeek *out)
{
	memset(out, 0, sizeof(*out));
	out->status = SPOTPASS_NO_MACHINES;
	const char *end = xml + len;

	/* Pass 1: the week's first day. */
	bool have_week = false, have_stage = false;
	Date week = {0, 0, 0}, earliest = {0, 0, 0};
	for (const char *p = find(xml, end, "<FileItem>"); p;)
	{
		const char *q = find(p, end, "</FileItem>");
		if (!q)
			break;
		Date d;
		if (!have_week && item_start(p, q, "PrizeCollection", &d))
		{
			week = d;
			have_week = true;
		}
		else if (item_start(p, q, "DefaultStage", &d) &&
		         (!have_stage || date_days(d) < date_days(earliest)))
		{
			earliest = d;
			have_stage = true;
		}
		p = find(q, end, "<FileItem>");
	}
	if (!have_week && !have_stage)
		return;
	if (!have_week)
		week = earliest;

	/* Pass 2: the machines the clock fold can reach (7 days from it). */
	bool day_used[7] = {false};
	long first = date_days(week);
	for (const char *p = find(xml, end, "<FileItem>"); p;)
	{
		const char *q = find(p, end, "</FileItem>");
		if (!q)
			break;
		Date d;
		if (item_start(p, q, "DefaultStage", &d))
		{
			long day = date_days(d) - first;
			if (day >= 0 && day < 7)
			{
				out->machines++;
				day_used[day] = true;
			}
		}
		p = find(q, end, "<FileItem>");
	}
	for (int i = 0; i < 7; i++)
		out->days += day_used[i];
	if (out->machines > 0)
	{
		out->status = SPOTPASS_OK;
		out->first_day = week;
	}
}

const char *spotpass_status_str(SpotPassStatus s)
{
	switch (s)
	{
	case SPOTPASS_OK: return "OK";
	case SPOTPASS_MISSING: return "no boss.sarc";
	case SPOTPASS_UNREADABLE: return "boss.sarc unreadable";
	case SPOTPASS_NOT_SARC: return "boss.sarc damaged";
	case SPOTPASS_NO_SCHEDULE: return "boss.sarc: no schedule";
	case SPOTPASS_NO_MACHINES: return "boss.sarc: no machines";
	}
	return "unknown";
}
