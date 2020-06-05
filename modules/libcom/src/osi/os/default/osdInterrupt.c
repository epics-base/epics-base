/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osi/default/osdInterrupt.c */

/* Author:  Marty Kraimer Date:    15JUL99 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "epicsMutex.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsInterrupt.h"


static epicsMutexId globalLock = NULL;
static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

static void initOnce(void *junk)
{
    globalLock = epicsMutexMustCreate();
}

LIBCOM_API int epicsInterruptLock()
{
    epicsThreadOnce(&onceId, initOnce, NULL);
    epicsMutexMustLock(globalLock);
    return 0;
}

LIBCOM_API void epicsInterruptUnlock(int key)
{
    if (!globalLock)
        cantProceed("epicsInterruptUnlock called before epicsInterruptLock\n");
    epicsMutexUnlock(globalLock);
}

LIBCOM_API int epicsInterruptIsInterruptContext()
{
    return 0;
}

LIBCOM_API void epicsInterruptContextMessage(const char *message)
{
    errlogPrintf("%s", message);
}

