/*asTrapWrite.h*/
/* Author:  Marty Kraimer Date:    07NOV2000 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

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
