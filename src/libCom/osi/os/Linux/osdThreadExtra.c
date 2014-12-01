/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2012 ITER Organization
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author:  Marty Kraimer Date:    18JAN2000 */

/* This differs from the posix implementation of epicsThread by:
 * - printing the Linux LWP ID instead of the POSIX thread ID in the show routines
 * - installing a default thread start hook, that sets the Linux thread name to the
 *   EPICS thread name to make it visible on OS level, and discovers the LWP ID */

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/prctl.h>

#define epicsExportSharedSymbols
#include "epicsStdio.h"
#include "ellLib.h"
#include "epicsEvent.h"
#include "epicsThread.h"

void epicsThreadShowInfo(epicsThreadId pthreadInfo, unsigned int level)
{
    if (!pthreadInfo) {
        fprintf(epicsGetStdout(), "            NAME       EPICS ID   "
            "LWP ID   OSIPRI  OSSPRI  STATE\n");
    } else {
        struct sched_param param;
        int priority = 0;

        if (pthreadInfo->tid) {
            int policy;
            int status = pthread_getschedparam(pthreadInfo->tid, &policy,
                &param);

            if (!status)
                priority = param.sched_priority;
        }
        fprintf(epicsGetStdout(),"%16.16s %14p %8lu    %3d%8d %8.8s\n",
             pthreadInfo->name,(void *)
             pthreadInfo,(unsigned long)pthreadInfo->lwpId,
             pthreadInfo->osiPriority,priority,
             pthreadInfo->isSuspended ? "SUSPEND" : "OK");
    }
}

static void thread_hook(epicsThreadId pthreadInfo)
{
    /* Set the name of the thread's process. Limited to 16 characters. */
    char comm[16];

    if (strcmp(pthreadInfo->name, "_main_")) {
        snprintf(comm, sizeof(comm), "%s", pthreadInfo->name);
        prctl(PR_SET_NAME, comm, 0l, 0l, 0l);
    }
    pthreadInfo->lwpId = syscall(SYS_gettid);
}

epicsShareDef EPICS_THREAD_HOOK_ROUTINE epicsThreadHookDefault = thread_hook;
epicsShareDef EPICS_THREAD_HOOK_ROUTINE epicsThreadHookMain = thread_hook;
