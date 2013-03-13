/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "osiSock.h"

#define epicsExportSharedSymbols
#include "cantProceed.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

#ifdef CLOCK_REALTIME
    #include "osiClockTime.h"

    #define TIME_INIT ClockTime_Init(CLOCKTIME_NOSYNC)
#else
    /* Some posix systems may not have CLOCK_REALTIME */

    #define TIME_INIT generalTimeCurrentTpRegister("GetTimeOfDay", \
        LAST_RESORT_PRIORITY, osdTimeGetCurrent)

    extern "C" {
    static int osdTimeGetCurrent (epicsTimeStamp *pDest)
    {
        struct timeval tv;
        struct timezone tz;

        if (gettimeofday (&tv, &tz))
            return epicsTimeERROR;

        *pDest = epicsTime(tv);
        return epicsTimeOK;
    }
    } // extern "C"
#endif

#ifdef CYGWIN32
int clock_settime(clockid_t clock, const timespec *tp)
{
    return -EFAULT;
}
#endif


static int timeRegister(void)
{
    TIME_INIT;
    return 1;
}
static int done = timeRegister();


int epicsTime_gmtime ( const time_t *pAnsiTime, // X aCC 361
                       struct tm *pTM )
{
    struct tm * pRet = gmtime_r ( pAnsiTime, pTM );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return epicsTimeERROR;
    }
}

int epicsTime_localtime ( const time_t *clock, // X aCC 361
                          struct tm *result )
{
    struct tm * pRet = localtime_r ( clock, result );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return epicsTimeERROR;
    }
}

extern "C" epicsShareFunc void
    convertDoubleToWakeTime(double timeout,struct timespec *wakeTime)
{
    struct timespec wait;
    int status;

    if(timeout<0.0) timeout = 0.0;
    else if(timeout>3600.0) timeout = 3600.0;
#ifdef CLOCK_REALTIME
    status = clock_gettime(CLOCK_REALTIME, wakeTime);
#else
    {
        struct timeval tv;
        struct timezone tz;
        status = gettimeofday(&tv, &tz);
        wakeTime->tv_sec = tv.tv_sec;
        wakeTime->tv_nsec = tv.tv_usec * 1000;
    }
#endif
    if (status) {
        perror("convertDoubleToWakeTime");
        cantProceed("convertDoubleToWakeTime");
    }
    wait.tv_sec  = static_cast< long >(timeout);
    wait.tv_nsec = static_cast< long >((timeout - (double)wait.tv_sec) * 1e9);
    wakeTime->tv_sec  += wait.tv_sec;
    wakeTime->tv_nsec += wait.tv_nsec;
    if (wakeTime->tv_nsec >= 1000000000L) {
        wakeTime->tv_nsec -= 1000000000L;
        ++wakeTime->tv_sec;
    }
}
