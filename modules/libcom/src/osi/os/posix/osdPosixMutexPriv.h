/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef osdPosixMutexPrivh
#define osdPosixMutexPrivh

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns ENOTSUP if requested mutextype is not supported */
/* At the moment, only PTHREAD_MUTEX_DEFAULT and PTHREAD_MUTEX_RECURSIVE are supported */
int osdPosixMutexInit(pthread_mutex_t *,int mutextype);
int osdPosixMutexInitDefault(pthread_mutex_t *);

/* Returns 0 if supported and enabled, -1 if not enabled, ENOTSUP if
 * not supported or an error code if retrieving the mutex attribute
 * fails.
 */
int osdPosixMutexSupportsRecursive();

#ifdef __cplusplus
}
#endif

#endif /* osdPosixMutexPrivh */
