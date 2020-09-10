/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "osiSock.h"

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
    int osdTimeGetCurrent (epicsTimeStamp *pDest)
    {
        struct timeval tv;
        struct timezone tz;

        if (gettimeofday (&tv, &tz))
            return errno;

        *pDest = epicsTime(tv);
        return epicsTimeOK;
    }
    } // extern "C"
#endif

#ifdef __CYGWIN__
int clock_settime(clockid_t clock, const timespec *tp)
{
    return -EFAULT;
}
#endif


static int timeRegister(void)
{
    TIME_INIT;

    osdMonotonicInit();
    return 1;
}
static int done = timeRegister();


int epicsTime_gmtime ( const time_t *pAnsiTime,
                       struct tm *pTM )
{
    struct tm * pRet = gmtime_r ( pAnsiTime, pTM );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return errno;
    }
}

int epicsTime_localtime ( const time_t *clock,
                          struct tm *result )
{
    struct tm * pRet = localtime_r ( clock, result );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return errno;
    }
}

extern "C" LIBCOM_API void
    convertDoubleToWakeTime(double timeout,struct timespec *wakeTime)
{
    struct timespec now, wait;
    int status;

    if (timeout < 0.0)
        timeout = 0.0;
    else if (timeout > 60 * 60 * 24 * 3652.5)
        timeout = 60 * 60 * 24 * 3652.5;    /* 10 years */

#ifdef CLOCK_REALTIME
    status = clock_gettime(CLOCK_REALTIME, &now);
#else
    {
        struct timeval tv;
        struct timezone tz;
        status = gettimeofday(&tv, &tz);
        now.tv_sec = tv.tv_sec;
        now.tv_nsec = tv.tv_usec * 1000;
    }
#endif
    if (status) {
        perror("convertDoubleToWakeTime");
        cantProceed("convertDoubleToWakeTime");
    }

    wait.tv_sec  = static_cast< time_t >(timeout);
    wait.tv_nsec = static_cast< long >((timeout - (double)wait.tv_sec) * 1e9);

    wakeTime->tv_sec  = now.tv_sec + wait.tv_sec;
    wakeTime->tv_nsec = now.tv_nsec + wait.tv_nsec;
    if (wakeTime->tv_nsec >= 1000000000L) {
        wakeTime->tv_nsec -= 1000000000L;
        ++wakeTime->tv_sec;
    }
}

#ifdef __rtems__
int osdTickGet()
{
    return epicsMonotonicGet(); // ns
}
int  osdTickRateGet()
{
    return 1000000000;
}
void osdNTPReport() {}
#endif
