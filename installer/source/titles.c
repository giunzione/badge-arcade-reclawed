/*
 * titles - see titles.h.
 */
#include "titles.h"

#include <stdlib.h>
#include <string.h>

const GameTitle GAME_TITLES[GAME_TITLE_COUNT] = {
	{"EUR", 0x0004000000153600ULL, 0x0004000E00153600ULL, 5136},
	{"USA", 0x0004000000153500ULL, 0x0004000E00153500ULL, 5136},
	{"JPN", 0x0004000000134600ULL, 0x0004000E00134600ULL, 5200},
};

int titles_find(const char *base_tid_hex)
{
	if (strlen(base_tid_hex) != 16 || strspn(base_tid_hex, "0123456789abcdefABCDEF") != 16)
		return -1;
	uint64_t tid = strtoull(base_tid_hex, NULL, 16);
	for (int i = 0; i < GAME_TITLE_COUNT; i++)
		if (GAME_TITLES[i].base_tid == tid)
			return i;
	return -1;
}
