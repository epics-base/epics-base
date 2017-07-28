/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*asTrapWrite.h*/
/* Author:  Marty Kraimer Date:    07NOV2000 */

#ifndef INCasTrapWriteh
#define INCasTrapWriteh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct asTrapWriteMessage {
    const char *userid;
    const char *hostid;
    void *serverSpecific;
    void *userPvt;
    int dbrType;    /* Data type from ca/db_access.h, NOT dbFldTypes.h */
    int no_elements;
    void *data;     /* Might be NULL if no data is available */
} asTrapWriteMessage;


typedef void *asTrapWriteId;
typedef void(*asTrapWriteListener)(asTrapWriteMessage *pmessage,int after);

epicsShareFunc asTrapWriteId epicsShareAPI asTrapWriteRegisterListener(
    asTrapWriteListener func);
epicsShareFunc void epicsShareAPI asTrapWriteUnregisterListener(
    asTrapWriteId id);

/*
 * asTrapWriteListener is called before and after the write is performed.
 * The listener can set userPvt on the before call and retrieve it after
 * after = (0,1) (before,after) the put.
 *
 * Each asTrapWriteMessage can change or may be deleted after
 * the user's asTrapWriteListener returns
 *
 * asTrapWriteListener delays the associated server thread so it must not
 * do anything that causes to to block.
*/

#ifdef __cplusplus
}
#endif

#endif /*INCasTrapWriteh*/
