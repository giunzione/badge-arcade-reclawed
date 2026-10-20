/*
 * net - see net.h.
 */
#include "net.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>
#include <curl/curl.h>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

#define SOC_BUFFER_SIZE 0x100000
#define CA_BUNDLE "romfs:/certs/cacert.pem"

static u32 *soc_buffer;
static bool curl_ready;
static volatile bool cancelled;

void net_cancel(void)
{
	cancelled = true;
}

/* curl calls this regularly during a transfer; non-zero aborts it. */
static int on_progress(void *user, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                       curl_off_t ulnow)
{
	(void)user, (void)dltotal, (void)dlnow, (void)ultotal, (void)ulnow;
	return cancelled ? 1 : 0;
}

bool net_init(void)
{
	soc_buffer = memalign(0x1000, SOC_BUFFER_SIZE);
	if (!soc_buffer || R_FAILED(socInit(soc_buffer, SOC_BUFFER_SIZE)))
	{
		free(soc_buffer);
		soc_buffer = NULL;
		return false;
	}
	curl_ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
	return curl_ready;
}

void net_exit(void)
{
	if (curl_ready)
		curl_global_cleanup();
	curl_ready = false;
	if (soc_buffer)
	{
		socExit();
		free(soc_buffer);
		soc_buffer = NULL;
	}
}

/* Growing download buffer; stops the transfer past the limit. */
typedef struct
{
	unsigned char *data;
	size_t size, max;
	bool too_big;
} Buffer;

/* curl write callback: appends the received bytes to the Buffer `user`.
 * Returns fewer bytes than received (which stops curl) past the size limit
 * or when memory runs out. */
static size_t on_data(char *ptr, size_t size, size_t nmemb, void *user)
{
	Buffer *b = user;
	size_t n = size * nmemb;
	if (b->size + n > b->max)
	{
		b->too_big = true;
		return 0; /* makes curl abort with CURLE_WRITE_ERROR */
	}
	unsigned char *grown = realloc(b->data, b->size + n + 1);
	if (!grown)
		return 0;
	b->data = grown;
	memcpy(b->data + b->size, ptr, n);
	b->size += n;
	b->data[b->size] = '\0';
	return n;
}

NetStatus net_get(const char *url, unsigned char **data, size_t *size, size_t max)
{
	*data = NULL;
	*size = 0;
	if (!curl_ready)
		return NET_ERR_INIT;
	CURL *c = curl_easy_init();
	if (!c)
		return NET_ERR_INIT;

	Buffer b = {NULL, 0, max, false};
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_data);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L); /* HTTP >= 400 is an error */
	curl_easy_setopt(c, CURLOPT_CAINFO, CA_BUNDLE);
	curl_easy_setopt(c, CURLOPT_USERAGENT, "reclawed-installer/" APP_VERSION);
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);

	CURLcode res = curl_easy_perform(c);
	curl_easy_cleanup(c);

	NetStatus st;
	switch (res)
	{
	case CURLE_OK: st = NET_OK; break;
	case CURLE_COULDNT_RESOLVE_HOST:
	case CURLE_COULDNT_CONNECT: st = NET_ERR_OFFLINE; break;
	case CURLE_OPERATION_TIMEDOUT: st = NET_ERR_TIMEOUT; break;
	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_SSL_CONNECT_ERROR:
	case CURLE_SSL_CACERT_BADFILE: st = NET_ERR_TLS; break;
	case CURLE_HTTP_RETURNED_ERROR: st = NET_ERR_HTTP; break;
	default: st = b.too_big ? NET_ERR_TOO_BIG : NET_ERR_OTHER; break;
	}
	if (st != NET_OK)
	{
		free(b.data);
		return st;
	}
	if (!b.data) /* empty body: still return a valid buffer */
		b.data = calloc(1, 1);
	*data = b.data;
	*size = b.size;
	return NET_OK;
}
