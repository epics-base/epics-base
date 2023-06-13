/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* src/libCom/adjustment.c */

/* Author: Peregrine McGehee */

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <epicsAssert.h>
#include "adjustment.h"

size_t adjustToWorstCaseAlignment(size_t size)
{
    union aline {
        /* largest primative types (so far...) */
        double dval;
        size_t uval;
        char *ptr;
    };

    /* assert that alignment size is a power of 2 */
    STATIC_ASSERT((sizeof(union aline) & (sizeof(union aline)-1))==0);

    /* round up to aligment size */
    size--;
    size |= sizeof(union aline)-1;
    size++;

    return size;
}
