/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* osi/os/posix/osdMutex.c */

/* Author:  Marty Kraimer Date:    13AUG1999 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define EPICS_PRIVATE_API
#define epicsStdioStdStreams
#define epicsStdioStdPrintfEtc

#include "epicsMutex.h"
#include "epicsMutexImpl.h"
#include "osdPosixMutexPriv.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsStdio.h"

#define checkStatus(status,message) \
    if((status)) { \
        errlogPrintf("epicsMutex %s failed: " ERL_ERROR " %s\n", \
            (message), strerror((status))); \
    }
#define checkStatusQuit(status,message,method) \
    if(status) { \
        errlogPrintf("epicsMutex %s failed: " ERL_ERROR " %s\n", \
            (message), strerror((status))); \
        cantProceed((method)); \
    }

#if defined(DONT_USE_POSIX_THREAD_PRIORITY_SCHEDULING)
#undef _POSIX_THREAD_PRIO_INHERIT
#endif

/* Global var - pthread_once does not support passing args but it is more efficient
 * than epicsThreadOnce which always acquires a mutex.
 */
static pthread_mutexattr_t globalAttrDefault;
static pthread_mutexattr_t globalAttrRecursive;
static pthread_once_t      globalAttrInitOnce = PTHREAD_ONCE_INIT;

static void globalAttrInit()
{
    int status;

    status = pthread_mutexattr_init(&globalAttrDefault);
    checkStatusQuit(status,"pthread_mutexattr_init(&globalAttrDefault)","globalAttrInit");
    status = pthread_mutexattr_init(&globalAttrRecursive);
    checkStatusQuit(status,"pthread_mutexattr_init(&globalAttrRecursive)","globalAttrInit");
    status = pthread_mutexattr_settype(&globalAttrRecursive, PTHREAD_MUTEX_RECURSIVE);
    checkStatusQuit(status, "pthread_mutexattr_settype(&globalAttrRecursive, PTHREAD_MUTEX_RECURSIVE)", "globalAttrInit");
#if defined _POSIX_THREAD_PRIO_INHERIT
    status = pthread_mutexattr_setprotocol(&globalAttrDefault, PTHREAD_PRIO_INHERIT);
    if (errVerbose) checkStatus(status, "pthread_mutexattr_setprotocol(&globalAttrDefault, PTHREAD_PRIO_INHERIT)");
    status = pthread_mutexattr_setprotocol(&globalAttrRecursive, PTHREAD_PRIO_INHERIT);
    if (errVerbose) checkStatus(status, "pthread_mutexattr_setprotocol(&globalAttrRecursive, PTHREAD_PRIO_INHERIT)");
    if (status == 0) {
        /* Can we really use PTHREAD_PRIO_INHERIT? */
        pthread_mutex_t temp;
        status = pthread_mutex_init(&temp, &globalAttrRecursive);
        if (errVerbose) checkStatus(status, "pthread_mutex_init(&temp, &globalAttrRecursive)");
        if (status != 0) {
            /* No, PTHREAD_PRIO_INHERIT does not work, fall back to PTHREAD_PRIO_NONE */;
            pthread_mutexattr_setprotocol(&globalAttrDefault, PTHREAD_PRIO_NONE);
            pthread_mutexattr_setprotocol(&globalAttrRecursive, PTHREAD_PRIO_NONE);
        } else {
            pthread_mutex_destroy(&temp);
        }
    }
#endif
}

int osdPosixMutexInit (pthread_mutex_t *m, int mutextype)
{
    pthread_mutexattr_t *atts;
    int status;

    status = pthread_once( &globalAttrInitOnce, globalAttrInit );
    checkStatusQuit(status,"pthread_once","epicsPosixMutexAttrGet");

    switch (mutextype) {
        case PTHREAD_MUTEX_DEFAULT:
            atts = &globalAttrDefault;
            break;
        case PTHREAD_MUTEX_RECURSIVE:
            atts = &globalAttrRecursive;
            break;
        default:
            return ENOTSUP;
    }
    return pthread_mutex_init(m, atts);
}

static int mutexLock(pthread_mutex_t *id)
{
    int status;

    while ((status = pthread_mutex_lock(id)) == EINTR) {
        errlogPrintf("pthread_mutex_lock returned EINTR. Violates SUSv3\n");
    }
    return status;
}

/* used if OS does not support statically allocated mutex */
static pthread_once_t epicsMutexOsdOnce = PTHREAD_ONCE_INIT;

static void epicsMutexOsdInit(void)
{
    int ret = pthread_mutex_init(&epicsMutexGlobalLock.osd, NULL);
    if(ret) {
        /* something has gone wrong early.  Not much can be done...*/
        fprintf(stderr, "osdMutex early init failure %d.\n", ret);
        abort();
    }
}

void epicsMutexOsdSetup()
{
    int ret = pthread_once(&epicsMutexOsdOnce, &epicsMutexOsdInit);
    if(ret) {
        /* ditto...*/
        fprintf(stderr, "osdMutex early once failure %d.\n", ret);
        abort();
    }
}

long epicsMutexOsdPrepare(struct epicsMutexParm *pmutex) {
    return osdPosixMutexInit(&pmutex->osd, PTHREAD_MUTEX_RECURSIVE);
}

void epicsMutexOsdCleanup(struct epicsMutexParm *pmutex)
{
    int status = pthread_mutex_destroy(&pmutex->osd);
    checkStatus(status, "pthread_mutex_destroy");
}

void epicsMutexUnlock(struct epicsMutexParm * pmutex)
{
    int status = pthread_mutex_unlock(&pmutex->osd);
    checkStatus(status, "pthread_mutex_unlock epicsMutexOsdUnlock");
}

epicsMutexLockStatus epicsMutexLock(struct epicsMutexParm * pmutex)
{
    int status = mutexLock(&pmutex->osd);
    if (status == EINVAL) return epicsMutexLockError;
    if(status) {
        errlogMessage("epicsMutex pthread_mutex_lock failed: error epicsMutexLock\n");
        return epicsMutexLockError;
    }
    return epicsMutexLockOK;
}

epicsMutexLockStatus epicsMutexTryLock(struct epicsMutexParm * pmutex)
{
    int status;

    if (!pmutex) return epicsMutexLockError;
    status = pthread_mutex_trylock(&pmutex->osd);
    if (status == EINVAL) return epicsMutexLockError;
    if (status == EBUSY) return epicsMutexLockTimeout;
    if(status) {
        errlogMessage("epicsMutex pthread_mutex_trylock failed: error epicsMutexOsdTryLock");
        return epicsMutexLockError;
    }
    return epicsMutexLockOK;
}

void epicsMutexOsdShow(struct epicsMutexParm * pmutex, unsigned int level)
{
    (void)level;
    /* GLIBC w/ NTPL is passing the &lock.__data.__lock as the first argument (UADDR)
     * of the futex() syscall.  __lock is at offset 0 of the enclosing structures.
     */
    epicsStdoutPrintf("    pthread_mutex_t* uaddr=%p\n", &pmutex->osd);
}

void epicsMutexOsdShowAll(void)
{
#if defined _POSIX_THREAD_PRIO_INHERIT
    int proto = -1;
    int ret = pthread_mutexattr_getprotocol(&globalAttrRecursive, &proto);
    if(ret) {
        epicsStdoutPrintf("PI maybe not enabled: %d\n", ret);
    } else {
        epicsStdoutPrintf("PI is%s enabled\n", proto==PTHREAD_PRIO_INHERIT ? "" : " not");
    }
#else
    epicsStdoutPrintf("PI not supported\n");
#endif
}
