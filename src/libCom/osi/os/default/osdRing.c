/* $Id$ */

/* Author:  Marty Kraimer Date:    15JUL99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "cantProceed.h"
#include "osiInterrupt.h"
#include "osiRing.h"


typedef struct ringPvt {
    int size;
    int nextGet;
    int nextPut;
    int isFull;
    char *buffer;
}ringPvt;

epicsShareFunc ringId  epicsShareAPI ringCreate(int size)
{
    ringPvt *pring = callocMustSucceed(1,sizeof(ringPvt),"ringCreate");
    pring->size = size;
    pring->buffer = callocMustSucceed(size,sizeof(char),"ringCreate");
    return((void *)pring);
}

epicsShareFunc void epicsShareAPI ringDelete(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    free((void *)pring->buffer);
    free((void *)pring);
}

epicsShareFunc int epicsShareAPI ringGet(ringId id, char *value,int nbytes)
{
    ringPvt *pring = (ringPvt *)id;
    int nextGet = pring->nextGet;
    int nextPut = pring->nextPut;
    int size = pring->size;
    int nToGet;
    int lockKey;

    if(nextGet < nextPut) {
        if(nbytes > (nextPut - nextGet)) return(0);
        memcpy(value,&pring->buffer[nextGet],nbytes);
        nextGet += nbytes;
        if(nextGet>=size) nextGet = 0;
        lockKey = interruptLock();
        pring->nextGet = nextGet;
        pring->isFull = 0;
        interruptUnlock(lockKey);
        return(nbytes);
    }
    if(nextGet==nextPut && !pring->isFull ) return(0);
    if(nbytes > (nextPut + (size - nextGet))) return(0);
    nToGet = size - nextGet;
    if(nToGet>nbytes) nToGet = nbytes;
    memcpy(value,&pring->buffer[nextGet],nToGet);
    value += nToGet;
    nextGet += nToGet;
    nToGet = nbytes - nToGet;
    if(nToGet>0) {
        nextGet = 0;
        memcpy(value,&pring->buffer[0],nToGet);
        nextGet += nToGet;
    } 
    if(nextGet>=size) nextGet = 0;
    lockKey = interruptLock();
    pring->nextGet = nextGet;
    pring->isFull = 0;
    interruptUnlock(lockKey);
    return(nbytes);
}

epicsShareFunc int epicsShareAPI ringPut(ringId id, char *value,int nbytes)
{
    ringPvt *pring = (ringPvt *)id;
    int nextGet = pring->nextGet;
    int nextPut = pring->nextPut;
    int size = pring->size;
    int nToPut;
    int lockKey;

    if(nextPut < nextGet) {
        if(nbytes > (nextGet - nextPut)) return(0);
        memcpy(&pring->buffer[nextPut],value,nbytes);
        nextPut += nbytes;
        if(nextPut>=size) nextPut = 0;
        lockKey = interruptLock();
        pring->nextPut = nextPut;
        if(nextPut==nextGet) pring->isFull = 1;
        interruptUnlock(lockKey);
        return(nbytes);
    }
    if(nextGet==nextPut && pring->isFull ) return(0);
    if(nbytes > (nextGet + (size - nextPut))) return(0);
    nToPut = size - nextPut;
    if(nToPut>nbytes) nToPut = nbytes;
    memcpy(&pring->buffer[nextPut],value,nToPut);
    value += nToPut;
    nextPut += nToPut;
    nToPut = nbytes - nToPut;
    if(nToPut>0) {
        nextPut = 0;
        memcpy(&pring->buffer[0],value,nToPut);
        nextPut += nToPut;
    }
    if(nextPut>=size) nextPut = 0;
    lockKey = interruptLock();
    pring->nextPut = nextPut;
    if(nextPut==nextGet) pring->isFull = 1;
    interruptUnlock(lockKey);
    return(nbytes);
}

epicsShareFunc void epicsShareAPI ringFlush(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    int lockKey;

    lockKey = interruptLock();
    pring->nextGet = pring->nextPut = 0;
    pring->isFull = 0;
    interruptUnlock(lockKey);
}

epicsShareFunc int epicsShareAPI ringFreeBytes(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    int n;
    int lockKey;

    lockKey = interruptLock();
    n = pring->nextGet - pring->nextPut;
    if(n<=0) n += pring->size;
    if(pring->isFull) n = 0;
    interruptUnlock(lockKey);
    return(n);
}

epicsShareFunc int epicsShareAPI ringUsedBytes(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    int n;
    int lockKey;

    lockKey = interruptLock();
    n = pring->nextPut - pring->nextGet;
    if(n<0) n += pring->size;
    if(pring->isFull) n = pring->size;
    interruptUnlock(lockKey);
    return(n);
}

epicsShareFunc int epicsShareAPI ringSize(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    return(pring->size);
}

epicsShareFunc int epicsShareAPI ringIsEmpty(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    int lockKey;
    int result;

    lockKey = interruptLock();
    result = (pring->nextGet==pring->nextPut && !pring->isFull) ? TRUE : FALSE;
    interruptUnlock(lockKey);
    return(result);
}

epicsShareFunc int epicsShareAPI ringIsFull(ringId id)
{
    ringPvt *pring = (ringPvt *)id;

    return(pring->isFull);
}
