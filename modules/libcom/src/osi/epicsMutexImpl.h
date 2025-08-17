/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2023 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Only include from osdMutex.c */

#ifndef epicsMutexImpl_H
#define epicsMutexImpl_H

#if defined(vxWorks)
#  include <vxWorks.h>
#  include <semLib.h>
#elif defined(_WIN32)
#  define VC_EXTRALEAN
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(__rtems__)
#  include <rtems.h>
#  include <rtems/score/cpuopts.h>
#else
#  include <pthread.h>
#endif

#include "ellLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct epicsMutexParm {
    /* global list of mutex */
    ELLNODE node;
    /* location where mutex was allocated */
    const char *pFileName;
    int lineno;
#if defined(vxWorks)
    SEM_ID osd;
#elif defined(_WIN32)
    CRITICAL_SECTION osd;
#elif defined(__RTEMS_MAJOR__) && __RTEMS_MAJOR__<5
    Semaphore_Control *osd;
#else
    pthread_mutex_t osd;
#endif
};

void epicsMutexOsdSetup(void);
long epicsMutexOsdPrepare(struct epicsMutexParm *);
void epicsMutexOsdCleanup(struct epicsMutexParm *);
void epicsMutexOsdShow(struct epicsMutexParm *,unsigned  int level);
void epicsMutexOsdShowAll(void);

extern struct epicsMutexParm epicsMutexGlobalLock;

#ifdef __cplusplus
} // extern "C
#endif

#endif // epicsMutexImpl_H
