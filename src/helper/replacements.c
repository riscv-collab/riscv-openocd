// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2006 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Copyright (C) 2007,2008 Øyvind Harboe                                 *
 *   oyvind.harboe@zylin.com                                               *
 *                                                                         *
 *   Copyright (C) 2008 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
 ***************************************************************************/
/* DANGER!!!! These must be defined *BEFORE* replacements.h and the malloc() macro!!!! */

#include <stdlib.h>
#include <string.h>
/*
 * clear_malloc
 *
 * will alloc memory and clear it
 */
void *clear_malloc(size_t size)
{
	void *t = malloc(size);
	if (t)
		memset(t, 0x00, size);
	return t;
}

void *fill_malloc(size_t size)
{
	void *t = malloc(size);
	if (t) {
		/* We want to initialize memory to some known bad state.
		 * 0 and 0xff yields 0 and -1 as integers, which often
		 * have meaningful values. 0x5555... is not often a valid
		 * integer and is quite easily spotted in the debugger
		 * also it is almost certainly an invalid address */
		memset(t, 0x55, size);
	}
	return t;
}

#define IN_REPLACEMENTS_C
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#endif

/* replacements for gettimeofday */
#ifndef HAVE_GETTIMEOFDAY

/* Windows */
#ifdef _WIN32

#ifndef __GNUC__
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	LARGE_INTEGER li;
	__int64 t;
	static int tzflag;

	if (tv) {
		GetSystemTimeAsFileTime(&ft);
		li.LowPart  = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		t  = li.QuadPart;					/* In 100-nanosecond intervals */
		t -= EPOCHFILETIME;					/* Offset to the Epoch time */
		t /= 10;							/* In microseconds */
		tv->tv_sec  = (long)(t / 1000000);
		tv->tv_usec = (long)(t % 1000000);
	}

	if (tz) {
		if (!tzflag) {
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}
#endif	/* _WIN32 */

#endif	/* HAVE_GETTIMEOFDAY */

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t maxlen)
{
	const char *end = (const char *)memchr(s, '\0', maxlen);
	return end ? (size_t) (end - s) : maxlen;
}
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *new = malloc(len + 1);

	if (!new)
		return NULL;

	new[len] = '\0';
	return (char *) memcpy(new, s, len);
}
#endif

#ifdef _WIN32
int win_select(int max_fd, fd_set *rfds, fd_set *wfds, fd_set *efds, struct timeval *tv)
{
	DWORD ms_total, limit;
	HANDLE handles[MAXIMUM_WAIT_OBJECTS];
	int handle_slot_to_fd[MAXIMUM_WAIT_OBJECTS];
	int n_handles = 0, i;
	fd_set sock_read, sock_write, sock_except;
	fd_set aread, awrite, aexcept;
	int sock_max_fd = -1;
	struct timeval tvslice;
	int retcode;

#define SAFE_FD_ISSET(fd, set)  (set && FD_ISSET(fd, set))

	/* calculate how long we need to wait in milliseconds */
	if (!tv)
		ms_total = INFINITE;
	else {
		ms_total = tv->tv_sec * 1000;
		ms_total += tv->tv_usec / 1000;
	}

	FD_ZERO(&sock_read);
	FD_ZERO(&sock_write);
	FD_ZERO(&sock_except);

	/* build an array of handles for non-sockets */
	for (i = 0; i < max_fd; i++) {
		if (SAFE_FD_ISSET(i, rfds) || SAFE_FD_ISSET(i, wfds) || SAFE_FD_ISSET(i, efds)) {
			intptr_t handle = (intptr_t) _get_osfhandle(i);
			handles[n_handles] = (HANDLE)handle;
			if (handles[n_handles] == INVALID_HANDLE_VALUE) {
				/* socket */
				if (SAFE_FD_ISSET(i, rfds))
					FD_SET(i, &sock_read);
				if (SAFE_FD_ISSET(i, wfds))
					FD_SET(i, &sock_write);
				if (SAFE_FD_ISSET(i, efds))
					FD_SET(i, &sock_except);
				if (i > sock_max_fd)
					sock_max_fd = i;
			} else {
				handle_slot_to_fd[n_handles] = i;
				n_handles++;
			}
		}
	}

	if (n_handles == 0) {
		/* plain sockets only - let winsock handle the whole thing */
		return select(max_fd, rfds, wfds, efds, tv);
	}

	/* mixture of handles and sockets; lets multiplex between
	 * winsock and waiting on the handles */

	FD_ZERO(&aread);
	FD_ZERO(&awrite);
	FD_ZERO(&aexcept);

	limit = GetTickCount() + ms_total;
	do {
		retcode = 0;

		if (sock_max_fd >= 0) {
			/* overwrite the zero'd sets here; the select call
			 * will clear those that are not active */
			aread = sock_read;
			awrite = sock_write;
			aexcept = sock_except;

			tvslice.tv_sec = 0;
			tvslice.tv_usec = 1000;

			retcode = select(sock_max_fd + 1, &aread, &awrite, &aexcept, &tvslice);
		}

		if (n_handles > 0) {
			/* check handles */
			DWORD wret;

			wret = MsgWaitForMultipleObjects(n_handles,
					handles,
					FALSE,
					retcode > 0 ? 0 : 1,
					QS_ALLEVENTS);

			if (wret == WAIT_TIMEOUT) {
				/* set retcode to 0; this is the default.
				 * select() may have set it to something else,
				 * in which case we leave it alone, so this branch
				 * does nothing */
				;
			} else if (wret == WAIT_FAILED) {
				if (retcode == 0)
					retcode = -1;
			} else {
				if (retcode < 0)
					retcode = 0;
				for (i = 0; i < n_handles; i++) {
					if (WaitForSingleObject(handles[i], 0) == WAIT_OBJECT_0) {
						if (SAFE_FD_ISSET(handle_slot_to_fd[i], rfds)) {
							DWORD bytes;
							intptr_t handle = (intptr_t) _get_osfhandle(
									handle_slot_to_fd[i]);

							if (PeekNamedPipe((HANDLE)handle, NULL, 0,
								    NULL, &bytes, NULL)) {
								/* check to see if gdb pipe has data available */
								if (bytes) {
									FD_SET(handle_slot_to_fd[i], &aread);
									retcode++;
								}
							} else {
								FD_SET(handle_slot_to_fd[i], &aread);
								retcode++;
							}
						}
						if (SAFE_FD_ISSET(handle_slot_to_fd[i], wfds)) {
							FD_SET(handle_slot_to_fd[i], &awrite);
							retcode++;
						}
						if (SAFE_FD_ISSET(handle_slot_to_fd[i], efds)) {
							FD_SET(handle_slot_to_fd[i], &aexcept);
							retcode++;
						}
					}
				}
			}
		}
	} while (retcode == 0 && (ms_total == INFINITE || GetTickCount() < limit));

	if (rfds)
		*rfds = aread;
	if (wfds)
		*wfds = awrite;
	if (efds)
		*efds = aexcept;

	return retcode;
}
#endif
