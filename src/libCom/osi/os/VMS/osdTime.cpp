
//
// should move the gettimeofday() proto for VMS
// into a different header
//
#include "osiSock.h"

#define epicsExportSharedSymbols
#include "epicsTime.h"

//
// epicsTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int epicsTimeGetCurrent (epicsTimeStamp *pDest)
{
#   if defined(CLOCK_REALTIME)
        struct timespec ts;
        int status;
    
        status = clock_gettime (CLOCK_REALTIME, &ts);
        if (status) {
            return -1;
        }
        *pDest = epicsTime (ts);
        return 0;
#   else
    	int status;
    	struct timeval tv;
    
    	status = gettimeofday (&tv, NULL);
        if (status!=0) {
            return -1;
        }
    	*pDest = epicsTime (tv); 
        return 0;
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
    else {
        return epicsTimeERROR;
    }
}
