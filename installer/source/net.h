/*
 * net - HTTPS downloads with libcurl + mbedTLS (3DS only).
 *
 * Certificates are checked against the Mozilla roots bundled in the RomFS
 * (certs/cacert.pem). Redirects are followed: GitHub sends release downloads
 * to its file servers.
 */
#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
	NET_OK,
	NET_ERR_INIT,     /* sockets or curl couldn't start */
	NET_ERR_OFFLINE,  /* no connection / DNS failure */
	NET_ERR_TIMEOUT,
	NET_ERR_TLS,      /* certificate rejected: often a wrong console clock */
	NET_ERR_HTTP,     /* the server answered with an error (e.g. 404) */
	NET_ERR_TOO_BIG,  /* more than the caller's limit */
	NET_ERR_BAD_DATA, /* downloaded, but not a patch list this app can read */
	NET_ERR_OTHER,    /* keep last (the tests loop up to it) */
} NetStatus;

/* Start sockets and curl; call once before net_get(). */
bool net_init(void);
void net_exit(void);

/* Make running and future net_get() calls fail quickly (used when the app
 * closes while a download is still going). Safe from any thread. */
void net_cancel(void);

/* Download `url` into a malloc'd buffer (caller frees), at most `max` bytes.
 * A NUL is appended after the data (not counted in *size). */
NetStatus net_get(const char *url, unsigned char **data, size_t *size, size_t max);

/* Short reason for the UI (at most 24 chars). */
const char *net_status_str(NetStatus st);

#endif
