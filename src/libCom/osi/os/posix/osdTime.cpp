
//
// should move the gettimeofday() proto 
// into a different header
//

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "osiSock.h"
#include "cantProceed.h"

#define epicsExportSharedSymbols
#include "epicsTime.h"


//
// epicsTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetCurrent (epicsTimeStamp *pDest)
{
        struct timespec ts;
        int status;
    
        status = clock_gettime (CLOCK_REALTIME, &ts);
        if (status) {
            return epicsTimeERROR;
        }

        *pDest = epicsTime (ts);
        return epicsTimeOK;
}

//
// epicsTimeGetEvent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetEvent (epicsTimeStamp *pDest, unsigned eventNumber)
{
    if (eventNumber==epicsTimeEventCurrentTime) {
        return epicsTimeGetCurrent (pDest);
    }
    return epicsTimeERROR;
}

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

extern "C" epicsShareFunc void epicsShareAPI
    convertDoubleToWakeTime(double timeout,struct timespec *wakeTime)
{
    struct timespec wait;
    int status;

    if(timeout<0.0) timeout = 0.0;
    else if(timeout>3600.0) timeout = 3600.0;
    status = clock_gettime(CLOCK_REALTIME,wakeTime);
    if(status) { 
        printf("clock_gettime failed with error %s\n",strerror(status));
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
