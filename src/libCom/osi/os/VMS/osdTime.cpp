/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// should move the gettimeofday() proto for VMS
// into a different header
//
#include "osiSock.h"

#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "generalTimeSup.h"

//
// epicsTime::osdGetCurrent ()
//
int osdTimeGetCurrent (epicsTimeStamp *pDest)
{
    int status;
#if defined(CLOCK_REALTIME)
    struct timespec t;
    status = clock_gettime(CLOCK_REALTIME, &t);
#else
    struct timeval t;
    status = gettimeofday(&t, NULL);
#endif
    if (status) {
        return epicsTimeERROR;
    }
    *pDest = epicsTime(t);
    return epicsTimeOK;
}

static int timeRegister(void)
{
#if defined (CLOCK_REALTIME)
    const char *name = "gettimeofday";
#else
    const char *name = "clock_gettime";
#endif

    generalTimeCurrentTpRegister(name, 150, osdTimeGetCurrent);
    return 1;
}
static int done = timeRegister();
