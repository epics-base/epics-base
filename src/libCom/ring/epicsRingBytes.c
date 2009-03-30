/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsRingBytes.cd */

/* Author:  Eric Norum & Marty Kraimer Date:    15JUL99 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "cantProceed.h"
#include "epicsRingBytes.h"

/*
 * Need at least one extra byte to be able to distinguish a completely
 * full buffer from a completely empty one.  Allow for a little extra
 * space to try and keep good alignment and avoid multiple calls to
 * memcpy for a single put/get operation.
 */
#define SLOP    16

typedef struct ringPvt {
    volatile int   nextPut;
    volatile int   nextGet;
    int            size;
    volatile char *buffer;
}ringPvt;

epicsShareFunc epicsRingBytesId  epicsShareAPI epicsRingBytesCreate(int size)
{
    ringPvt *pring = mallocMustSucceed(sizeof(ringPvt),"epicsRingBytesCreate");
    pring->size = size + SLOP;
    pring->buffer = mallocMustSucceed(pring->size,"ringCreate");
    pring->nextGet = 0;
    pring->nextPut = 0;
    return((void *)pring);
}

epicsShareFunc void epicsShareAPI epicsRingBytesDelete(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;
    free((void *)pring->buffer);
    free((void *)pring);
}

epicsShareFunc int epicsShareAPI epicsRingBytesGet(
    epicsRingBytesId id, char *value,int nbytes)
{
    ringPvt *pring = (ringPvt *)id;
    int nextGet = pring->nextGet;
    int nextPut = pring->nextPut;
    int size = pring->size;
    int count;

    if (nextGet <= nextPut) {
        count = nextPut - nextGet;
        if (count < nbytes)
            nbytes = count;
        if (nbytes)
            memcpy (value, (void *)&pring->buffer[nextGet], nbytes);
        nextGet += nbytes;
    }
    else {
        count = size - nextGet;
        if (count > nbytes)
            count = nbytes;
        memcpy (value, (void *)&pring->buffer[nextGet], count);
        nextGet += count;
        if (nextGet == size) {
            int nLeft = nbytes - count;
            if (nLeft > nextPut)
                nLeft = nextPut;
            memcpy (value+count, (void *)&pring->buffer[0], nLeft);
            nextGet = nLeft;
            nbytes = count + nLeft;
        }
        else {
            nbytes = count;
        }
    }
    pring->nextGet = nextGet;
    return nbytes;
}

epicsShareFunc int epicsShareAPI epicsRingBytesPut(
    epicsRingBytesId id, char *value,int nbytes)
{
    ringPvt *pring = (ringPvt *)id;
    int nextGet = pring->nextGet;
    int nextPut = pring->nextPut;
    int size = pring->size;
    int freeCount, copyCount, topCount;

    if (nextPut < nextGet) {
        freeCount = nextGet - nextPut - SLOP;
        if (nbytes > freeCount)
            return 0;
        if (nbytes)
            memcpy ((void *)&pring->buffer[nextPut], value, nbytes);
        nextPut += nbytes;
    }
    else {
        freeCount = size - nextPut + nextGet - SLOP;
        if (nbytes > freeCount)
            return 0;
        topCount = size - nextPut;
        copyCount = (nbytes > topCount) ?  topCount : nbytes;
        if (copyCount)
            memcpy ((void *)&pring->buffer[nextPut], value, copyCount);
        nextPut += copyCount;
        if (nextPut == size) {
            int nLeft = nbytes - copyCount;
            if (nLeft)
                memcpy ((void *)&pring->buffer[0], value+copyCount, nLeft);
            nextPut = nLeft;
        }
    }
    pring->nextPut = nextPut;
    return nbytes;
}

epicsShareFunc void epicsShareAPI epicsRingBytesFlush(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;

    pring->nextGet = pring->nextPut;
}

epicsShareFunc int epicsShareAPI epicsRingBytesFreeBytes(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;
    int nextGet = pring->nextGet;
    int nextPut = pring->nextPut;

    if (nextPut < nextGet)
        return nextGet - nextPut - SLOP;
    else
        return pring->size - nextPut + nextGet - SLOP;
}

epicsShareFunc int epicsShareAPI epicsRingBytesUsedBytes(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;

    return pring->size - epicsRingBytesFreeBytes(id) - SLOP;
}

epicsShareFunc int epicsShareAPI epicsRingBytesSize(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;

    return pring->size - SLOP;
}

epicsShareFunc int epicsShareAPI epicsRingBytesIsEmpty(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;

    return (pring->nextPut == pring->nextGet);
}

epicsShareFunc int epicsShareAPI epicsRingBytesIsFull(epicsRingBytesId id)
{
    return (epicsRingBytesFreeBytes(id) <= 0);
}
