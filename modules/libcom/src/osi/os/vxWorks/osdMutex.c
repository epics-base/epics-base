/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* os/vxWorks/osdMutex.c */

/* Author:  Marty Kraimer Date:    25AUG99 */

#include <vxWorks.h>
#include <semLib.h>
#include <errno.h>
#include <time.h>
#include <objLib.h>
#include <sysLib.h>

/* The following not defined in an vxWorks header */
int sysClkRateGet(void);

#define EPICS_PRIVATE_API

#include "epicsMutex.h"
#include "epicsMutexImpl.h"

void epicsMutexOsdSetup(void)
{
    if(!epicsMutexGlobalLock.osd) {
        epicsMutexOsdPrepare(&epicsMutexGlobalLock);
    }
}

long epicsMutexOsdPrepare(struct epicsMutexParm *mutex)
{
    mutex->osd = semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY);
    return mutex->osd ? 0 : ENOMEM;
}

void epicsMutexOsdCleanup(struct epicsMutexParm *mutex)
{
    semDelete(mutex->osd);
}

epicsMutexLockStatus epicsMutexLock(struct epicsMutexParm *mutex)
{
    return semTake(mutex->osd,WAIT_FOREVER)==OK ? epicsMutexLockOK : epicsMutexLockError;
}

epicsMutexLockStatus epicsMutexTryLock(struct epicsMutexParm *mutex)
{
    int status = semTake(mutex->osd,NO_WAIT);
    if(status==OK) return(epicsMutexLockOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(epicsMutexLockTimeout);
    return(epicsMutexLockError);
}

void epicsMutexUnlock(struct epicsMutexParm *mutex)
{
    semGive(mutex->osd);
}

void epicsMutexOsdShow(struct epicsMutexParm *mutex,unsigned int level)
{
    semShow(mutex->osd,level);
}

void epicsMutexOsdShowAll(void) {}
