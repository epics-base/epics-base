/*
 * $Id$
 *
 * Author: Eric Norum
 */

#include "osiSock.h"

#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsMutex.h"

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

