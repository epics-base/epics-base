/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <windows.h>

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "dbDefs.h"
#include "errlog.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

static epicsUInt64 osdMonotonicResolution; /* timer resolution in nanoseconds */
static double perfCounterScale; /* convert performance counter tics to nanoseconds */

void osdMonotonicInit(void)
{
    LARGE_INTEGER freq, val;
    /* QueryPerformanceCounter() is available on Windows 2000 and later, and guaranteed
       to always succeed on Windows XP or later. On Windows 2000 it may
       return 0 for freq.QuadPart if unavailable */
    if(QueryPerformanceFrequency(&freq) &&
            QueryPerformanceCounter(&val) &&
            freq.QuadPart != 0)
    {
        perfCounterScale = 1e9 / freq.QuadPart;
        osdMonotonicResolution = 1 + (int)perfCounterScale;
    } else {
        errMessage(errlogMajor, "Windows Performance Counter is not available\n");
    }
}

epicsUInt64 epicsMonotonicResolution(void)
{
    return osdMonotonicResolution;
}

epicsUInt64 epicsMonotonicGet(void)
{
    LARGE_INTEGER val;
    if(!QueryPerformanceCounter(&val)) {
        errMessage(errlogMinor, "Warning: failed to fetch performance counter\n");
        return 0;
    } else {
        return val.QuadPart * perfCounterScale; /* return value in nanoseconds */
    }
}
