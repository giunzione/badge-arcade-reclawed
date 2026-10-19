/*
 * luma_config - see luma_config.h.
 *
 * The scanner follows the rules of inih, the INI parser Luma3DS uses, closely
 * enough to find the same key/value pairs Luma will see:
 *   - an optional UTF-8 BOM at the start is skipped;
 *   - lines starting with ';' or '#' (after indentation) are comments;
 *   - "[name]" starts a section;
 *   - "name = value" or "name : value"; name and value are trimmed;
 *   - in a value, ';' preceded by whitespace starts an inline comment;
 *   - an indented line right after a key is a continuation of that key
 *     (inih calls the handler again with the same name).
 */
#include "luma_config.h"

#include <string.h>

/* One logical piece of the file: [start, end) offsets into the text. */
typedef struct
{
	size_t start, end;
} Span;

static int is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

static Span trim(const char *text, Span s)
{
	while (s.start < s.end && is_space(text[s.start]))
		s.start++;
	while (s.end > s.start && is_space(text[s.end - 1]))
		s.end--;
	return s;
}

static int span_equals(const char *text, Span s, const char *word)
{
	size_t n = strlen(word);
	return s.end - s.start == n && memcmp(text + s.start, word, n) == 0;
}

/* Parse a non-negative decimal number; -1 if the span isn't one. */
static int span_to_int(const char *text, Span s)
{
	if (s.start == s.end || s.end - s.start > 5)
		return -1;
	int v = 0;
	for (size_t i = s.start; i < s.end; i++)
	{
		if (text[i] < '0' || text[i] > '9')
			return -1;
		v = v * 10 + (text[i] - '0');
	}
	return v;
}

/* Cut an inih-style inline comment (" ;...") off a value. */
static Span strip_inline_comment(const char *text, Span v)
{
	for (size_t i = v.start; i < v.end; i++)
		if (text[i] == ';' && i > v.start && is_space(text[i - 1]))
		{
			v.end = i;
			break;
		}
	return trim(text, v);
}

/* Record one value of enable_game_patching. */
static LumaCfgStatus take_value(const char *text, Span v, LumaCfgInfo *info, int *seen)
{
	if (*seen)
		return LUMACFG_ERR_DUPLICATE_KEY;
	*seen = 1;
	v = strip_inline_comment(text, v);
	if (v.end - v.start != 1 || (text[v.start] != '0' && text[v.start] != '1'))
		return LUMACFG_ERR_BAD_VALUE;
	info->game_patching = text[v.start] - '0';
	info->value_offset = v.start;
	return LUMACFG_OK;
}

LumaCfgStatus lumacfg_parse(const char *text, size_t len, LumaCfgInfo *info)
{
	info->version_major = -1;
	info->version_minor = -1;
	info->game_patching = -1;
	info->value_offset = 0;

	Span section = {0, 0};
	int in_boot = 0, in_meta = 0;
	int last_key_is_target = 0; /* previous key was [boot] enable_game_patching */
	int have_last_key = 0;
	int seen = 0;
	int version_seen = 0; /* config_version_major present, valid or not */

	size_t pos = 0;
	if (len >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0)
		pos = 3;

	while (pos < len)
	{
		/* Current line is [pos, eol); the '\n' itself is not part of it. */
		size_t eol = pos;
		while (eol < len && text[eol] != '\n')
			eol++;
		Span line = {pos, eol};
		pos = eol + 1;

		Span t = trim(text, line);
		if (t.start == t.end)
			continue;
		char first = text[t.start];
		if (first == ';' || first == '#')
			continue;

		/* Indented line after a key: inih treats it as another value. */
		if (have_last_key && t.start > line.start)
		{
			if (last_key_is_target)
			{
				LumaCfgStatus st = take_value(text, t, info, &seen);
				if (st != LUMACFG_OK)
					return st;
			}
			continue;
		}

		if (first == '[')
		{
			size_t close = t.start + 1;
			while (close < t.end && text[close] != ']')
				close++;
			section = trim(text, (Span){t.start + 1, close});
			in_boot = span_equals(text, section, "boot");
			in_meta = span_equals(text, section, "meta");
			have_last_key = 0;
			continue;
		}

		/* name = value (inih also accepts ':' as the separator). */
		size_t sep = t.start;
		while (sep < t.end && text[sep] != '=' && text[sep] != ':')
			sep++;
		if (sep == t.end)
		{
			have_last_key = 0; /* not a key line; Luma would flag it, we ignore it */
			continue;
		}
		Span name = trim(text, (Span){t.start, sep});
		Span value = trim(text, (Span){sep + 1, t.end});
		have_last_key = 1;
		last_key_is_target = in_boot && span_equals(text, name, "enable_game_patching");

		if (in_meta && span_equals(text, name, "config_version_major"))
		{
			version_seen = 1;
			info->version_major = span_to_int(text, strip_inline_comment(text, value));
		}
		else if (in_meta && span_equals(text, name, "config_version_minor"))
			info->version_minor = span_to_int(text, strip_inline_comment(text, value));
		else if (last_key_is_target)
		{
			LumaCfgStatus st = take_value(text, value, info, &seen);
			if (st != LUMACFG_OK)
				return st;
		}
	}

	if (!version_seen)
		return LUMACFG_ERR_NO_VERSION;
	if (info->version_major != LUMACFG_SUPPORTED_MAJOR)
		return LUMACFG_ERR_UNSUPPORTED_VERSION;
	if (!seen)
		return LUMACFG_ERR_NO_KEY;
	return LUMACFG_OK;
}

LumaCfgStatus lumacfg_set_game_patching(char *text, size_t len, int enabled)
{
	LumaCfgInfo info;
	LumaCfgStatus st = lumacfg_parse(text, len, &info);
	if (st == LUMACFG_OK)
		text[info.value_offset] = enabled ? '1' : '0';
	return st;
}

const char *lumacfg_status_str(LumaCfgStatus status)
{
	switch (status)
	{
	case LUMACFG_OK: return "OK";
	case LUMACFG_ERR_NO_VERSION: return "config version not found";
	case LUMACFG_ERR_UNSUPPORTED_VERSION: return "unsupported config version";
	case LUMACFG_ERR_NO_KEY: return "enable_game_patching not found";
	case LUMACFG_ERR_DUPLICATE_KEY: return "enable_game_patching appears twice";
	case LUMACFG_ERR_BAD_VALUE: return "enable_game_patching is not 0 or 1";
	}
	return "unknown error";
}
