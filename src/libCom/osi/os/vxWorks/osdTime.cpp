
#include <vxWorks.h>
#include <tickLib.h>
#include <sysLib.h>

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
	ULONG	ticks;
	ULONG	sec;
	ULONG	nsec;
	ULONG	rate = sysClkRateGet();
 
	//
	// currently assuming that this has been already adjusted
	// for the EPICS epoch
	//
	ticks = tickGet();
	sec = ticks / rate;
	nsec = (ticks % rate) * (nSecPerSec / rate); 
 
	return osiTime (sec, nsec);
}
