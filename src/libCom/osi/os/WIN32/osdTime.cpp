
//
// $Id$
//
// Author: Jeff Hill
//
//

//
// ANSI C
//
#include <math.h>
#include <time.h>
#include <limits.h>

//
// WIN32
//
#include <winsock2.h>

/*
 * for _ftime()
 */
#include <sys/types.h>
#include <sys/timeb.h>

//
// EPICS
//
#define epicsExportSharedSymbols
#include <osiTime.h>

//
// performance counter last value, ticks per sec,
// and its offset from the EPICS epoch.
// 
static LONGLONG perf_last, perf_freq, perf_offset;

//
// divide the offset into seconds and
// fractions of a second so that overflow is less
// likely (we dont know the magnitude of perf_freq
// until run time)
//
static LONGLONG perf_sec_offset=-1, perf_frac_offset;

static const SYSTEMTIME epicsEpochST = {
	1990, // year
	1, // month
	1, // day of the week (Monday)
	1, // day of the month
	0, // hour
	0, // min
	0, // sec
	0 // milli sec
};

static LONGLONG epicsEpoch;

static LONGLONG FILE_TIME_TICKS_PER_SEC = 10000000L;

/*
 * synchronize()
 */
void osiTime::synchronize()
{
	static int init = 0;
	LONGLONG new_sec_offset, new_frac_offset;
	LARGE_INTEGER parm;
	LONGLONG secondsSinceBoot;
	FILETIME currentTimeFT;
	LONGLONG currentTime;
	BOOL win32Stat;

	//
	// one time initialization of constants
	//
	if (!init) {
		FILETIME epicsEpochFT;

		//
		// initialize elapsed time counters
		//
		// All CPUs running win32 currently have HR
		// counters (Intel and Mips processors do)
		//
		if (QueryPerformanceFrequency (&parm)==0) {
            throw unableToFetchCurrentTime ();
		}
		perf_freq = parm.QuadPart;

		//
		// convert the EPICS epoch to file time
		//
		win32Stat = SystemTimeToFileTime (&epicsEpochST, &epicsEpochFT);
        if (win32Stat==0) {
            throw unableToFetchCurrentTime ();
        }
		parm.LowPart = epicsEpochFT.dwLowDateTime;
		parm.HighPart = epicsEpochFT.dwHighDateTime;
		epicsEpoch = parm.QuadPart;

		init = 1;
	}

	//
	// its important that the following two time queries
	// are close together (immediately adjacent to each
	// other) in the code
	//
	GetSystemTimeAsFileTime (&currentTimeFT);
	// this one is second because QueryPerformanceFrequency()
	// has forced its code to load
    if (QueryPerformanceCounter (&parm)==0) {
        throw unableToFetchCurrentTime ();
    }
	 
	perf_last = parm.QuadPart;
	parm.LowPart = currentTimeFT.dwLowDateTime;
	parm.HighPart = currentTimeFT.dwHighDateTime;
	currentTime = parm.QuadPart;

	//
	// check for strange date, and clamp to the
	// epics epoch if so
	//
	if (currentTime>epicsEpoch) {
		//
		// compute the offset from the EPICS epoch
		//
		LONGLONG diff = currentTime - epicsEpoch;

		//
		// compute the seconds offset and the 
		// fractional offset part in the FILETIME timebase
		//
		new_sec_offset = diff / FILE_TIME_TICKS_PER_SEC;
		new_frac_offset = diff % FILE_TIME_TICKS_PER_SEC;

		//
		// compute the fractional second offset in the performance 
		// counter time base
		//	
		new_frac_offset = (new_frac_offset*perf_freq) / FILE_TIME_TICKS_PER_SEC;
	}
	else {
		new_sec_offset = 0;
		new_frac_offset = 0;
	}

	//
	// subtract out the perormance counter ticks since the 
	// begining of windows
	//	
	secondsSinceBoot = perf_last / perf_freq;
	if (new_sec_offset>=secondsSinceBoot) {
		new_sec_offset -= secondsSinceBoot;

		LONGLONG fracSinceBoot = perf_last % perf_freq;
		if (new_frac_offset>=fracSinceBoot) {
			new_frac_offset -= fracSinceBoot;
		}
		else if (new_sec_offset>0) {
			//
			// borrow
			//
			new_sec_offset--;
			new_frac_offset += perf_freq - fracSinceBoot;
		}
		else {
			new_frac_offset = 0;
		}
	}
	else {
		new_sec_offset = 0;
		new_frac_offset = 0;
	}


#if 0
	//
	// calculate the change
	//
	{
		double change;
		change = (double) (perf_sec_offset-new_sec_offset)*perf_freq +
			(perf_frac_offset-new_frac_offset);
		change /= perf_freq;
		printf ("The performance counter drifted by %f sec\n", change);
	}
#endif

	//
	// update the current value
	//
	perf_sec_offset = new_sec_offset;
	perf_frac_offset = new_frac_offset;
}

//
// osiTime::osdGetCurrent ()
//
osiTime osiTime::osdGetCurrent ()
{
	LONGLONG time_cur, time_sec, time_remainder;
	LARGE_INTEGER parm;
	unsigned long sec, nsec;

	/*
	 * lazy init
	 */
	if (perf_sec_offset<0) {
		osiTime::synchronize();
	}

	/*
	 * dont need to check status since it was checked once
	 * during initialization to see if the CPU has HR
	 * counters (Intel and Mips processors do)
	 */
	QueryPerformanceCounter (&parm);
	time_cur = parm.QuadPart;
	if (perf_last > time_cur)	{	/* must have been a timer roll-over */

		/*
		 * must have been a timer roll-over
		 * It takes 9.223372036855e+18/perf_freq sec
		 * to roll over this counter (perf_freq is 1193182
		 * sec on my system). This is currently about 245118 years.
		 *
		 * attempt to add number of seconds in a 64 bit integer
		 * in case the timer resolution improves
		 */
		perf_sec_offset += MAXLONGLONG / perf_freq;
		perf_frac_offset += MAXLONGLONG % perf_freq;
		if (perf_frac_offset>=perf_freq) {
			perf_sec_offset++;
			perf_frac_offset -= perf_freq;
		}
	}
	time_sec = time_cur / perf_freq;
	time_remainder = time_cur % perf_freq;

	/* 
	 * add time (sec) since the EPICS epoch 
	 */
	time_sec += perf_sec_offset;
	time_remainder += perf_frac_offset;
	if (time_remainder>=perf_freq) {
		time_sec += 1;
		time_remainder -= perf_freq;
	}

	perf_last = time_cur;

	sec = (unsigned long) (time_sec%ULONG_MAX);
	nsec = (unsigned long) ((time_remainder*nSecPerSec)/perf_freq);
	return osiTime (sec, nsec);
}

//
// gmtime_r ()
//
// from posix real time
//
struct tm *gmtime_r (const time_t *pAnsiTime, struct tm *pTM)
{
	HANDLE thisThread = GetCurrentThread ();
	struct tm *p;
	int oldPriority;
	BOOL win32Success;

	oldPriority = GetThreadPriority (thisThread);
	if (oldPriority==THREAD_PRIORITY_ERROR_RETURN) {
		return NULL;
	}

	win32Success = SetThreadPriority (thisThread, THREAD_PRIORITY_TIME_CRITICAL);
	if (!win32Success) {
		return NULL;
	}

	p = gmtime (pAnsiTime);
	if (p!=NULL) {
		*pTM = *p;
	}

	win32Success = SetThreadPriority (thisThread, oldPriority);
	if (!win32Success) {
        throw osiTime::internalFailure();
    }

	return p;
}

//
// localtime_r ()
//
// from posix real time
//
struct tm *localtime_r (const time_t *pAnsiTime, struct tm *pTM)
{
	HANDLE thisThread = GetCurrentThread ();
	struct tm *p;
	int oldPriority;
	BOOL win32Success;

	oldPriority = GetThreadPriority (thisThread);
	if (oldPriority==THREAD_PRIORITY_ERROR_RETURN) {
		return NULL;
	}

	win32Success = SetThreadPriority (thisThread, THREAD_PRIORITY_TIME_CRITICAL);
	if (!win32Success) {
		return NULL;
	}

	p = localtime (pAnsiTime);
	if (p!=NULL) {
		*pTM = *p;
	}

	win32Success = SetThreadPriority (thisThread, oldPriority);
	if (!win32Success) {
        throw osiTime::internalFailure ();
    }
	
	return p;
}
