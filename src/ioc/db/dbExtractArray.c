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
#include "dbAddr.h"
#define epicsExportSharedSymbols
#include "dbExtractArray.h"

void dbExtractArray(const dbAddr *paddr,
        void *pto, long nRequest, long no_elements, long offset, long increment)
{
    char *pdst = (char *) pto;
    char *psrc = (char *) paddr->pfield;
    long nUpperPart;
    short srcSize = paddr->field_size;
    short dstSize = srcSize;
    char isString = (paddr->field_type == DBF_STRING);

    if (nRequest > no_elements) nRequest = no_elements;
    if (isString && dstSize >= MAX_STRING_SIZE) dstSize = MAX_STRING_SIZE - 1;

    if (increment == 1) {
        nUpperPart = nRequest < no_elements - offset ? nRequest : no_elements - offset;
        memcpy(pdst, &psrc[offset * srcSize], dstSize * nUpperPart);
        if (nRequest > nUpperPart)
            memcpy(&pdst[dstSize * nUpperPart], psrc, dstSize * (nRequest - nUpperPart));
    } else {
        for (; nRequest > 0; nRequest--, pdst += srcSize, offset += increment) {
            offset %= no_elements;
            memcpy(pdst, &psrc[offset*srcSize], dstSize);
            if (isString) pdst[dstSize] = '\0';
        }
    }
}
