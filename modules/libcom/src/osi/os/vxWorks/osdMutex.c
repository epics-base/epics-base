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
#include <time.h>
#include <objLib.h>
#include <sysLib.h>

/* The following not defined in an vxWorks header */
int sysClkRateGet(void);

#define EPICS_PRIVATE_API

#include "epicsMutex.h"

struct epicsMutexOSD * epicsMutexOsdCreate(void)
{
    return((struct epicsMutexOSD *)
        semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY));
}

void epicsMutexOsdDestroy(struct epicsMutexOSD * id)
{
    semDelete((SEM_ID)id);
}

epicsMutexLockStatus epicsMutexOsdTryLock(struct epicsMutexOSD * id)
{
    int status;
    status = semTake((SEM_ID)id,NO_WAIT);
    if(status==OK) return(epicsMutexLockOK);
    if(errno==S_objLib_OBJ_UNAVAILABLE) return(epicsMutexLockTimeout);
    return(epicsMutexLockError);
}

void epicsMutexOsdShow(struct epicsMutexOSD * id,unsigned int level)
{
    semShow((SEM_ID)id,level);
}

void epicsMutexOsdShowAll(void) {}
