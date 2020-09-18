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

static unsigned char osdUsePrefCounter;
static epicsUInt64 osdMonotonicResolution; /* timer resolution in nanoseconds */
static double perfCounterScale; /* convert performance counter tics to nanoseconds */

void osdMonotonicInit(void)
{
    LARGE_INTEGER freq, val;

    if(QueryPerformanceFrequency(&freq) &&
            QueryPerformanceCounter(&val))
    {
        perfCounterScale = 1e9 / freq.QuadPart;
        osdMonotonicResolution = 1 + (int)perfCounterScale;
        osdUsePrefCounter = 1;
    } else {
        osdMonotonicResolution = 1e6; /* 1 ms TODO place holder */
#if _WIN32_WINNT < 0x0600 /* Older than Windows Vista / Server 2008 */
        errMessage(errlogMinor, "Warning: using GetTickCount() so monotonic time will wrap every 49.7 days\n");
#endif
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
            return val.QuadPart * perfCounterScale;
    } else {
        epicsUInt64 ret =
#if _WIN32_WINNT >= 0x0600 /* Windows Vista / Server 2008 and later */
                   GetTickCount64();
#else
                   GetTickCount(); /* this wraps every 49.7 days */
#endif /* _WIN32_WINNT >= 0x0600 */
        ret *= 1000000;
        return ret;
    }
}
