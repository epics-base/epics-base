

#define epicsExportSharedSymbols
#include "osiTime.h"

//
// osiTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int tsStampGetCurrent (TS_STAMP *pDest)
{
    int status;
    struct timespec ts;

    status = clock_gettime (CLOCK_REALTIME, &ts);
    if (status) {
        return -1;
    }
    *pDest = osiTime (ts);
    return 0;
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
