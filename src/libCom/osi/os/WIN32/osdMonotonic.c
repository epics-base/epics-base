/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <windows.h>

#define epicsExportSharedSymbols
#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "dbDefs.h"
#include "errlog.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

static unsigned char osdUsePrefCounter;
static epicsUInt64 osdMonotonicResolution;

void osdMonotonicInit(void)
{
    LARGE_INTEGER freq, val;

    if(!QueryPerformanceFrequency(&freq) ||
            !QueryPerformanceCounter(&val))
    {
        double period = 1.0/freq.QuadPart;
        osdMonotonicResolution = period*1e9;
        osdUsePrefCounter = 1;
    } else {
        osdMonotonicResolution = 1e6; /* 1 ms TODO place holder */
    }
}

epicsUInt64 epicsMonotonicResolution(void)
{
    return osdMonotonicResolution;
}

epicsUInt64 epicsMonotonicGet(void)
{
    LARGE_INTEGER val;
    if(osdUsePrefCounter) {
        if(!QueryPerformanceCounter(&val)) {
            errMessage(errlogMinor, "Warning: failed to fetch performance counter\n");
            return 0;
        } else
            return val.QuadPart;
    } else {
        epicsUInt64 ret = GetTickCount();
        ret *= 1000000;
        return ret;
    }
}
