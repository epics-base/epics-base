

#define epicsExportSharedSymbols
#include <osiTime.h>


#include <tickLib.h>
#include <sysLib.h>

//
// osiTime::getCurrent ()
//
osiTime osiTime::getCurrent ()
{
	ULONG	ticks;
	ULONG	sec;
	ULONG	nsec;
	ULONG	rate = sysClkRateGet();
 
	ticks = tickGet();
	sec = ticks/rate;
	nsec = (ticks%rate)*(nSecPerSec/rate); 
 
	return osiTime(sec, nsec);
}

