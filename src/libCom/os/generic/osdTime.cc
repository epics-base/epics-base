
//
// should move the time stuff into a different header
//
#include "osiSock.h"

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
	int status;
	struct timeval tv;

	status = gettimeofday (&tv, NULL);
	assert (status==0);

	return osiTime (osiTime::time_tToInternalSec (tv.tv_sec), tv.tv_usec * nSecPerUSec);
}

