/*
 * Tests for source/spotpass.c: SARC lookup, Schedule.xml week, dates.
 * The archive and schedule are built here; no game files are used.
 */
#include <stdlib.h>

#include "spotpass.h"
#include "test.h"

/* Writes `v` at `p` as a little-endian 16-bit value. */
static void put16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
}

/* Writes `v` at `p` as a little-endian 32-bit value (the test archives are
 * small: every offset fits). */
static void put32(unsigned char *p, size_t v)
{
	put16(p, (unsigned)(v & 0xFFFF));
	put16(p + 2, (unsigned)((v >> 16) & 0xFFFF));
}

/* A little-endian SARC with the given members, laid out as the game's:
 * header, SFAT nodes, SFNT names (4-aligned), data from a 0x80 boundary.
 * Returns its size; *data_offset receives the header size. */
static size_t make_sarc(unsigned char *out, size_t cap, int count, const char *const names[],
                        const char *const data[], unsigned *data_offset)
{
	memset(out, 0, cap);
	size_t names_at = 0x20 + 16 * (size_t)count + 8, p = names_at;
	size_t name_off[8];
	for (int i = 0; i < count; i++)
	{
		name_off[i] = (p - names_at) / 4;
		strcpy((char *)out + p, names[i]);
		p += strlen(names[i]) + 1;
		p = (p + 3) & ~(size_t)3;
	}
	size_t doff = (p + 0x7F) & ~(size_t)0x7F, d = doff;
	for (int i = 0; i < count; i++)
	{
		unsigned char *node = out + 0x20 + 16 * i;
		put32(node, 0x1234 + (unsigned long)i); /* hash: not checked */
		put32(node + 4, 0x01000000ul | name_off[i]);
		put32(node + 8, d - doff);
		memcpy(out + d, data[i], strlen(data[i]));
		d += strlen(data[i]);
		put32(node + 12, d - doff);
		d = (d + 0x7F) & ~(size_t)0x7F;
	}
	memcpy(out, "SARC", 4);
	put16(out + 4, 0x14);
	out[6] = 0xFF;
	out[7] = 0xFE;
	put32(out + 8, d);
	put32(out + 0x0C, doff);
	put32(out + 0x10, 0x0100);
	memcpy(out + 0x14, "SFAT", 4);
	put16(out + 0x18, 0x0C);
	put16(out + 0x1A, (unsigned)count);
	put32(out + 0x1C, 0x65);
	memcpy(out + names_at - 8, "SFNT", 4);
	put16(out + names_at - 4, 8);
	*data_offset = (unsigned)doff;
	return d;
}

static void sarc(void)
{
	static unsigned char buf[2048];
	const char *names[] = {"post/a.sarc", "Schedule.xml", "sharc/x.sarc"};
	const char *data[] = {"AAAA", "<FileItems/>", "BB"};
	unsigned doff;
	size_t n = make_sarc(buf, sizeof(buf), 3, names, data, &doff);

	uint32_t hdr, s, e;
	CHECK(sarc_header_size(buf, SARC_HEADER_MIN, &hdr) && hdr == doff);
	CHECK(sarc_find(buf, hdr, "Schedule.xml", &s, &e));
	CHECK(e - s == strlen(data[1]) && memcmp(buf + s, data[1], e - s) == 0);
	CHECK(sarc_find(buf, hdr, "sharc/x.sarc", &s, &e) && e - s == 2 && e <= n);
	CHECK(!sarc_find(buf, hdr, "Schedule", &s, &e));      /* prefix only */
	CHECK(!sarc_find(buf, hdr, "Schedule.xml2", &s, &e));
	CHECK(!sarc_find(buf, hdr - 1, "Schedule.xml", &s, &e)); /* header cut short */

	/* Not a little-endian SARC. */
	unsigned char bad[SARC_HEADER_MIN];
	memcpy(bad, buf, sizeof(bad));
	bad[6] = 0xFE;
	bad[7] = 0xFF;
	CHECK(!sarc_header_size(bad, sizeof(bad), &hdr));
	memcpy(bad, buf, sizeof(bad));
	bad[0] = 'Y';
	CHECK(!sarc_header_size(bad, sizeof(bad), &hdr));
	CHECK(!sarc_header_size(buf, SARC_HEADER_MIN - 1, &hdr));
	memcpy(bad, buf, sizeof(bad));
	put32(bad + 0x0C, SARC_HEADER_MAX + 4); /* implausible header size */
	put32(bad + 0x08, SARC_HEADER_MAX + 8);
	CHECK(!sarc_header_size(bad, sizeof(bad), &hdr));
}

static void dates(void)
{
	Date d;
	CHECK(date_parse("20180713", 8, &d) && d.year == 2018 && d.month == 7 && d.day == 13);
	CHECK(date_parse("20000101", 8, &d) && date_days(d) == 0 && date_ms(d) == 0);
	CHECK(date_parse("20000301", 8, &d) && date_days(d) == 60); /* 2000 is a leap year */
	CHECK(date_parse("20180713", 8, &d) && date_ms(d) == 584755200000ull);
	CHECK(date_parse("20190110", 8, &d) && date_ms(d) == 600393600000ull);
	CHECK(date_parse("20240229", 8, &d));
	CHECK(!date_parse("20230229", 8, &d));
	CHECK(!date_parse("20181301", 8, &d));
	CHECK(!date_parse("20180700", 8, &d));
	CHECK(!date_parse("19991231", 8, &d)); /* before the 3DS epoch */
	CHECK(!date_parse("2018071", 7, &d));
	CHECK(!date_parse("2018-7-1", 8, &d));
}

/* One FileItem of a schedule. */
static void item(char *xml, size_t cap, const char *set, const char *start, const char *expire)
{
	size_t n = strlen(xml);
	snprintf(xml + n, cap - n,
	         "    <FileItem>\r\n      <DateStartText>%s</DateStartText>\r\n"
	         "      <DateExpireText>%s</DateExpireText>\r\n      <Comment />\r\n"
	         "      <RegexSetName>%s</RegexSetName>\r\n      <Key>k</Key>\r\n"
	         "      <Value>v</Value>\r\n    </FileItem>\r\n",
	         start, expire, set);
}

static void week(void)
{
	static char xml[16384];
	SpotPassWeek w;

	/* A full week: 3 machines a day, a message dir and the week's catalog. */
	snprintf(xml, sizeof(xml), "<?xml version=\"1.0\"?>\r\n<FileItems>\r\n  <Items>\r\n");
	item(xml, sizeof(xml), "MessageDir", "20180713", "20180720");
	for (int day = 13; day < 20; day++)
	{
		char a[24], b[24]; /* room for any int: gcc's -Wformat-truncation can't see day < 20 */
		snprintf(a, sizeof(a), "201807%02d", day);
		snprintf(b, sizeof(b), "201807%02d", day + 1);
		for (int k = 0; k < 3; k++)
			item(xml, sizeof(xml), "DefaultStage", a, b);
	}
	item(xml, sizeof(xml), "PrizeCollection", "20180713", "20180720");
	strcat(xml, "  </Items>\r\n</FileItems>\r\n");
	schedule_week(xml, strlen(xml), &w);
	CHECK(w.status == SPOTPASS_OK && w.days == 7 && w.machines == 21);
	CHECK(w.first_day.year == 2018 && w.first_day.month == 7 && w.first_day.day == 13);

	/* No PrizeCollection: the earliest machine day, even out of order. */
	xml[0] = '\0';
	item(xml, sizeof(xml), "DefaultStage", "20190112", "20190113");
	item(xml, sizeof(xml), "DefaultStage", "20190110", "20190111");
	item(xml, sizeof(xml), "BonusStage", "20190101", "20190102"); /* not a machine of the week */
	schedule_week(xml, strlen(xml), &w);
	CHECK(w.status == SPOTPASS_OK && w.days == 2 && w.machines == 2);
	CHECK(w.first_day.year == 2019 && w.first_day.month == 1 && w.first_day.day == 10);

	/* Machines outside the 7 days from the week's start don't count. */
	xml[0] = '\0';
	item(xml, sizeof(xml), "PrizeCollection", "20180713", "20180720");
	item(xml, sizeof(xml), "DefaultStage", "20180712", "20180713");
	item(xml, sizeof(xml), "DefaultStage", "20180720", "20180721");
	item(xml, sizeof(xml), "DefaultStage", "20180719", "20180720");
	schedule_week(xml, strlen(xml), &w);
	CHECK(w.status == SPOTPASS_OK && w.days == 1 && w.machines == 1);

	/* A dated catalog but no machines in its week. */
	xml[0] = '\0';
	item(xml, sizeof(xml), "PrizeCollection", "20180713", "20180720");
	schedule_week(xml, strlen(xml), &w);
	CHECK(w.status == SPOTPASS_NO_MACHINES && w.machines == 0);

	/* Bad dates and unrelated XML. */
	xml[0] = '\0';
	item(xml, sizeof(xml), "DefaultStage", "2018-07-13", "20180714");
	schedule_week(xml, strlen(xml), &w);
	CHECK(w.status == SPOTPASS_NO_MACHINES);
	schedule_week("<FileItems><Items/></FileItems>", 31, &w);
	CHECK(w.status == SPOTPASS_NO_MACHINES);
	schedule_week("", 0, &w);
	CHECK(w.status == SPOTPASS_NO_MACHINES);

	/* An item cut off at the end of the buffer is ignored, not read past. */
	xml[0] = '\0';
	item(xml, sizeof(xml), "DefaultStage", "20180713", "20180714");
	size_t whole = strlen(xml);
	item(xml, sizeof(xml), "DefaultStage", "20180714", "20180715");
	schedule_week(xml, whole + 40, &w);
	CHECK(w.status == SPOTPASS_OK && w.machines == 1);
}

static void status_text(void)
{
	/* "<reason>: built-in machines" must fit one 42-character check line. */
	for (int s = SPOTPASS_OK; s <= SPOTPASS_NO_MACHINES; s++)
		CHECK(strlen(spotpass_status_str((SpotPassStatus)s)) <= 22);
}

void test_spotpass(void)
{
	sarc();
	dates();
	week();
	status_text();
}
