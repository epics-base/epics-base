
#include <taskLib.h>
#include <sysLib.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "osiSleep.h"

epicsShareFunc void epicsShareAPI osiSleep (unsigned sec, unsigned uSec)
{
	ULONG ticks;
	int status;
	
	ticks = (uSec*sysClkRateGet())/1000000;
	ticks += sec*sysClkRateGet();

	if (ticks==0) {
		ticks = 1;
	}

	status = taskDelay (ticks);
	assert (status==OK);
}