/*epicsRingBytes.h */

/* Author:  Eric Norum & Marty Kraimer Date:    15JUL99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#ifndef INCepicsRingBytesh
#define INCepicsRingBytesh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

typedef void *epicsRingBytesId;

epicsShareFunc epicsRingBytesId  epicsShareAPI epicsRingBytesCreate(int nbytes);
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
*/

#endif /* INCepicsRingBytesh */
