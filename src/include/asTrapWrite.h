/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
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
    char *userid;
    char *hostid;
    void *serverSpecific;
    void *userPvt;
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
*/

#ifdef __cplusplus
}
#endif

#endif /*INCasTrapWriteh*/
