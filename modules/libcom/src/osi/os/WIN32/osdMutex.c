/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osdMutex.c */
/*
 *      WIN32 version
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 */

#include <stdio.h>
#include <limits.h>

#define EPICS_PRIVATE_API

#include "libComAPI.h"
#include "epicsMutex.h"
#include "epicsMutexImpl.h"
#include "epicsThread.h"
#include "epicsAssert.h"
#include "epicsStdio.h"

static epicsThreadOnceId epicsMutexOsdOnce = EPICS_THREAD_ONCE_INIT;
static void epicsMutexOsdInit(void* unused)
{
    (void)unused;
    InitializeCriticalSection(&epicsMutexGlobalLock.osd);
}

void epicsMutexOsdSetup()
{
    epicsThreadOnce(&epicsMutexOsdOnce, &epicsMutexOsdInit, NULL);
}

long epicsMutexOsdPrepare(struct epicsMutexParm *mutex)
{
    InitializeCriticalSection(&mutex->osd);
    return 0;
}

void epicsMutexOsdCleanup(struct epicsMutexParm *mutex)
{
    DeleteCriticalSection(&mutex->osd);
}

void epicsStdCall epicsMutexUnlock ( struct epicsMutexParm *mutex )
{
    LeaveCriticalSection ( &mutex->osd );
}

epicsMutexLockStatus epicsStdCall epicsMutexLock ( struct epicsMutexParm *mutex )
{
    EnterCriticalSection ( &mutex->osd );
    return epicsMutexLockOK;
}

epicsMutexLockStatus epicsStdCall epicsMutexTryLock ( struct epicsMutexParm *mutex )
{
    return TryEnterCriticalSection ( &mutex->osd ) ? epicsMutexLockOK : epicsMutexLockTimeout;
}

void epicsMutexOsdShow ( struct epicsMutexParm *mutex, unsigned level )
{
    (void)level;
    printf ("epicsMutex: win32 critical section at %p\n",
        (void * ) & mutex->osd );
}

void epicsMutexOsdShowAll(void) {}

