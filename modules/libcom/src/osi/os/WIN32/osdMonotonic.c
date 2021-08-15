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
#include "cantProceed.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

static epicsUInt64 osdMonotonicResolution;      /* timer resolution in nanoseconds */
static epicsUInt64 perfCounterFrequency;        /* performance counter tics per second */
static LONGLONG perfCounterOffset;              /* performance counter value at initialization */
static const epicsUInt64 sec2nsec = 1000000000; /* number of nanoseconds in a second */

void osdMonotonicInit(void)
{
    LARGE_INTEGER freq, val;
    /* QueryPerformanceCounter() is available on Windows 2000 and later, and is
     * guaranteed to succeed on Windows XP or later. On Windows 2000 it may
     * return 0 for freq.QuadPart if unavailable */
    if (QueryPerformanceFrequency(&freq) &&
            QueryPerformanceCounter(&val) &&
            freq.QuadPart != 0) {
        perfCounterFrequency = freq.QuadPart;
        perfCounterOffset = val.QuadPart;
        osdMonotonicResolution = sec2nsec / perfCounterFrequency +
            !!(sec2nsec % perfCounterFrequency);
    }
    else {
        cantProceed("osdMonotonicInit: Windows Performance Counter is not available\n");
    }
}

epicsUInt64 epicsMonotonicResolution(void)
{
    return osdMonotonicResolution;
}

epicsUInt64 epicsMonotonicGet(void)
{
    LARGE_INTEGER val;
    double dval;
    if (!QueryPerformanceCounter(&val)) {
        cantProceed("epicsMonotonicGet: Failed to read Windows Performance Counter\n");
        return 0;
    }
    dval = val.QuadPart - perfCounterOffset;
    return (epicsUInt64)(dval * sec2nsec / perfCounterFrequency + 0.5);
}
