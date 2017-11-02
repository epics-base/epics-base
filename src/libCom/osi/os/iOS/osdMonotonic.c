/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#import <mach/mach_time.h>

#define epicsExportSharedSymbols
#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "dbDefs.h"
#include "errlog.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

void osdMonotonicInit(void)
{
    /* no-op */
}

epicsUInt64 epicsMonotonicResolution(void)
{
    return 1; /* TODO, how to find ? */
}

epicsUInt64 epicsMonotonicGet(void)
{
    uint64_t val = mach_absolute_time(), ret;
    absolutetime_to_nanoseconds(val, &ret);
    return ret;
}
