/*
 * ips - see ips.h.
 */
#include "ips.h"

#include <string.h>

bool ips_is_valid(const unsigned char *data, size_t size, unsigned *records)
{
	if (size < 8 || memcmp(data, "PATCH", 5) != 0)
		return false;

	size_t i = 5;
	unsigned n = 0;
	for (;;)
	{
		if (i + 3 > size)
			return false; /* ran out of data before "EOF" */
		/* "EOF" is only the footer when it's the last 3 bytes; otherwise those
		 * bytes are the offset 0x454F46 of a record. */
		if (i + 3 == size && memcmp(data + i, "EOF", 3) == 0)
			break;
		if (i + 5 > size)
			return false;
		size_t len = ((size_t)data[i + 3] << 8) | data[i + 4];
		if (len == 0)
			return false; /* RLE record: never produced by build_ips.py */
		i += 5 + len;
		n++;
	}
	if (records)
		*records = n;
	return true;
}
