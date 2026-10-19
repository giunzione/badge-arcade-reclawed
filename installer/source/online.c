/*
 * online - see online.h.
 */
#include "online.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include "report.h" /* REPORT_REPO_URL */

/* Where releases live. Test builds point this at tests/fake_release.py
 * (`make ONLINE_BASE=http://127.0.0.1:8080`) to simulate every online case. */
#ifndef RECLAWED_ONLINE_BASE
#define RECLAWED_ONLINE_BASE REPORT_REPO_URL
#endif

#define MANIFEST_URL RECLAWED_ONLINE_BASE "/releases/latest/download/patches.txt"
#define MANIFEST_MAX_SIZE (64 * 1024)
#define THREAD_STACK (64 * 1024) /* TLS handshakes need a fair amount of stack */

static void work(void *arg)
{
	OnlineJob *job = arg;
	unsigned char *text;
	size_t size;

	job->status = net_get(MANIFEST_URL, &text, &size, MANIFEST_MAX_SIZE);
	if (job->status == NET_OK)
	{
		job->manifest_ok = manifest_parse((const char *)text, size, &job->manifest) == MANIFEST_OK;
		if (!job->manifest_ok)
			job->status = NET_ERR_BAD_DATA;
		free(text);
	}

	for (int i = 0; i < GAME_TITLE_COUNT && job->manifest_ok; i++)
	{
		if (!job->want[i].wanted)
			continue;
		const PatchEntry *e = manifest_find(&job->manifest, job->want[i].base_tid,
		                                    job->want[i].code, job->want[i].version);
		if (!e)
		{
			job->got[i].result = FETCH_NOT_LISTED;
			continue;
		}
		/* The file from the same release as the manifest, not "latest" again:
		 * a release published in between can't mix files. */
		char url[256];
		snprintf(url, sizeof(url), RECLAWED_ONLINE_BASE "/releases/download/%s/%s",
		         job->manifest.release, e->file);
		unsigned char *ips;
		size_t ips_size;
		if (net_get(url, &ips, &ips_size, MANIFEST_MAX_PATCH_SIZE) != NET_OK)
		{
			job->got[i].result = FETCH_FAILED;
			continue;
		}
		if (!manifest_entry_matches(e, ips, ips_size))
		{
			/* Not the file the manifest describes: drop it, it is never installed. */
			free(ips);
			job->got[i].result = FETCH_BAD_HASH;
			continue;
		}
		job->got[i].result = FETCH_OK;
		job->got[i].ok = true;
		job->got[i].data = ips;
		job->got[i].size = ips_size;
	}
	job->finished = true;
}

bool online_start(OnlineJob *job)
{
	job->finished = false;
	job->manifest_ok = false;
	job->status = NET_ERR_INIT;
	memset(job->got, 0, sizeof(job->got));

	/* One step below the UI thread's priority, so drawing never stutters. */
	s32 prio = 0x30;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	Thread t = threadCreate(work, job, THREAD_STACK, prio + 1, -2, false);
	job->thread = t;
	if (!t)
	{
		job->finished = true;
		return false;
	}
	return true;
}

bool online_done(OnlineJob *job)
{
	if (!job->finished)
		return false;
	if (job->thread)
	{
		threadJoin((Thread)job->thread, U64_MAX);
		threadFree((Thread)job->thread);
		job->thread = NULL;
	}
	return true;
}

void online_free(OnlineJob *job)
{
	for (int i = 0; i < GAME_TITLE_COUNT; i++)
	{
		free(job->got[i].data);
		job->got[i].data = NULL;
		job->got[i].ok = false;
	}
}
