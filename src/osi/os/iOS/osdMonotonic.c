/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <mach/mach.h>
#include <mach/mach_time.h>

#define epicsExportSharedSymbols
#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "dbDefs.h"
#include "errlog.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

/* see https://developer.apple.com/library/content/qa/qa1398/_index.html */
static mach_timebase_info_data_t tbinfo;

void osdMonotonicInit(void)
{
    (void)mach_timebase_info(&tbinfo);
}

epicsUInt64 epicsMonotonicResolution(void)
{
    return 1e-9 * tbinfo.numer / tbinfo.denom;
}

epicsUInt64 epicsMonotonicGet(void)
{
    uint64_t val = mach_absolute_time();
    return val * tbinfo.numer / tbinfo.denom;
}
