/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

//
// Author: Jeff Hill
//

//
// ANSI C
//
#include <cmath>
#include <ctime>
#include <climits>
#include <cstdio>

//
// WIN32
//
#define VC_EXTRALEAN
#define STRICT
#include <windows.h>
#include <process.h>

//
// EPICS
//
#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "generalTimeSup.h"
#include "epicsTimer.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsThread.h"

#if defined ( DEBUG )
#   define debugPrintf(argsInParen) ::printf argsInParen
#else
#   define debugPrintf(argsInParen)
#endif

extern "C" void setThreadName ( DWORD dwThreadID, LPCSTR szThreadName );

static int osdTimeGetCurrent ( epicsTimeStamp *pDest );

// for mingw
#if !defined ( MAXLONGLONG )
#   define MAXLONGLONG 0x7fffffffffffffffLL
#endif
#if !defined ( MINLONGLONG )
#   define MINLONGLONG ~0x7fffffffffffffffLL
#endif
#ifndef STACK_SIZE_PARAM_IS_A_RESERVATION
#   define STACK_SIZE_PARAM_IS_A_RESERVATION 0x00010000
#endif

static const LONGLONG epicsEpochInFileTime = 0x01b41e2a18d64000LL;

static unsigned __stdcall _pllThreadEntry ( void * pCurrentTimeIn );

class currentTime {
public:
    currentTime ();
    ~currentTime ();
    void getCurrentTime ( epicsTimeStamp & dest );
    void startPLL ();
private:
    CRITICAL_SECTION mutex;
    LONGLONG lastPerfCounter;
    LONGLONG perfCounterFreq;
    LONGLONG epicsTimeLast; // nano-sec since the EPICS epoch
    LONGLONG perfCounterFreqPLL;
    LONGLONG lastPerfCounterPLL;
    LONGLONG lastFileTimePLL;
    HANDLE threadHandle;
    unsigned threadId;
    bool perfCtrPresent;
    bool threadShutdownCmd;
    bool threadHasExited;
    void updatePLL ();
    static const int pllDelay; /* integer seconds */
    // cant be static because of diff btw __stdcall and __cdecl 
    friend unsigned __stdcall _pllThreadEntry ( void * pCurrentTimeIn );
};

const int currentTime :: pllDelay = 5;

static currentTime * pCurrentTime = 0;
static const LONGLONG FILE_TIME_TICKS_PER_SEC = 10000000;
static const LONGLONG EPICS_TIME_TICKS_PER_SEC = 1000000000;
static const LONGLONG ET_TICKS_PER_FT_TICK =
            EPICS_TIME_TICKS_PER_SEC / FILE_TIME_TICKS_PER_SEC;
    
//
// Start and register time provider
//
static int timeRegister(void)
{
    pCurrentTime = new currentTime ();

    generalTimeCurrentTpRegister("PerfCounter", 150, osdTimeGetCurrent);

    pCurrentTime->startPLL ();
    return 1;
}
static int done = timeRegister();

//
// osdTimeGetCurrent ()
//
static int osdTimeGetCurrent ( epicsTimeStamp *pDest )
{
    assert ( pCurrentTime );

    pCurrentTime->getCurrentTime ( *pDest );
    return epicsTimeOK;
}

// synthesize a reentrant gmtime on WIN32
int epicsShareAPI epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    struct tm * pRet = gmtime ( pAnsiTime );
    if ( pRet ) {
        *pTM = *pRet;
        return epicsTimeOK;
    }
    else {
        return errno;
    }
}

// synthesize a reentrant localtime on WIN32
int epicsShareAPI epicsTime_localtime (
    const time_t * pAnsiTime, struct tm * pTM )
{
    struct tm * pRet = localtime ( pAnsiTime );
    if ( pRet ) {
        *pTM = *pRet;
        return epicsTimeOK;
    }
    else {
        return errno;
    }
}

currentTime::currentTime () :
    lastPerfCounter ( 0 ),
    perfCounterFreq ( 0 ),
    epicsTimeLast ( 0 ),
    perfCounterFreqPLL ( 0 ),
    lastPerfCounterPLL ( 0 ),
    lastFileTimePLL ( 0 ),
    threadHandle ( 0 ),
    threadId ( 0 ),
    perfCtrPresent ( false ),
    threadShutdownCmd ( false ),
    threadHasExited ( false )
{
    InitializeCriticalSection ( & this->mutex );

    // avoid interruptions by briefly becoming a time critical thread
    int originalPriority = GetThreadPriority ( GetCurrentThread () );
    SetThreadPriority ( GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL );

    FILETIME ft;
    GetSystemTimeAsFileTime ( & ft );
    LARGE_INTEGER tmp;
    QueryPerformanceCounter ( & tmp );
    this->lastPerfCounter = tmp.QuadPart;
    // if no high resolution counters then default to low res file time
    if ( QueryPerformanceFrequency ( & tmp ) ) {
        this->perfCounterFreq = tmp.QuadPart;
        this->perfCtrPresent = true;
    }
    SetThreadPriority ( GetCurrentThread (), originalPriority );

    LARGE_INTEGER liFileTime;
    liFileTime.LowPart = ft.dwLowDateTime;
    liFileTime.HighPart = ft.dwHighDateTime;

    if ( liFileTime.QuadPart >= epicsEpochInFileTime ) {
        // the windows file time has a maximum resolution of 100 nS
        // and a nominal resolution of 10 mS - 16 mS
        this->epicsTimeLast =
            ( liFileTime.QuadPart - epicsEpochInFileTime ) *
            ET_TICKS_PER_FT_TICK;
    }
    else {
        errlogPrintf (
            "win32 osdTime.cpp init detected questionable "
            "system date prior to EPICS epoch, epics time will not advance\n" );
        this->epicsTimeLast = 0;
    }

    this->perfCounterFreqPLL = this->perfCounterFreq;
    this->lastPerfCounterPLL = this->lastPerfCounter;
    this->lastFileTimePLL = liFileTime.QuadPart;
}

void currentTime :: startPLL ()
{
    // create frequency estimation thread when needed
    if ( this->perfCtrPresent && ! this->threadHandle ) {
        this->threadHandle = (HANDLE) 
            _beginthreadex ( 0, 4096, _pllThreadEntry, this,
                CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION, 
                & this->threadId );
        assert ( this->threadHandle );
        BOOL bstat = SetThreadPriority ( 
        	this->threadHandle, THREAD_PRIORITY_HIGHEST );
        assert ( bstat );
        DWORD wstat =  ResumeThread ( this->threadHandle );
        assert ( wstat != 0xFFFFFFFF );
    }
}

currentTime::~currentTime ()
{
    EnterCriticalSection ( & this->mutex );
    this->threadShutdownCmd = true;
    while ( ! this->threadHasExited ) {
        LeaveCriticalSection ( & this->mutex );
        Sleep ( 250 /* mS */ );
        EnterCriticalSection ( & this->mutex );
    }
    LeaveCriticalSection ( & this->mutex );
    DeleteCriticalSection ( & this->mutex );
}

void currentTime::getCurrentTime ( epicsTimeStamp & dest )
{
    if ( this->perfCtrPresent ) {
        EnterCriticalSection ( & this->mutex );

        LARGE_INTEGER curPerfCounter;
        QueryPerformanceCounter ( & curPerfCounter );

        LONGLONG offset;
        if ( curPerfCounter.QuadPart >= this->lastPerfCounter ) {
            offset = curPerfCounter.QuadPart - this->lastPerfCounter;
        }
        else {
            //
            // must have been a timer roll-over event
            //
            // It takes 9.223372036855e+18/perf_freq sec to roll over this
            // counter. This is currently about 245118 years using the perf
            // counter freq value on my system (1193182). Nevertheless, I
            // have code for this situation because the performance
            // counter resolution will more than likely improve over time.
            //
            offset = ( MAXLONGLONG - this->lastPerfCounter )
                                + ( curPerfCounter.QuadPart - MINLONGLONG ) + 1;
        }
        if ( offset < MAXLONGLONG / EPICS_TIME_TICKS_PER_SEC ) {
            offset *= EPICS_TIME_TICKS_PER_SEC;
            offset /= this->perfCounterFreq;
        }
        else {
            double fpOffset = static_cast < double > ( offset );
            fpOffset *= EPICS_TIME_TICKS_PER_SEC;
            fpOffset /= static_cast < double > ( this->perfCounterFreq );
            offset = static_cast < LONGLONG > ( fpOffset );
        }
        LONGLONG epicsTimeCurrent = this->epicsTimeLast + offset;
        if ( this->epicsTimeLast > epicsTimeCurrent ) {
            double diff = static_cast < double >
                ( this->epicsTimeLast - epicsTimeCurrent ) / EPICS_TIME_TICKS_PER_SEC;
            errlogPrintf (
                "currentTime::getCurrentTime(): %f sec "
                "time discontinuity detected\n",
                diff );
        }
        this->epicsTimeLast = epicsTimeCurrent;
        this->lastPerfCounter = curPerfCounter.QuadPart;

        LeaveCriticalSection ( & this->mutex );

        dest.secPastEpoch = static_cast < epicsUInt32 >
            ( epicsTimeCurrent / EPICS_TIME_TICKS_PER_SEC );
        dest.nsec = static_cast < epicsUInt32 >
            ( epicsTimeCurrent % EPICS_TIME_TICKS_PER_SEC );
    }
    else {
        // if high resolution performance counters are not supported then
        // fall back to low res file time
        FILETIME ft;
        GetSystemTimeAsFileTime ( & ft );
        dest = epicsTime ( ft );
    }
}

//
// Maintain corrected version of the performance counter's frequency using
// a phase locked loop. This approach is similar to NTP's.
//
void currentTime :: updatePLL ()
{
    EnterCriticalSection ( & this->mutex );

    // avoid interruptions by briefly becoming a time critical thread
    LARGE_INTEGER curFileTime;
    LARGE_INTEGER curPerfCounter;
    {
        int originalPriority = GetThreadPriority ( GetCurrentThread () );
        SetThreadPriority ( GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL );
        FILETIME ft;
        GetSystemTimeAsFileTime ( & ft );
        QueryPerformanceCounter ( & curPerfCounter );
        SetThreadPriority ( GetCurrentThread (), originalPriority );

        curFileTime.LowPart = ft.dwLowDateTime;
        curFileTime.HighPart = ft.dwHighDateTime;
    }

    LONGLONG perfCounterDiff;
    if ( curPerfCounter.QuadPart >= this->lastPerfCounterPLL ) {
        perfCounterDiff = curPerfCounter.QuadPart - this->lastPerfCounterPLL;
    }
    else {
        //
        // must have been a timer roll-over event
        //
        // It takes 9.223372036855e+18/perf_freq sec to roll over this
        // counter. This is currently about 245118 years using the perf
        // counter freq value on my system (1193182). Nevertheless, I
        // have code for this situation because the performance
        // counter resolution will more than likely improve over time.
        //
        perfCounterDiff = ( MAXLONGLONG - this->lastPerfCounterPLL )
                            + ( curPerfCounter.QuadPart - MINLONGLONG ) + 1;
    }
    this->lastPerfCounterPLL = curPerfCounter.QuadPart;

    LONGLONG fileTimeDiff = curFileTime.QuadPart - this->lastFileTimePLL;
    this->lastFileTimePLL = curFileTime.QuadPart;

    // discard glitches
    if ( fileTimeDiff <= 0 ) {
        LeaveCriticalSection( & this->mutex );
        debugPrintf ( ( "currentTime: file time difference in PLL was less than zero\n" ) );
        return;
    }

    LONGLONG freq = ( FILE_TIME_TICKS_PER_SEC * perfCounterDiff ) / fileTimeDiff;
    LONGLONG delta = freq - this->perfCounterFreqPLL;

    // discard glitches
    LONGLONG bound = this->perfCounterFreqPLL >> 10;
    if ( delta < -bound || delta > bound ) {
        LeaveCriticalSection( & this->mutex );
        debugPrintf ( ( "freq est out of bounds l=%d e=%d h=%d\n",
            static_cast < int > ( -bound ),
            static_cast < int > ( delta ),
            static_cast < int > ( bound ) ) );
        return;
    }

    // update feedback loop estimating the performance counter's frequency
    LONGLONG feedback = delta >> 8;
    this->perfCounterFreqPLL += feedback;

    LONGLONG perfCounterDiffSinceLastFetch;
    if ( curPerfCounter.QuadPart >= this->lastPerfCounter ) {
        perfCounterDiffSinceLastFetch =
            curPerfCounter.QuadPart - this->lastPerfCounter;
    }
    else {
        //
        // must have been a timer roll-over event
        //
        // It takes 9.223372036855e+18/perf_freq sec to roll over this
        // counter. This is currently about 245118 years using the perf
        // counter freq value on my system (1193182). Nevertheless, I
        // have code for this situation because the performance
        // counter resolution will more than likely improve over time.
        //
        perfCounterDiffSinceLastFetch =
            ( MAXLONGLONG - this->lastPerfCounter )
                            + ( curPerfCounter.QuadPart - MINLONGLONG ) + 1;
    }
    
    // discard performance counter delay measurement glitches
    {
        const LONGLONG expectedDly = this->perfCounterFreq * pllDelay;
        const LONGLONG bnd = expectedDly / 4;
        if ( perfCounterDiffSinceLastFetch <= 0 || 
                perfCounterDiffSinceLastFetch >= expectedDly + bnd ) {
            LeaveCriticalSection( & this->mutex );
            debugPrintf ( ( "perf ctr measured delay out of bounds m=%d max=%d\n",               
                static_cast < int > ( perfCounterDiffSinceLastFetch ),
                static_cast < int > ( expectedDly + bnd ) ) );
            return;
        }
    }

    // Update the current estimated time.
    this->epicsTimeLast +=
        ( perfCounterDiffSinceLastFetch * EPICS_TIME_TICKS_PER_SEC )
            / this->perfCounterFreq;
    this->lastPerfCounter = curPerfCounter.QuadPart;

    LONGLONG epicsTimeFromCurrentFileTime;
    
    {
        static bool firstMessageWasSent = false;
        if ( curFileTime.QuadPart >= epicsEpochInFileTime ) {
            epicsTimeFromCurrentFileTime =
                ( curFileTime.QuadPart - epicsEpochInFileTime ) *
                ET_TICKS_PER_FT_TICK;
            firstMessageWasSent = false;
        }
        else {
            /*
             * if the system time jumps to before the EPICS epoch
             * then latch to the EPICS epoch printing only one
             * warning message the first time that the issue is
             * detected
             */
            if ( ! firstMessageWasSent ) {
                errlogPrintf (
                    "win32 osdTime.cpp time PLL update detected questionable "
                    "system date prior to EPICS epoch, epics time will not advance\n" );
                firstMessageWasSent = true;
            }
            epicsTimeFromCurrentFileTime = 0;
        }
    }

    delta = epicsTimeFromCurrentFileTime - this->epicsTimeLast;
    if ( delta > EPICS_TIME_TICKS_PER_SEC || delta < -EPICS_TIME_TICKS_PER_SEC ) {
        // When there is an abrupt shift in the current computed time vs 
        // the time derived from the current file time then someone has 
        // probably adjusted the real time clock and the best reaction 
        // is to just assume the new time base
        this->epicsTimeLast = epicsTimeFromCurrentFileTime;
        this->perfCounterFreq = this->perfCounterFreqPLL;
        debugPrintf ( ( "currentTime: did someone adjust the date?\n" ) );
    }
    else {
        // update the effective performance counter frequency that will bring
        // our calculated time base in syncy with file time one second from now.
        this->perfCounterFreq =
            ( EPICS_TIME_TICKS_PER_SEC * this->perfCounterFreqPLL )
                / ( delta + EPICS_TIME_TICKS_PER_SEC );

        // limit effective performance counter frequency rate of change
        LONGLONG lowLimit = this->perfCounterFreqPLL - bound;
        if ( this->perfCounterFreq < lowLimit ) {
            debugPrintf ( ( "currentTime: out of bounds low freq excursion %d\n",
                static_cast <int> ( lowLimit - this->perfCounterFreq ) ) );
            this->perfCounterFreq = lowLimit;
        }
        else {
            LONGLONG highLimit = this->perfCounterFreqPLL + bound;
            if ( this->perfCounterFreq > highLimit ) {
                debugPrintf ( ( "currentTime: out of bounds high freq excursion %d\n",
                    static_cast <int> ( this->perfCounterFreq - highLimit ) ) );
                this->perfCounterFreq = highLimit;
            }
        }

#       if defined ( DEBUG )
            LARGE_INTEGER sysFreq;
            QueryPerformanceFrequency ( & sysFreq );
            double freqDiff = static_cast <int>
                ( this->perfCounterFreq - sysFreq.QuadPart );
            freqDiff /= sysFreq.QuadPart;
            freqDiff *= 100.0;
            double freqEstDiff = static_cast <int>
                ( this->perfCounterFreqPLL - sysFreq.QuadPart );
            freqEstDiff /= sysFreq.QuadPart;
            freqEstDiff *= 100.0;
            debugPrintf ( ( "currentTime: freq delta %f %% freq est "
                "delta %f %% time delta %f sec\n", 
                freqDiff, 
                freqEstDiff, 
                static_cast < double > ( delta ) / 
                        EPICS_TIME_TICKS_PER_SEC ) );
#       endif
    }

    LeaveCriticalSection ( & this->mutex );
}

static unsigned __stdcall _pllThreadEntry ( void * pCurrentTimeIn )
{
    currentTime * pCT = 
        reinterpret_cast < currentTime * > ( pCurrentTimeIn );
    setThreadName ( pCT->threadId, "EPICS Time PLL" );
    while ( ! pCT->threadShutdownCmd ) {
        Sleep ( currentTime :: pllDelay * 1000 /* mS */ );
        pCT->updatePLL ();
    }
    EnterCriticalSection ( & pCT->mutex );
    pCT->threadHasExited = true;
    LeaveCriticalSection ( & pCT->mutex );
    return 1;
}

epicsTime::operator FILETIME () const
{
    LARGE_INTEGER ftTicks;
    ftTicks.QuadPart = ( this->secPastEpoch * FILE_TIME_TICKS_PER_SEC ) +
        ( this->nSec / ET_TICKS_PER_FT_TICK );
    ftTicks.QuadPart += epicsEpochInFileTime;
    FILETIME ts;
    ts.dwLowDateTime = ftTicks.LowPart;
    ts.dwHighDateTime = ftTicks.HighPart;
    return ts;
}

epicsTime::epicsTime ( const FILETIME & ts )
{
    LARGE_INTEGER lift;
    lift.LowPart = ts.dwLowDateTime;
    lift.HighPart = ts.dwHighDateTime;
    if ( lift.QuadPart > epicsEpochInFileTime ) {
        LONGLONG fileTimeTicksSinceEpochEPICS =
            lift.QuadPart - epicsEpochInFileTime;
        this->secPastEpoch = static_cast < epicsUInt32 >
            ( fileTimeTicksSinceEpochEPICS / FILE_TIME_TICKS_PER_SEC );
        this->nSec = static_cast < epicsUInt32 >
            ( ( fileTimeTicksSinceEpochEPICS % FILE_TIME_TICKS_PER_SEC ) *
            ET_TICKS_PER_FT_TICK );
    }
    else {
        this->secPastEpoch = 0;
        this->nSec = 0;
    }
}

epicsTime & epicsTime::operator = ( const FILETIME & rhs )
{
    *this = epicsTime ( rhs );
    return *this;
}
