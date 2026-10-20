/*
 * spotpass - which week of SpotPass data the console has cached.
 *
 * The game keeps the last downloaded week in its extdata as boss.sarc, a
 * SARC archive. Its Schedule.xml dates every entry, e.g.
 *
 *     <FileItem>
 *       <DateStartText>20180713</DateStartText>
 *       <DateExpireText>20180714</DateExpireText>
 *       <RegexSetName>DefaultStage</RegexSetName>      (one day's machines)
 *       ...
 *     <FileItem> ... <RegexSetName>PrizeCollection</RegexSetName> ...  (the week)
 *
 * The patch folds the clock into that week (docs/04-time.md), so it must
 * know its first day: the PrizeCollection entry's start, or else the
 * earliest DefaultStage start. probe.c reads the file; everything here is
 * plain parsing, unit-tested in tests/test_spotpass.c with synthetic data.
 */
#ifndef SPOTPASS_H
#define SPOTPASS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SPOTPASS_FILE "/boss.sarc"
#define SPOTPASS_SCHEDULE "Schedule.xml"
#define SARC_HEADER_MIN 0x14            /* enough to read the data offset */
#define SARC_HEADER_MAX (256 * 1024)    /* real one: ~2 KB (34 files) */
#define SCHEDULE_MAX_SIZE (1024 * 1024) /* real ones: ~220 KB */

typedef struct
{
	int year, month, day;
} Date;

typedef enum
{
	SPOTPASS_OK,
	SPOTPASS_MISSING,     /* no boss.sarc (or no extdata) */
	SPOTPASS_UNREADABLE,  /* the file can't be read */
	SPOTPASS_NOT_SARC,    /* not an archive this app understands */
	SPOTPASS_NO_SCHEDULE, /* no Schedule.xml inside */
	SPOTPASS_NO_MACHINES, /* Schedule.xml lists no dated machines */
} SpotPassStatus;

typedef struct
{
	SpotPassStatus status;
	Date first_day;  /* when SPOTPASS_OK */
	int days;        /* days of that week with machines (normally 7) */
	int machines;    /* DefaultStage entries in the week */
} SpotPassWeek;

/* Size of the SARC header part (everything before the file data), from its
 * first SARC_HEADER_MIN bytes. False if it isn't a little-endian SARC or the
 * size is not plausible. */
bool sarc_header_size(const unsigned char *b, size_t n, uint32_t *header_size);

/* Find the member `name` in a SARC header part (as read by the size above).
 * Its data is bytes [*start, *end) of the archive file. */
bool sarc_find(const unsigned char *hdr, size_t n, const char *name, uint32_t *start,
               uint32_t *end);

/* Read the week out of a Schedule.xml. Sets out->status to SPOTPASS_OK or
 * SPOTPASS_NO_MACHINES. */
void schedule_week(const char *xml, size_t len, SpotPassWeek *out);

/* "YYYYMMDD" -> date. False if malformed or not a real day of 2000-2099. */
bool date_parse(const char *s, size_t len, Date *out);

/* Days since 2000-01-01 (the 3DS epoch). */
long date_days(Date d);

/* Milliseconds since 2000-01-01 00:00, the game's DateTime. */
uint64_t date_ms(Date d);

/* A short reason for the screen (at most 22 characters), e.g. "no boss.sarc". */
const char *spotpass_status_str(SpotPassStatus s);

#endif
