/*osiRing.h */

/* Author:  Marty Kraimer Date:    15JUL99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#ifndef INCosiRingh
#define INCosiRingh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

typedef void *ringId;

epicsShareFunc ringId  epicsShareAPI ringCreate(int nbytes);
epicsShareFunc void    epicsShareAPI ringDelete(ringId id);
epicsShareFunc int     epicsShareAPI ringGet(ringId id, char *value,int nbytes);
epicsShareFunc int     epicsShareAPI ringPut(ringId id, char *value,int nbytes);
epicsShareFunc void    epicsShareAPI ringFlush(ringId id);
epicsShareFunc int     epicsShareAPI ringFreeBytes(ringId id);
epicsShareFunc int     epicsShareAPI ringUsedBytes(ringId id);
epicsShareFunc int     epicsShareAPI ringSize(ringId id);
epicsShareFunc int     epicsShareAPI ringIsEmpty(ringId id);
epicsShareFunc int     epicsShareAPI ringIsFull(ringId id);

#ifdef __cplusplus
}
#endif

/* NOTES
    If there is only one writer it is not necessary to lock for put
    If there is a single reader it is not necessary to lock for puts
*/

#endif /* INCosiRingh */
