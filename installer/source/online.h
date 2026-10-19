/*
 * online - fetch the latest patches from GitHub in a background thread (3DS only).
 *
 * 1. download releases/latest/download/patches.txt and parse it;
 * 2. for every game code the caller asked for that the manifest lists,
 *    download the file from that same release and check its size and SHA-256.
 *
 * The UI keeps running meanwhile; main.c polls online_done() every frame.
 * Any failure just leaves the job without results: the app then falls back
 * to the patches built into it (manifest_choose()).
 */
#ifndef ONLINE_H
#define ONLINE_H

#include <stdbool.h>
#include <stdint.h>

#include "manifest.h"
#include "net.h"
#include "titles.h"

typedef struct
{
	/* Filled by the caller before online_start(). */
	struct
	{
		bool wanted;
		uint64_t base_tid;
		char code;
		unsigned version;
	} want[GAME_TITLE_COUNT];

	/* Results, valid once online_done() returned true. */
	NetStatus status;          /* of the manifest download/parse */
	bool manifest_ok;
	Manifest manifest;
	struct
	{
		FetchResult result;
		bool ok;               /* downloaded and verified (result == FETCH_OK) */
		unsigned char *data;
		size_t size;
	} got[GAME_TITLE_COUNT];

	/* Internal */
	void *thread;
	volatile bool finished;
} OnlineJob;

/* Start the thread. Returns false (and marks the job finished, failed) if it
 * can't be started. */
bool online_start(OnlineJob *job);

/* True once the work is over; joins the thread the first time. */
bool online_done(OnlineJob *job);

/* Free the downloaded files. */
void online_free(OnlineJob *job);

#endif
