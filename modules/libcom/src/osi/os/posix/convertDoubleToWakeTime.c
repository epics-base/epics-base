/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <time.h>
#include "cantProceed.h"

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
