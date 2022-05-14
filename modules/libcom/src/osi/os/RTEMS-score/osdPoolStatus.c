/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#include <rtems/malloc.h>

#include "osiPoolStatus.h"

/*
 * osiSufficentSpaceInPool ()
 */
LIBCOM_API int epicsStdCall osiSufficentSpaceInPool ( size_t contiguousBlockSize )
{
    rtems_malloc_statistics_t s;
    unsigned long n;

    malloc_get_statistics(&s);
    n = s.space_available - (unsigned long)(s.lifetime_allocated - s.lifetime_freed);
    return (n > (50000 + contiguousBlockSize));
}
