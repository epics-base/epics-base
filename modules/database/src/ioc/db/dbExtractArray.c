/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 *
 *  based on dbConvert.c, see copyNoConvert
 *  written by: Bob Dalesio, Marty Kraimer
 */

#include <string.h>
#include <assert.h>

#include "epicsTypes.h"

#include "dbAddr.h"
#include "dbExtractArray.h"

void dbExtractArray(const void *pfrom, void *pto, short field_size,
    long nRequest, long no_elements, long offset, long increment)
{
    char *pdst = (char *) pto;
    const char *psrc = (char *) pfrom;

    /* assert preconditions */
    assert(nRequest >= 0);
    assert(no_elements >= 0);
    assert(increment > 0);
    assert(0 <= offset);
    assert(offset < no_elements);

    if (increment == 1) {
        long nUpperPart =
            nRequest < no_elements - offset ? nRequest : no_elements - offset;
        memcpy(pdst, psrc + (offset * field_size), field_size * nUpperPart);
        if (nRequest > nUpperPart)
            memcpy(pdst + (field_size * nUpperPart), psrc,
                field_size * (nRequest - nUpperPart));
    } else {
        for (; nRequest > 0; nRequest--, pdst += field_size, offset += increment) {
            offset %= no_elements;
            memcpy(pdst, psrc + (offset * field_size), field_size);
        }
    }
}
