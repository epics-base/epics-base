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
    int nextPut;
    int nextGet;
    int size;
    char *buffer;
}ringPvt;


epicsShareFunc ringId  epicsShareAPI ringCreate(int size)
{
    ringPvt *pring = mallocMustSucceed(sizeof(ringPvt),"ringCreate");
    pring->size = size + 1;
    pring->buffer = mallocMustSucceed(pring->size,"ringCreate");
    pring->nextGet = pring->nextPut = 0;
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
    int count;

    if (pring->nextGet <= pring->nextPut) {
        count = pring->nextPut - pring->nextGet;
        if (count < nbytes)
            nbytes = count;
        memcpy (value, &pring->buffer[pring->nextGet], nbytes);
        pring->nextGet += nbytes;
    }
    else {
        int nextGet;
        count = pring->size - pring->nextGet;
        if (count > nbytes)
            count = nbytes;
        memcpy (value, &pring->buffer[pring->nextGet], count);
        if ((nextGet = pring->nextGet + count) == pring->size) {
            int nLeft = nbytes - count;
            if (nLeft > pring->nextPut)
                nLeft = pring->nextPut;
            memcpy (value+count, &pring->buffer[0], nLeft);
            pring->nextGet = nLeft;
            nbytes = count + nLeft;
        }
        else {
            nbytes = count;
            pring->nextGet = nextGet;
        }
    }
    return nbytes;
}

epicsShareFunc int epicsShareAPI ringPut(ringId id, char *value,int nbytes)
{
    ringPvt *pring = (ringPvt *)id;
    int count;

    if (pring->nextPut < pring->nextGet) {
        count = pring->nextGet - pring->nextPut - 1;
        if (nbytes > count)
            nbytes = count;
        memcpy (&pring->buffer[pring->nextPut], value, nbytes);
        pring->nextPut += nbytes;
    }
    else if (pring->nextGet == 0) {
        count = pring->size - pring->nextPut - 1;
        if (nbytes > count)
            nbytes = count;
        memcpy (&pring->buffer[pring->nextPut], value, nbytes);
        pring->nextPut += nbytes;
    }
    else {
        int nextPut;
        count = pring->size - pring->nextPut;
        if (count > nbytes)
            count = nbytes;
        memcpy (&pring->buffer[pring->nextPut], value, count);
        if ((nextPut = pring->nextPut + count) == pring->size) {
            int nLeft = nbytes - count;
            if (nLeft > (pring->nextGet - 1))
                nLeft = pring->nextGet - 1;
            memcpy (&pring->buffer[0], value+count, nLeft);
            pring->nextPut = nLeft;
            nbytes = count + nLeft;
        }
        else {
            nbytes = count;
            pring->nextPut = nextPut;
        }
    }
    return nbytes;
}

epicsShareFunc void epicsShareAPI ringFlush(ringId id)
{
    ringPvt *pring = (ringPvt *)id;

    pring->nextGet = pring->nextPut = 0;
}

epicsShareFunc int epicsShareAPI ringFreeBytes(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    int n;

    n = pring->nextGet - pring->nextPut - 1;
    if (n < 0)
    	n += pring->size;
    return n;
}

epicsShareFunc int epicsShareAPI ringUsedBytes(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    int n;

    n = pring->nextPut - pring->nextGet;
    if (n < 0)
    	n += pring->size;
    return n;
}

epicsShareFunc int epicsShareAPI ringSize(ringId id)
{
    ringPvt *pring = (ringPvt *)id;

    return pring->size - 1;
}

epicsShareFunc int epicsShareAPI ringIsEmpty(ringId id)
{
    ringPvt *pring = (ringPvt *)id;

    return (pring->nextPut == pring->nextGet);
}

epicsShareFunc int epicsShareAPI ringIsFull(ringId id)
{
    ringPvt *pring = (ringPvt *)id;
    int count;

    count = (pring->nextPut - pring->nextGet) + 1;
    return ((count == 0) || (count == pring->size));
}
