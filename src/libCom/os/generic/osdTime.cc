
//
// should move the time stuff into a different header
//
#include "osiSock.h"

#define epicsExportSharedSymbols
#include "osiTime.h"
 

//
// osiTime::getCurrent ()
//
osiTime osiTime::getCurrent ()
{
	int             status;
	struct timeval  tv;

	status = gettimeofday (&tv, NULL);
	assert (status==0);

	return osiTime(tv.tv_sec, tv.tv_usec * nSecPerUSec);
}

