/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 *
 *  based on dbConvert.c
 *  written by: Bob Dalesio, Marty Kraimer
 */

#include <string.h>

#include "epicsTypes.h"

#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "dbExtractArray.h"

void dbExtractArrayFromRec(const dbAddr *paddr, void *pto,
                           long nRequest, long no_elements, long offset, long increment)
{
    char *pdst = (char *) pto;
    char *psrc = (char *) paddr->pfield;
    long nUpperPart;
    int i;
    short srcSize = paddr->field_size;
    short dstSize = srcSize;
    char isString = (paddr->field_type == DBF_STRING);

    if (nRequest > no_elements) nRequest = no_elements;
    if (isString && srcSize > MAX_STRING_SIZE) dstSize = MAX_STRING_SIZE;

    if (increment == 1 && dstSize == srcSize) {
        nUpperPart = nRequest < no_elements - offset ? nRequest : no_elements - offset;
        memcpy(pdst, &psrc[offset * srcSize], dstSize * nUpperPart);
        if (nRequest > nUpperPart)
            memcpy(&pdst[dstSize * nUpperPart], psrc, dstSize * (nRequest - nUpperPart));
        if (isString)
            for (i = 1; i <= nRequest; i++)
                pdst[dstSize*i-1] = '\0';
    } else {
        for (; nRequest > 0; nRequest--, pdst += dstSize, offset += increment) {
            offset %= no_elements;
            memcpy(pdst, &psrc[offset*srcSize], dstSize);
            if (isString) pdst[dstSize-1] = '\0';
        }
    }
}

void dbExtractArrayFromBuf(const void *pfrom, void *pto,
                           short field_size, short field_type,
                           long nRequest, long no_elements, long offset, long increment)
{
    char *pdst = (char *) pto;
    char *psrc = (char *) pfrom;
    int i;
    short srcSize = field_size;
    short dstSize = srcSize;
    char isString = (field_type == DBF_STRING);

    if (nRequest > no_elements) nRequest = no_elements;
    if (offset > no_elements - 1) offset = no_elements - 1;
    if (isString && dstSize >= MAX_STRING_SIZE) dstSize = MAX_STRING_SIZE - 1;

    if (increment == 1) {
        memcpy(pdst, &psrc[offset * srcSize], dstSize * nRequest);
        if (isString)
            for (i = 1; i <= nRequest; i++)
                pdst[dstSize*i] = '\0';
    } else {
        for (; nRequest > 0; nRequest--, pdst += srcSize, offset += increment) {
            memcpy(pdst, &psrc[offset*srcSize], dstSize);
            if (isString) pdst[dstSize] = '\0';
        }
    }
}
