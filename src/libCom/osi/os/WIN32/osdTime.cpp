
#include <math.h>
#include <time.h>
#include <winsock2.h>

#define epicsExportSharedSymbols
#include <osiTime.h>

static long offset_time_s = 0;  /* time diff (sec) from 1990 when EPICS started */
static LARGE_INTEGER time_prev, time_freq;

/*
 *	init_osi_time has to be called before using the timer,
 *	exit_osi_time has to be called in balance.
 */
int init_osi_time ()
{
	if (offset_time_s == 0) {
		/*
		 * initialize elapsed time counters
		 *
		 * All CPUs running win32 currently have HR
		 * counters (Intel and Mips processors do)
		 */
		if (QueryPerformanceCounter (&time_prev)==0) {
			return 1;
		}
		if (QueryPerformanceFrequency (&time_freq)==0) {
			return 1;
		}
		offset_time_s = (long)time(NULL) - 
				(long)(time_prev.QuadPart/time_freq.QuadPart);
	}

	return 0;
}

int exit_osi_time ()
{
	offset_time_s = 0;

	return 0;
}


//
// osiTime::getCurrent ()
//
osiTime osiTime::getCurrent ()
{
	LARGE_INTEGER time_cur, time_sec, time_remainder;
	unsigned long sec, nsec;

	/*
	 * this allows the code to work when it is in an object
	 * library (in addition to inside a dll)
	 */
	if (offset_time_s==0) {
		init_osi_time();
	}

	/*
	 * dont need to check status since it was checked once
	 * during initialization to see if the CPU has HR
	 * counters (Intel and Mips processors do)
	 */
	QueryPerformanceCounter (&time_cur);
	if (time_prev.QuadPart > time_cur.QuadPart)	{	/* must have been a timer roll-over */
		double offset;
		/*
		 * must have been a timer roll-over
		 * It takes 9.223372036855e+18/time_freq sec
		 * to roll over this counter (time_freq is 1193182
		 * sec on my system). This is currently about 245118 years.
		 *
		 * attempt to add number of seconds in a 64 bit integer
		 * in case the timer resolution improves
		 */
		offset = pow(2.0, 63.0)-1.0/time_freq.QuadPart;
		if (offset<=LONG_MAX-offset_time_s) {
			offset_time_s += (long) offset;
		}
		else {
			/*
			 * this problem cant be fixed, but hopefully will never occurr
			 */
			fprintf (stderr, "%s.%d Timer overflowed\n", __FILE__, __LINE__);
			return osiTime (0, 0);
		}
	}
	time_sec.QuadPart = time_cur.QuadPart / time_freq.QuadPart;
	time_remainder.QuadPart = time_cur.QuadPart % time_freq.QuadPart;
	if (time_sec.QuadPart > LONG_MAX-offset_time_s) {
		/*
		 * this problem cant be fixed, but hopefully will never occurr
		 */
		fprintf (stderr, "%s.%d Timer value larger than storage\n", __FILE__, __LINE__);
		return osiTime (0, 0);
	}

	/* add time (sec) since 1970 */
	sec = offset_time_s + (long)time_sec.QuadPart;
	nsec = (long)((time_remainder.QuadPart*1000000000)/time_freq.QuadPart);

	time_prev = time_cur;

	return osiTime (sec, nsec);
}
