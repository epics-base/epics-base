#define epicsExportSharedSymbols
#include "osiTime.h"

extern long TSgetTimeStamp(int event_number,TS_STAMP* ts);

//
// osiTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int tsStampGetCurrent (TS_STAMP *pDest)
{
    return(TSgetTimeStamp(0,pDest));
}

//
// tsStampGetEvent ()
//
extern "C" epicsShareFunc int epicsShareAPI tsStampGetEvent (TS_STAMP *pDest, unsigned eventNumber)
{
    return(TSgetTimeStamp((int)eventNumber,pDest));
}
