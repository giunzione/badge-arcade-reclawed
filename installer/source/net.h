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

/* Starts sockets and curl; call once before net_get(). Returns false if they
 * can't start (then every net_get() fails with NET_ERR_INIT). */
bool net_init(void);

/* Shuts down what net_init() started. */
void net_exit(void);

/* Makes running and future net_get() calls fail quickly (used when the app
 * closes while a download is still going). Safe from any thread. */
void net_cancel(void);

/* Downloads `url`, at most `max` bytes.
 *
 * On NET_OK, *data is a malloc'd buffer the caller frees and *size its length;
 * a NUL follows the data (not counted in *size). */
NetStatus net_get(const char *url, unsigned char **data, size_t *size, size_t max);

/* Returns a short reason for `st`, for the UI (at most 24 characters). */
const char *net_status_str(NetStatus st);

#endif
