
//
// should move the gettimeofday() proto for VMS
// into a different header
//
#include "osiSock.h"

#define epicsExportSharedSymbols
#include "osiTime.h"

//
// osiTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int tsStampGetCurrent (TS_STAMP *pDest)
{
#   if defined(CLOCK_REALTIME)
        struct timespec ts;
        int status;
    
        status = clock_gettime (CLOCK_REALTIME, &ts);
        if (status) {
            return -1;
        }
        *pDest = osiTime (ts);
        return 0;
#   else
    	int status;
    	struct timeval tv;
    
    	status = gettimeofday (&tv, NULL);
        if (status!=0) {
            return -1;
        }
    	*pDest = osiTime (tv); 
        return 0;
#   endif
}

//
// tsStampGetEvent ()
//
extern "C" epicsShareFunc int epicsShareAPI tsStampGetEvent (TS_STAMP *pDest, unsigned eventNumber)
{
    if (eventNumber==tsStampEventCurrentTime) {
        return tsStampGetCurrent (pDest);
    }
    else {
        return tsStampERROR;
    }
}