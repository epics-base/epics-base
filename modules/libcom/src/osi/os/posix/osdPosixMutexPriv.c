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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "osdPosixMutexPriv.h"
#include "cantProceed.h"
#include "errlog.h"

#ifndef checkStatus
#define checkStatus(status,message) \
    if((status)) { \
        errlogPrintf("epicsMutex %s failed: " ERL_ERROR " %s\n", \
            (message), strerror((status))); \
    }
#endif
#ifndef checkStatusQuit
#define checkStatusQuit(status,message,method) \
    if(status) { \
        errlogPrintf("epicsMutex %s failed: " ERL_ERROR " %s\n", \
            (message), strerror((status))); \
        cantProceed((method)); \
    }
#endif

#if defined(DONT_USE_POSIX_THREAD_PRIORITY_SCHEDULING)
#undef _POSIX_THREAD_PRIO_INHERIT
#endif

#if defined(_WRS_VXWORKS_5_X) || (defined(_WRS_VXWORKS_MAJOR) && (_WRS_VXWORKS_MAJOR < 7))
#undef  PTHREAD_HAS_MUTEXATTR_SETTYPE
#else
#define PTHREAD_HAS_MUTEXATTR_SETTYPE
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
#if defined(PTHREAD_HAS_MUTEXATTR_SETTYPE)
    status = pthread_mutexattr_settype(&globalAttrRecursive, PTHREAD_MUTEX_RECURSIVE);
    checkStatusQuit(status, "pthread_mutexattr_settype(&globalAttrRecursive, PTHREAD_MUTEX_RECURSIVE)", "globalAttrInit");
#endif
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

static int doOsdPosixMutexInit (pthread_mutex_t *m, int internalType)
{
    pthread_mutexattr_t *atts;
    int status;

    status = pthread_once( &globalAttrInitOnce, globalAttrInit );
    checkStatusQuit(status,"pthread_once","epicsPosixMutexAttrInit");

    switch (internalType) {
        case 0:
            atts = &globalAttrDefault;
            break;
        case 1:
            atts = &globalAttrRecursive;
            break;
        default:
            return ENOTSUP;
    }
    return pthread_mutex_init(m, atts);
}


#if defined(PTHREAD_HAS_MUTEXATTR_SETTYPE)
int osdPosixMutexInit (pthread_mutex_t *m, int mutexType)
{
int internalType;
    switch ( mutexType ) {
        case PTHREAD_MUTEX_DEFAULT:
            internalType = 0;
            break;
        case PTHREAD_MUTEX_RECURSIVE:
            internalType = 1;
            break;
        default:
            return ENOTSUP;
    }
    return doOsdPosixMutexInit( m, internalType );
}
#endif

int osdPosixMutexInitDefault (pthread_mutex_t *m)
{
    return doOsdPosixMutexInit(m, 0);
}

int osdPosixMutexSupportsRecursive()
{
#if defined _POSIX_THREAD_PRIO_INHERIT
    int proto = -1;
    int ret = pthread_mutexattr_getprotocol(&globalAttrRecursive, &proto);
    if(ret) {
        return ret;
    } else {
        return ( proto==PTHREAD_PRIO_INHERIT ) ? 0 : -1;
    }
#else
    return ENOTSUP;
#endif
}
