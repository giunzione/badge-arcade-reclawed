/*
 * ips - structural check of an IPS patch, before it is copied to the SD.
 *
 * Same rules as the "Validate IPS structure" step in .github/workflows/build.yml:
 * "PATCH" header, records of [offset:3][size:2][data:size] with size > 0
 * (build_ips.py never writes RLE records), "EOF" footer, nothing after it.
 */
#ifndef IPS_H
#define IPS_H

#include <stdbool.h>
#include <stddef.h>

/* True if `data` is a well-formed IPS. If `records` is not NULL it receives
 * the number of records. */
bool ips_is_valid(const unsigned char *data, size_t size, unsigned *records);

#endif
