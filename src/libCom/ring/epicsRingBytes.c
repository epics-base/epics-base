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

typedef struct ringPvt {
    int nextPut;
    int nextGet;
    int          size;
    char         *buffer;
}ringPvt;


epicsShareFunc epicsRingBytesId  epicsShareAPI epicsRingBytesCreate(int size)
{
    ringPvt *pring = mallocMustSucceed(sizeof(ringPvt),"epicsRingBytesCreate");
    pring->size = size + 1;
    pring->buffer = mallocMustSucceed(pring->size,"ringCreate");
    pring->nextGet = pring->nextPut = 0;
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
        memcpy (value, &pring->buffer[nextGet], nbytes);
        nextGet += nbytes;
    }
    else {
        count = size - nextGet;
        if (count > nbytes)
            count = nbytes;
        memcpy (value, &pring->buffer[nextGet], count);
        if ((nextGet = nextGet + count) == size) {
            int nLeft = nbytes - count;
            if (nLeft > nextPut)
                nLeft = nextPut;
            memcpy (value+count, &pring->buffer[0], nLeft);
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
    int count;

    if (nextPut < nextGet) {
        count = nextGet - nextPut - 1;
        if (nbytes > count)
            nbytes = count;
        memcpy (&pring->buffer[nextPut], value, nbytes);
        nextPut += nbytes;
    }
    else if (nextGet == 0) {
        count = size - nextPut - 1;
        if (nbytes > count)
            nbytes = count;
        memcpy (&pring->buffer[nextPut], value, nbytes);
        nextPut += nbytes;
    }
    else {
        count = size - nextPut;
        if (count > nbytes)
            count = nbytes;
        memcpy (&pring->buffer[nextPut], value, count);
        if ((nextPut = nextPut + count) == size) {
            int nLeft = nbytes - count;
            if (nLeft > (nextGet - 1))
                nLeft = nextGet - 1;
            memcpy (&pring->buffer[0], value+count, nLeft);
            nextPut = nLeft;
            nbytes = count + nLeft;
        }
        else {
            nbytes = count;
            nextPut = nextPut;
        }
    }
    pring->nextPut = nextPut;
    return nbytes;
}

epicsShareFunc void epicsShareAPI epicsRingBytesFlush(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;

    pring->nextGet = pring->nextPut = 0;
}

epicsShareFunc int epicsShareAPI epicsRingBytesFreeBytes(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;
    int n;

    n = pring->nextGet - pring->nextPut - 1;
    if (n < 0)
        n += pring->size;
    return n;
}

epicsShareFunc int epicsShareAPI epicsRingBytesUsedBytes(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;
    int n;

    n = pring->nextPut - pring->nextGet;
    if (n < 0)
        n += pring->size;
    return n;
}

epicsShareFunc int epicsShareAPI epicsRingBytesSize(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;

    return pring->size - 1;
}

epicsShareFunc int epicsShareAPI epicsRingBytesIsEmpty(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;

    return (pring->nextPut == pring->nextGet);
}

epicsShareFunc int epicsShareAPI epicsRingBytesIsFull(epicsRingBytesId id)
{
    ringPvt *pring = (ringPvt *)id;
    int count;

    count = (pring->nextPut - pring->nextGet) + 1;
    return ((count == 0) || (count == pring->size));
}
