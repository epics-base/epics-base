/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
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
#include <sys/time.h>

#ifndef CLOCK_REALTIME
  #include <mach/mach.h>
  #include <mach/clock.h>

  static clock_serv_t host_clock;

  #define HOST_GETCLOCK host_get_clock_service(mach_host_self(), \
              CALENDAR_CLOCK, &host_clock)
  #define TIMESPEC mach_timespec_t
  #define CLOCK_GETTIME(ts) clock_get_time(host_clock, ts)
  #define TP_NAME "MachTime"
#else
  #define HOST_GETCLOCK
  #define TIMESPEC struct timespec
  #define CLOCK_GETTIME(ts) clock_gettime(CLOCK_REALTIME, ts)
  #define TP_NAME "macOS Clock"
#endif

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include "osiSock.h"

#include "cantProceed.h"
#include "epicsTime.h"
#include "generalTimeSup.h"

extern "C" {
int osdTimeGetCurrent (epicsTimeStamp *pDest)
{
    struct timespec ts;

#ifndef CLOCK_REALTIME
    TIMESPEC mts;
    CLOCK_GETTIME(&mts);
    ts.tv_sec = mts.tv_sec;
    ts.tv_nsec = mts.tv_nsec;
#else
    CLOCK_GETTIME(&ts);
#endif
    *pDest = epicsTime(ts);
    return epicsTimeOK;
}
} // extern "C"


static int timeRegister(void)
{
    HOST_GETCLOCK;

    generalTimeCurrentTpRegister(TP_NAME, \
        LAST_RESORT_PRIORITY, osdTimeGetCurrent);

    osdMonotonicInit();
    return 1;
}
static int done = timeRegister();


int epicsTime_gmtime(const time_t *pAnsiTime, struct tm *pTM)
{
    return gmtime_r(pAnsiTime, pTM) ?
        epicsTimeOK : errno;
}

int epicsTime_localtime(const time_t *clock, struct tm *result)
{
    return localtime_r(clock, result) ?
        epicsTimeOK : errno;
}

extern "C" LIBCOM_API void
convertDoubleToWakeTime(double timeout, struct timespec *wakeTime)
{
    TIMESPEC now;
    struct timespec wait;

    if (timeout < 0.0)
        timeout = 0.0;
    else if (timeout > 60 * 60 * 24 * 3652.5)
        timeout = 60 * 60 * 24 * 3652.5;    /* 10 years */

    CLOCK_GETTIME(&now);

    wait.tv_sec  = static_cast< time_t >(timeout);
    wait.tv_nsec = static_cast< long >((timeout - (double)wait.tv_sec) * 1e9);

    wakeTime->tv_sec  = now.tv_sec + wait.tv_sec;
    wakeTime->tv_nsec = now.tv_nsec + wait.tv_nsec;
    if (wakeTime->tv_nsec >= 1000000000L) {
        wakeTime->tv_nsec -= 1000000000L;
        ++wakeTime->tv_sec;
    }
}
