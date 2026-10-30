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
    union align {
        /* largest primitive types (so far...) */
        double dval;
        size_t uval;
        char *ptr;
    };

    /* assert that alignment size is a power of 2 */
    STATIC_ASSERT((sizeof(union align) & (sizeof(union align)-1))==0);

    /* round up to alignment size */
    size--;
    size |= sizeof(union align)-1;
    size++;

    return size;
}
