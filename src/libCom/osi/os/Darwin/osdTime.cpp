/*
 * $Id$
 *
 * Author: Eric Norum
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "osiSock.h"
#include "cantProceed.h"

#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsMutex.h"


//
// epicsTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetCurrent (epicsTimeStamp *pDest)
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

//
// epicsTimeGetEvent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetEvent (epicsTimeStamp *pDest, int eventNumber)
{
    if (eventNumber==epicsTimeEventCurrentTime) {
        return epicsTimeGetCurrent (pDest);
    }
    return epicsTimeERROR;
}

/*
 * Darwin provides neither gmtime_r nor localtime_r and I can't confirm
 * the gmtime and localtime are thread-safe, so protect calls to these
 * functions with a mutex.
 */
static epicsMutexId timeLock;
static epicsThreadOnceId osdTimeOnceFlag = EPICS_THREAD_ONCE_INIT;
static void osdTimeInit ( void * )
{
    timeLock = epicsMutexMustCreate();
}

int epicsTime_gmtime ( const time_t *pAnsiTime, // X aCC 361
                       struct tm *pTM )
{
    struct tm * pRet;
    epicsThreadOnce ( &osdTimeOnceFlag, osdTimeInit, 0 );
    epicsMutexLock(timeLock);
    pRet = gmtime ( pAnsiTime );
    if ( pRet ) {
        *pTM = *pRet;
        epicsMutexUnlock(timeLock);
        return epicsTimeOK;
    }
    else {
        epicsMutexUnlock(timeLock);
        return epicsTimeERROR;
    }
}

int epicsTime_localtime ( const time_t *clock, // X aCC 361
                          struct tm *result )
{
    struct tm * pRet;
    epicsThreadOnce ( &osdTimeOnceFlag, osdTimeInit, 0 );
    epicsMutexLock(timeLock);
    pRet = localtime ( clock );
    if ( pRet ) {
        *result = *pRet;
        epicsMutexUnlock(timeLock);
        return epicsTimeOK;
    }
    else {
        epicsMutexUnlock(timeLock);
        return epicsTimeERROR;
    }
}

extern "C" epicsShareFunc void epicsShareAPI
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
    wait.tv_sec = timeout;
    wait.tv_nsec = (long)((timeout - (double)wait.tv_sec) * 1e9);
    wakeTime->tv_sec += wait.tv_sec;
    wakeTime->tv_nsec += wait.tv_nsec;
    if(wakeTime->tv_nsec>1000000000L) {
        wakeTime->tv_nsec -= 1000000000L;
        ++wakeTime->tv_sec;
    }
}
