/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2010  Frediano Ziglio
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

#include <config.h>
#include <stdlib.h>
#ifdef _WIN32

#include <string.h>

#include "tds.h"
#include "tdsthread.h"

TDS_RCSID(var, "$Id: win_mutex.c,v 1.4 2011-09-01 07:55:57 freddy77 Exp $");

#include "ptw32_MCS_lock.c"

static void
tds_win_mutex_lock(tds_mutex * mutex)
{
	if (!InterlockedExchangeAdd(&mutex->done, 0)) {	/* MBR fence */
		ptw32_mcs_local_node_t node;

		ptw32_mcs_lock_acquire(&mutex->lock, &node);
		if (!mutex->done) {
			InitializeCriticalSection(&mutex->crit);
			mutex->done = 1;
		}
		ptw32_mcs_lock_release(&node);
	}
	EnterCriticalSection(&mutex->crit);
}

int
tds_mutex_init(tds_mutex *mtx)
{
	mtx->lock = NULL;
	mtx->done = 0;
	return 0;
}

void tds_mutex_unlock(tds_mutex *mtx)
{
	LeaveCriticalSection(&(mtx)->crit);
}

void tds_mutex_free(tds_mutex *mtx)
{
	if ((mtx)->done) {
		DeleteCriticalSection(&(mtx)->crit);
		(mtx)->done = 0;
	}
}

void
tds_mutex_lock(tds_mutex *mtx)
{
  if (mtx->done)
    EnterCriticalSection(&(mtx)->crit);
  else
    tds_win_mutex_lock(mtx);
}

#endif
