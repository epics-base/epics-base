
//
// should move the gettimeofday() proto 
// into a different header
//
#include "osiSock.h"

#define epicsExportSharedSymbols
#include "epicsTime.h"


//
// epicsTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetCurrent (epicsTimeStamp *pDest)
{
#   ifdef _POSIX_TIMERS
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
    
    	status = gettimeofday (&tv, NULL);
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
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetEvent (epicsTimeStamp *pDest, unsigned eventNumber)
{
    if (eventNumber==epicsTimeEventCurrentTime) {
        return epicsTimeGetCurrent (pDest);
    }
    return epicsTimeERROR;
}

int epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    struct tm * pRet = gmtime_r ( pAnsiTime, pTM );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return epicsTimeERROR;
    }
}

int epicsTime_localtime ( const time_t *clock, struct tm *result )
{
    struct tm * pRet = localtime_r ( clock, result );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return epicsTimeERROR;
    }
}

