/*************************************************************************\
* Copyright (c) 2015 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "dbDefs.h"
#include "errlog.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

static clockid_t osdMonotonicID;
static epicsUInt64 osdMonotonicResolution;

void osdMonotonicInit(void)
{
    unsigned i;
clockid_t ids[] = {
#ifdef CLOCK_HIGHRES
        CLOCK_HIGHRES, /* solaris specific */
#endif
#ifdef CLOCK_MONOTONIC
        CLOCK_MONOTONIC, /* Linux, RTEMS, and probably others */
#endif
    /* fallback and vxWorks, not actually monotonic, but always available */
        CLOCK_REALTIME
    };

    for(i=0; i<NELEMENTS(ids); i++) {
        epicsUInt64 res;
        struct timespec ts;
        int ret = clock_getres(ids[i], &ts);
        if(ret) continue;

        res  = ts.tv_sec;
        res *= 1000000000;
        res += ts.tv_nsec;

        /* fetch the time once to see that it actually succeeds */
        ret = clock_gettime(ids[i], &ts);
        if(ret) continue;

        osdMonotonicID = ids[i];
        osdMonotonicResolution = res;
        return;
    }

    errMessage(errlogMinor, "Warning: failed to setup monotonic time source");
}

epicsUInt64 epicsMonotonicResolution(void)
{
    return osdMonotonicResolution;
}

epicsUInt64 epicsMonotonicGet(void)
{
    struct timespec ts;
    int ret = clock_gettime(osdMonotonicID, &ts);
    if(ret) {
        errlogPrintf("Warning: failed to fetch monotonic time %d %d\n",
                     (int)osdMonotonicID, ret);
        return 0;
    } else {
        epicsUInt64 ret = ts.tv_sec;
        ret *= 1000000000;
        ret += ts.tv_nsec;
        return ret;
    }
}
