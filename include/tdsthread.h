/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 *
 * Copyright (C) 2005 Liam Widdowson
 * Copyright (C) 2010-2012 Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef TDSTHREAD_H
#define TDSTHREAD_H 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* $Id: tdsthread.h,v 1.13 2011-09-09 08:50:47 freddy77 Exp $ */

#undef TDS_HAVE_MUTEX

#if defined(_THREAD_SAFE) && defined(TDS_HAVE_PTHREAD_MUTEX)

#include <pthread.h>

typedef pthread_mutex_t tds_mutex;
#define TDS_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

static inline void tds_mutex_lock(tds_mutex *mtx)
{
	pthread_mutex_lock(mtx);
}

static inline void tds_mutex_unlock(tds_mutex *mtx)
{
	pthread_mutex_unlock(mtx);
}

static inline int tds_mutex_init(tds_mutex *mtx)
{
	return pthread_mutex_init(mtx, NULL);
}

static inline void tds_mutex_free(tds_mutex *mtx)
{
	pthread_mutex_destroy(mtx);
}

#define TDS_HAVE_MUTEX 1

#elif defined(_WIN32)

struct ptw32_mcs_node_t_;

typedef struct {
	struct ptw32_mcs_node_t_ *lock;
	LONG done;
	CRITICAL_SECTION crit;
} tds_mutex;

#define TDS_MUTEX_INITIALIZER { NULL, 0 }

int tds_mutex_init(tds_mutex *mtx);
void tds_mutex_unlock(tds_mutex *mtx);
void tds_mutex_free(tds_mutex *mtx);
void tds_mutex_lock(tds_mutex *mtx);

#define TDS_HAVE_MUTEX 1

#else

/* define noops as "successful" */
typedef struct {
} tds_mutex;

#define TDS_MUTEX_INITIALIZER {}

static inline void tds_mutex_lock(tds_mutex *mtx)
{
}

static inline void tds_mutex_unlock(tds_mutex *mtx)
{
}

static inline int tds_mutex_init(tds_mutex *mtx)
{
	return 0;
}

static inline void tds_mutex_free(tds_mutex *mtx)
{
}

#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
