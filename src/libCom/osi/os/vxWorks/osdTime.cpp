

#define epicsExportSharedSymbols
#include "osiTime.h"

//
// osiTime::synchronize()
//
void osiTime::synchronize()
{
}

//
// osiTime::osdGetCurrent ()
//
osiTime osiTime::osdGetCurrent ()
{
    struct timespec ts;
    int status;

    status = clock_gettime(CLOCK_REALTIME, &ts);
    if (status!=0) {
        throw unableToFetchCurrentTime ();
    }
    return osiTime (ts);
}
