
#include <sys/types.h>
#include <sys/time.h>

//
// for sunos4
//
extern "C" {
int gettimeofday (struct timeval *tp, struct timezone *tzp);
}

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

