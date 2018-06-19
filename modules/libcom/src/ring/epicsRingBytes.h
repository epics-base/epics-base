/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2012 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author:  Marty Kraimer Date:    15JUL99
 *          Eric Norum
 *          Ralph Lange <Ralph.Lange@gmx.de>
 */

#ifndef INCepicsRingBytesh
#define INCepicsRingBytesh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

typedef void *epicsRingBytesId;

epicsShareFunc epicsRingBytesId  epicsShareAPI epicsRingBytesCreate(int nbytes);
/* Same, but secured by a spinlock */
epicsShareFunc epicsRingBytesId  epicsShareAPI epicsRingBytesLockedCreate(int nbytes);
epicsShareFunc void epicsShareAPI epicsRingBytesDelete(epicsRingBytesId id);
epicsShareFunc int  epicsShareAPI epicsRingBytesGet(
    epicsRingBytesId id, char *value,int nbytes);
epicsShareFunc int  epicsShareAPI epicsRingBytesPut(
    epicsRingBytesId id, char *value,int nbytes);
epicsShareFunc void epicsShareAPI epicsRingBytesFlush(epicsRingBytesId id);
epicsShareFunc int  epicsShareAPI epicsRingBytesFreeBytes(epicsRingBytesId id);
epicsShareFunc int  epicsShareAPI epicsRingBytesUsedBytes(epicsRingBytesId id);
epicsShareFunc int  epicsShareAPI epicsRingBytesSize(epicsRingBytesId id);
epicsShareFunc int  epicsShareAPI epicsRingBytesIsEmpty(epicsRingBytesId id);
epicsShareFunc int  epicsShareAPI epicsRingBytesIsFull(epicsRingBytesId id);

#ifdef __cplusplus
}
#endif

/* NOTES
    If there is only one writer it is not necessary to lock for put
    If there is a single reader it is not necessary to lock for puts

    epicsRingBytesLocked uses a spinlock.
*/

#endif /* INCepicsRingBytesh */
