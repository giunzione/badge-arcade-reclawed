/*
 * net_status - text for NetStatus values (see net.h).
 *
 * Kept apart from net.c, which needs libcurl and the 3DS, because checks.c
 * shows these texts and is unit-tested on the PC.
 */
#include "net.h"

const char *net_status_str(NetStatus st)
{
	switch (st)
	{
	case NET_OK: return "OK";
	case NET_ERR_INIT: return "network unavailable";
	case NET_ERR_OFFLINE: return "no internet";
	case NET_ERR_TIMEOUT: return "timed out";
	case NET_ERR_TLS: return "check date and time";
	case NET_ERR_HTTP: return "not found on GitHub";
	case NET_ERR_TOO_BIG: return "file too big";
	case NET_ERR_BAD_DATA: return "bad patch list";
	case NET_ERR_OTHER: return "download error";
	}
	return "download error";
}
