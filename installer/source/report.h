/*
 * report - the link to a pre-filled GitHub issue for a game version that has
 * no patch yet, shown as a QR code by the app.
 *
 * The issue form is .github/ISSUE_TEMPLATE/game-version.yml; GitHub fills its
 * fields from query parameters named after the field ids. The URL is kept
 * short (about 220 bytes) so the QR code stays small enough to scan from the
 * bottom screen.
 *
 * Pure string building, unit-tested in tests/test_report.c.
 */
#ifndef REPORT_H
#define REPORT_H

#include <stddef.h>
#include <stdint.h>

#define REPORT_REPO_URL "https://github.com/giunzione/badge-arcade-reclawed"
#define REPORT_URL_MAX 320

/* Write the issue URL into `out`. `luma` may be NULL when Luma3DS wasn't
 * detected. Returns 0, or -1 if it doesn't fit in `len`. */
int report_issue_url(char *out, size_t len, const char *region, uint64_t base_tid, char code,
                     unsigned version, const char *app_version, const char *luma);

/* Percent-encode `in` for a query string value (RFC 3986 unreserved
 * characters stay as they are). Returns 0, or -1 if it doesn't fit. */
int report_url_encode(const char *in, char *out, size_t len);

#endif
