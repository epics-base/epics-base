
#include <time.h>
#include <winsock2.h>
#include <mmsystem.h>

#define epicsExportSharedSymbols
#include <osiTime.h>


/*
 *	This code is mainly adapted form Chris Timossi's windows_depen.c
 *
 *	8-2-96  -kuk-
 */


/* offset from timeGetTime() to 1970 */
static unsigned long	offset_secs;
static DWORD		prev_time = 0;
static UINT		res;
static char init = 0;

/*
 *	init_osi_time has to be called before using the timer,
 *	exit_osi_time has to be called in balance.
 */
int init_osi_time ()
{
	TIMECAPS	tc;

	if (init) {
		return 1;
	}

	if (timeGetDevCaps (&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
	{
		fprintf (stderr, "init_osi_time: cannot get timer info\n");
		return 1;
	}
	/* set for 1 ms resoulution */
	res = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);
	if (timeBeginPeriod (res) != TIMERR_NOERROR)
	{
		fprintf(stderr,"timer setup failed\n");
		return 2;
	}
	prev_time =  timeGetTime();
	offset_secs = (long)time(NULL) - (long)prev_time/1000;

	init = 1;

	return 0;
}

int exit_osi_time ()
{
	if (!init) {
		return 0;
	}

	timeEndPeriod (res);
	init = 0;

	return 0;
}


//
// osiTime::getCurrent ()
//
osiTime osiTime::getCurrent ()
{
	unsigned long now;

	/*
	 * this allows the code to work when it is in an object
	 * library (in addition to inside a dll)
	 */
	if (!init) {
		init_osi_time();
		init = 1;
	}

	/* MS Online help:
	 * Note that the value returned by the timeGetTime function is
	 * a DWORD value. The return value wraps around to
	 * 0 every 2^32 milliseconds, which is about 49.71 days.
	 */

	// get millisecs, timeGetTime gives DWORD
	now = timeGetTime ();
	if (prev_time > now)	/* looks like a DWORD overflow */
		offset_secs +=  4294967ul; /* 0x100000000 / 1000 */
 
	prev_time = now;

	// compute secs and nanosecs from millisecs:
	return osiTime (now / 1000ul + offset_secs, (now % 1000ul) * 1000000ul);
}

