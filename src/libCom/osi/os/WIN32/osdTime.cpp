
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
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN 
#include <winsock2.h>
#include <process.h>

/*
 * for _ftime()
 */
#include <sys/types.h>
#include <sys/timeb.h>

//
// EPICS
//
#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsThread.h"

//
// performance counter last value, ticks per sec,
// and its offset from the EPICS epoch.
// 
static LONGLONG perf_last, perf_freq;

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

static bool osdTimeInitSuccess = false;
static epicsThreadOnceId osdTimeOnceFlag = EPICS_THREAD_ONCE_INIT;
static const LONGLONG FILE_TIME_TICKS_PER_SEC = 10000000L;
static CRITICAL_SECTION osdTimeCriticalSection; 
static bool osdTimeSyncThreadExit = false;
static LONGLONG epicsEpoch;

static int osdTimeSych ();


/*
 * osdTimeExit ()
 */
static void osdTimeExit ()
{
    DeleteCriticalSection ( & osdTimeCriticalSection );
    osdTimeSyncThreadExit = true;
}

/*
 * epicsWin32ThreadEntry()
 */
static unsigned __stdcall osdTimeSynchThreadEntry (LPVOID)
{
    static const DWORD tmoTenSec = 10 * epicsTime::mSecPerSec;
    int status;

    while ( ! osdTimeSyncThreadExit ) {
	    Sleep (tmoTenSec); 
        status = osdTimeSych ();
        if (status!=epicsTimeOK) {
            errlogPrintf ("osdTimeSych (): failed?\n");
        }
    }
    return 0u;
}

//
// osdTimeInit ()
//
static void osdTimeInit ( void * )
{
	LARGE_INTEGER parm;
	BOOL success;
    int unixStyleStatus;
    HANDLE osdTimeThread;
    unsigned threadAddr;
	FILETIME epicsEpochFT;

    InitializeCriticalSection ( & osdTimeCriticalSection );

	//
	// initialize elapsed time counters
	//
	// All CPUs running win32 currently have HR
	// counters (Intel and Mips processors do)
	//
	if ( QueryPerformanceFrequency (&parm) == 0 ) {
        DeleteCriticalSection ( & osdTimeCriticalSection );
        return;
	}
	perf_freq = parm.QuadPart;

	//
	// convert the EPICS epoch to file time
	//
	success = SystemTimeToFileTime (&epicsEpochST, &epicsEpochFT);
    if ( ! success ) {
        DeleteCriticalSection ( & osdTimeCriticalSection );
        return;
    }
	parm.LowPart = epicsEpochFT.dwLowDateTime;
	parm.HighPart = epicsEpochFT.dwHighDateTime;
	epicsEpoch = parm.QuadPart;

    // set here to avoid recursion problems
    osdTimeInitSuccess = true;

    unixStyleStatus = osdTimeSych ();
    if ( unixStyleStatus != epicsTimeOK ) {
        DeleteCriticalSection ( & osdTimeCriticalSection );
        return;
    }

    //
    // spawn off a thread which periodically resynchronizes the offset
    //
    osdTimeThread = (HANDLE) _beginthreadex ( NULL, 4096, osdTimeSynchThreadEntry, 
                0, 0, &threadAddr );
    if ( osdTimeThread == NULL ) {
        errlogPrintf ( "osdTimeInit(): unable to start time synch thread\n" );
    }
    else {
        assert ( CloseHandle ( osdTimeThread ) );
    }

    atexit ( osdTimeExit );

}

//
// osdTimeSych ()
//
static int osdTimeSych ()
{
    static const DWORD tmoTwentySec = 20 * epicsTime::mSecPerSec;
	LONGLONG new_sec_offset, new_frac_offset;
	LARGE_INTEGER parm;
	LONGLONG secondsSinceBoot;
	FILETIME currentTimeFT;
	LONGLONG currentTime;

    epicsThreadOnce ( &osdTimeOnceFlag, osdTimeInit, 0 );
    if ( ! osdTimeInitSuccess ) {
        return epicsTimeERROR;
    }

	//
	// its important that the following two time queries
	// are close together (immediately adjacent to each
	// other) in the code
	//
	GetSystemTimeAsFileTime (&currentTimeFT);
	// this one is second because QueryPerformanceFrequency()
	// has forced its code to load
    if ( QueryPerformanceCounter ( & parm ) == 0 ) {
        return epicsTimeERROR;
    }

    EnterCriticalSection ( & osdTimeCriticalSection );
	 
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

    LeaveCriticalSection ( & osdTimeCriticalSection );

    return epicsTimeOK;
}

//
// epicsTime::osdGetCurrent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetCurrent (epicsTimeStamp *pDest)
{
    static const DWORD tmoTwentySec = 20 * epicsTime::mSecPerSec;
	LONGLONG time_cur, time_sec, time_remainder;
	LARGE_INTEGER parm;
    BOOL status;

    epicsThreadOnce ( &osdTimeOnceFlag, osdTimeInit, 0 );
    if ( ! osdTimeInitSuccess ) {
        return epicsTimeERROR;
    }

    EnterCriticalSection ( & osdTimeCriticalSection );

	//
	// dont need to check status since it was checked once
	// during initialization to see if the CPU has HR
	// counters (Intel and Mips processors do)
	//
	status = QueryPerformanceCounter (&parm);
    if (!status) {
        LeaveCriticalSection ( & osdTimeCriticalSection );
        return epicsTimeERROR;
    }
	time_cur = parm.QuadPart;
	if (perf_last > time_cur) {	

		//
		// must have been a timer roll-over
		// It takes 9.223372036855e+18/perf_freq sec
		// to roll over this counter (perf_freq is 1193182
		// sec on my system). This is currently about 245118 years.
		//
		// attempt to add number of seconds in a 64 bit integer
		// in case the timer resolution improves
		//
		perf_sec_offset += MAXLONGLONG / perf_freq;
		perf_frac_offset += MAXLONGLONG % perf_freq;
		if (perf_frac_offset>=perf_freq) {
			perf_sec_offset++;
			perf_frac_offset -= perf_freq;
		}
	}
	time_sec = time_cur / perf_freq;
	time_remainder = time_cur % perf_freq;

	//
	// add time (sec) since the EPICS epoch 
	//
	time_sec += perf_sec_offset;
	time_remainder += perf_frac_offset;
	if (time_remainder>=perf_freq) {
		time_sec += 1;
		time_remainder -= perf_freq;
	}

	perf_last = time_cur;

	pDest->secPastEpoch = (unsigned long) (time_sec%ULONG_MAX);
    pDest->nsec = (unsigned long) ((time_remainder*epicsTime::nSecPerSec)/perf_freq);

    LeaveCriticalSection ( & osdTimeCriticalSection );

	return epicsTimeOK;
}

//
// epicsTimeGetEvent ()
//
extern "C" epicsShareFunc int epicsShareAPI epicsTimeGetEvent (epicsTimeStamp *pDest, unsigned eventNumber)
{
    if (eventNumber==epicsTimeEventCurrentTime) {
        return epicsTimeGetCurrent (pDest);
    }
    else {
        return epicsTimeERROR;
    }
}

inline void UnixTimeToFileTime ( const time_t * pAnsiTime, LPFILETIME pft )
{
     // Note that LONGLONG is a 64-bit value
     LARGE_INTEGER ll;

     ll.QuadPart = Int32x32To64 ( *pAnsiTime, 10000000 ) + 116444736000000000;
     pft->dwLowDateTime = ll.LowPart;
     pft->dwHighDateTime = ( DWORD ) ll.HighPart;
}

static int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31,
    31, 30, 31, 30, 31 }; 

static bool isLeapYear ( DWORD year ) 
{
    if ( (year % 4) == 0 ) {
        if ( ( year % 100 ) != 0 || ( year % 400 ) == 0 ) {
            return true;         
        }
        else {
            return false;         
        }
    } else {                 
        return false;         
    } 
} 

static int dayOfYear ( DWORD day, DWORD month, DWORD year ) 
{
    DWORD nDays = 0;
    for ( unsigned m = 1; m < month; m++ ) {
        nDays += daysInMonth[m-1];
        if ( m == 2 && isLeapYear(year) ) {
            nDays++;
        }
    }
    return nDays + day;
}

// synthesize a reentrant gmtime on WIN32
int epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    FILETIME ft;
    UnixTimeToFileTime ( pAnsiTime, &ft );

    SYSTEMTIME st;
    BOOL status = FileTimeToSystemTime ( &ft, &st );
    if ( ! status ) {
		return epicsTimeERROR;
    }

    pTM->tm_sec = st.wSecond; // seconds after the minute - [0,59]
    pTM->tm_min = st.wMinute; // minutes after the hour - [0,59]
    pTM->tm_hour = st.wHour; // hours since midnight - [0,23]
    assert ( st.wDay >= 1 && st.wDay <= 31 );
    pTM->tm_mday = st.wDay; // day of the month - [1,31]
    assert ( st.wMonth >= 1 && st.wMonth <= 12 );
    pTM->tm_mon = st.wMonth - 1; // months since January - [0,11]
    assert ( st.wYear >= 1900 );
    pTM->tm_year = st.wYear - 1900; // years since 1900
    pTM->tm_wday = st.wDayOfWeek; // days since Sunday - [0,6] 
    pTM->tm_yday = dayOfYear ( st.wDay, st.wMonth, st.wYear ) - 1; 
    pTM->tm_isdst = 0;

	return epicsTimeOK;
}

// synthesize a reentrant localtime on WIN32
int epicsTime_localtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    FILETIME ft;
    UnixTimeToFileTime ( pAnsiTime, &ft );

    FILETIME lft;
    BOOL status = FileTimeToLocalFileTime ( &ft, &lft );
    if ( ! status ) {
		return epicsTimeERROR;
    }

    SYSTEMTIME st;
    status = FileTimeToSystemTime ( &lft, &st );
    if ( ! status ) {
		return epicsTimeERROR;
    }
    pTM->tm_sec = st.wSecond; // seconds after the minute - [0,59]
    pTM->tm_min = st.wMinute; // minutes after the hour - [0,59]
    pTM->tm_hour = st.wHour; // hours since midnight - [0,23]
    assert ( st.wDay >= 1 && st.wDay <= 31 );
    pTM->tm_mday = st.wDay; // day of the month - [1,31]
    assert ( st.wMonth >= 1 && st.wMonth <= 12 );
    pTM->tm_mon = st.wMonth - 1; // months since January - [0,11]
    assert ( st.wYear >= 1900 );
    pTM->tm_year = st.wYear - 1900; // years since 1900
    pTM->tm_wday = st.wDayOfWeek; // days since Sunday - [0,6] 
    pTM->tm_yday = dayOfYear ( st.wDay, st.wMonth, st.wYear ) - 1;

    TIME_ZONE_INFORMATION tzInfo;
    DWORD tzStatus = GetTimeZoneInformation ( &tzInfo );
    switch ( tzStatus ) {
    case TIME_ZONE_ID_UNKNOWN:
        pTM->tm_isdst = -1;
        break;
    case TIME_ZONE_ID_STANDARD:
        pTM->tm_isdst = 0;
        break;
    case TIME_ZONE_ID_DAYLIGHT:
        pTM->tm_isdst = 1;
        break;
    default:
        pTM->tm_isdst = -1;
        break;
    }

	return epicsTimeOK;
}
 

















