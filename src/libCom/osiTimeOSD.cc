
#include <osiTime.h>


#ifdef SUNOS4
extern "C" {
int gettimeofday (struct timeval *tp, struct timezone *tzp);
}
#endif

#if defined(UNIX)

#	include <sys/types.h>
#	include <sys/time.h>

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

#elif defined(vxWorks)

#	include <tickLib.h>
#	include <sysLib.h>

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
#else
#	error please define an OS type
#endif

