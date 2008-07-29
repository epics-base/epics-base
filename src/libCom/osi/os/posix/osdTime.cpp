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
#include "cantProceed.h"

#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "generalTimeSup.h"

extern "C" {
static int osdTimeGetCurrent (epicsTimeStamp *pDest)
{
#   ifdef CLOCK_REALTIME
        struct timespec ts;
        int status;
    
        status = clock_gettime (CLOCK_REALTIME, &ts);
        if (status) {
            return epicsTimeERROR;
        }

        *pDest = epicsTime (ts);
        return epicsTimeOK;
#   else
        int status;
        struct timeval tv;
        struct timezone tz;
    
      status = gettimeofday (&tv, &tz);
        if (status) {
            return epicsTimeERROR;
        }
      *pDest = epicsTime (tv); 
        return epicsTimeOK;
#   endif
}
}

static int timeRegister(void)
{
    generalTimeCurrentTpRegister("Posix Time", 100, osdTimeGetCurrent);
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
#   ifdef CLOCK_REALTIME
    status = clock_gettime(CLOCK_REALTIME,wakeTime);
#   else
    {
    struct timeval tv;
    struct timezone tz;
    status = gettimeofday(&tv, &tz);
    wakeTime->tv_sec = tv.tv_sec;
    wakeTime->tv_nsec = tv.tv_usec * 1000;
    }
#   endif
    if(status) { 
        printf("clock_gettime failed with error %s\n",strerror(errno));
        cantProceed("convertDoubleToWakeTime"); 
    }
    wait.tv_sec = static_cast < long > ( timeout );
    wait.tv_nsec = static_cast < long > ( (timeout - (double)wait.tv_sec) * 1e9 );
    wakeTime->tv_sec += wait.tv_sec;
    wakeTime->tv_nsec += wait.tv_nsec;
    if(wakeTime->tv_nsec>=1000000000L) {
        wakeTime->tv_nsec -= 1000000000L;
        ++wakeTime->tv_sec;
    }
}
