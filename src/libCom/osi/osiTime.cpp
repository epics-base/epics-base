/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#include <stdio.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "envDefs.h"
#include "osiTime.h"

//
// useful public constants
//
const unsigned osiTime::mSecPerSec = 1000u;
const unsigned osiTime::uSecPerSec = 1000u*osiTime::mSecPerSec;
const unsigned osiTime::nSecPerSec = 1000u*osiTime::uSecPerSec;
const unsigned osiTime::nSecPerUSec = 1000u;
const unsigned osiTime::secPerMin = 60u;

//
// this is defined by POSIX 1003.1b (POSIX real time) compilant OS
//
#ifndef CLOCK_REALTIME

	//
	// this is part of the POSIX RT standard but some OS 
	// still do not define this in time.h 
	//
	struct timespec {
		time_t tv_sec; /* seconds since some epoch */
		long tv_nsec; /* nanoseconds within the second */
	};

	struct tm *gmtime_r (const time_t *, struct tm *);
	struct tm *localtime_r (const time_t *, struct tm *);
#endif
    
#if 0
struct TS_STAMP {
    epicsUInt32    secPastEpoch;   /* seconds since 0000 Jan 1, 1990 */
    epicsUInt32    nsec;           /* nanoseconds within second */
};
#endif

static const unsigned ntpEpochYear = 1900;
static const unsigned ntpEpocMonth = 0; // January
static const unsigned ntpEpocDayOfTheMonth = 1; // the 1st day of the month
static const long double ULONG_MAX_PLUS_ONE = (static_cast<long double>(ULONG_MAX) + 1.0);

//
// force this module to include code that can convert
// to GDD's aitTimeStamp, but dont require that it must
// link with gdd. Therefore, gdd.h is not included here.
//
class aitTimeStamp {
public:
	unsigned long tv_sec;
	unsigned long tv_nsec;
};

static const unsigned tmStructEpochYear = 1900;

static const unsigned epicsEpochYear = 1990;
static const unsigned epicsEpocMonth = 0; // January
static const unsigned epicsEpocDayOfTheMonth = 1; // the 1st day of the month

//
// osiTime (const unsigned long secIn, const unsigned long nSecIn)
//
inline osiTime::osiTime (const unsigned long secIn, const unsigned long nSecIn) 
{
	this->sec = nSecIn/nSecPerSec + secIn;
	this->nSec = nSecIn%nSecPerSec;
}

//
// getCurrent ()
// 
// force a logical progression of time
//
// (this does not appear to add any significant
// overhead when the code is optimized)
// 
osiTime osiTime::getCurrent ()
{
	static osiTime last;
	osiTime ts = osiTime::osdGetCurrent();

	if (last<ts) {
		last = ts;
		return ts;
	}
	else {
		return last;
	}
}

//
// loadTimeInit
//
class loadTimeInit {
public:
	loadTimeInit ();

	long double epicsEpochOffset; // seconds
#ifdef NTP_SUPPORT
    long double ntpEpochOffset; // seconds
#endif
	long double time_tSecPerTick; // seconds (both NTP and EPICS use int sec)
};

static const loadTimeInit lti;

//
// loadTimeInit ()
//
loadTimeInit::loadTimeInit ()
{
    static const time_t ansiEpoch = 0;
	long double secWest;

	{
		time_t current = time (NULL);
		time_t error;
		tm date;

		gmtime_r (&current, &date);
		error = mktime (&date);
		assert (error!=(time_t)-1);
		secWest =  difftime (error, current);
	}
	
	{
		time_t first = static_cast<time_t> (0);
		time_t last = static_cast<time_t> (1);
		this->time_tSecPerTick = difftime (last, first);
	}

	{
		struct tm tmEpicsEpoch;
		time_t epicsEpoch;

		tmEpicsEpoch.tm_sec = 0;
		tmEpicsEpoch.tm_min = 0;
		tmEpicsEpoch.tm_hour = 0;
		tmEpicsEpoch.tm_mday = epicsEpocDayOfTheMonth;
		tmEpicsEpoch.tm_mon = epicsEpocMonth;
		tmEpicsEpoch.tm_year = epicsEpochYear-tmStructEpochYear;
		tmEpicsEpoch.tm_isdst = -1; // dont know if its daylight savings time

		epicsEpoch = mktime (&tmEpicsEpoch);
		assert (epicsEpoch!=(time_t)-1);
		this->epicsEpochOffset = difftime (epicsEpoch, ansiEpoch) - secWest;
	}

#ifdef NTP_SUPPORT
    /* unfortunately, on NT mktime cant calculate a time_t for a date before 1970 */
	{
		struct tm tmEpochNTP;
		time_t ntpEpoch;

        tmEpochNTP.tm_sec = 0;
		tmEpochNTP.tm_min = 0;
		tmEpochNTP.tm_hour = 0;
		tmEpochNTP.tm_mday = ntpEpocDayOfTheMonth;
		tmEpochNTP.tm_mon = ntpEpocMonth;
		tmEpochNTP.tm_year = ntpEpochYear-tmStructEpochYear;
		tmEpochNTP.tm_isdst = -1; // dont know if its daylight savings time

		ntpEpoch = mktime (&tmEpochNTP);
		assert (ntpEpoch!=(time_t)-1);

        this->ntpEpochOffset = static_cast<long> (difftime (ansiEpoch, ntpEpoch) + this->epicsEpochOffset - secWest);
	}
#endif
}

//
// osiTime::addNanoSec ()
//
// many of the UNIX timestamp formats have nano sec stored as a long
//
inline void osiTime::addNanoSec (long nSecAdj)
{
    //
    // for now assume that they dont allow negative nanoseconds
    // on UNIX platforms (this could be a mistake)
    //
    if ( nSecAdj < 0 ) {
        throw negNanoSecInTimeStampFromUNIX ();
    }

    unsigned long offset = static_cast<unsigned long> (nSecAdj);

    if ( offset >= nSecPerSec ) {
        throw nanoSecFieldIsTooLarge ();
    }

    //
    // no need to worry about overflow here because
    // offset + this->nSec will be less than ULONG_MAX/2
    //
    *this = osiTime (this->sec, this->nSec + offset);
}

//
// osiTime (const time_t_wrapper &tv)
//
osiTime::osiTime (const time_t_wrapper &ansiTimeTicks)
{
    static long double uLongMax = static_cast<long double>(ULONG_MAX);
	long double sec;

    //
    // map time_t, which ansi C defines as some arithmetic type, into "long double"  
    //
	sec = ansiTimeTicks.ts * lti.time_tSecPerTick - lti.epicsEpochOffset;

    //
    // map into the the EPICS time stamp range (which allows rollover)
    //
    if (sec < 0.0) {
        if (sec < -uLongMax) {
            sec = sec + static_cast<unsigned long>(-sec/uLongMax)*uLongMax;
        }
        sec += uLongMax;
    }
    else if (sec > uLongMax) {
        sec = sec - static_cast<unsigned long>(sec/uLongMax)*uLongMax;
    }

    this->sec = static_cast <unsigned long> (sec);
	this->nSec = static_cast <unsigned long> ( (sec-this->sec) * nSecPerSec );
}

//
// operator time_t_wrapper ()
//
osiTime::operator time_t_wrapper () const
{
	long double tmp;
	time_t_wrapper wrap;

    tmp = (this->sec + lti.epicsEpochOffset) / lti.time_tSecPerTick;
	tmp += (this->nSec / lti.time_tSecPerTick) / nSecPerSec;

    //
    // map "long double" into time_t which ansi C defines as some arithmetic type
    //
    wrap.ts = static_cast <time_t> (tmp);

	return wrap;
}

//
// convert to and from ANSI C struct tm (with nano seconds)
//
osiTime::operator tm_nano_sec () const
{
	tm_nano_sec tm;
	time_t_wrapper ansiTimeTicks;
    struct tm *p;

	ansiTimeTicks = *this;

	// reentrant version of localtime() - from POSIX RT
    // WRS prototype is incorrect ?
	p = localtime_r (&ansiTimeTicks.ts, &tm.ansi_tm);
    if (p != &tm.ansi_tm) {
        throw internalFailure ();
    }

	tm.nSec = this->nSec;

	return tm;
}

//
// osiTime (const tm_nano_sec &tm)
//
osiTime::osiTime (const tm_nano_sec &tm)
{
	time_t_wrapper ansiTimeTicks;
	struct tm tmp = tm.ansi_tm;

	ansiTimeTicks.ts = mktime (&tmp);
    if (ansiTimeTicks.ts==(time_t)-1) {
        throw formatProblemWithStructTM ();
    }

    *this = osiTime (ansiTimeTicks);

    if (tm.nSec>=nSecPerSec) {
        throw nanoSecFieldIsTooLarge ();
    }
    *this = osiTime (this->sec, this->nSec + tm.nSec);
}

//
// operator struct timespec ()
//
osiTime::operator struct timespec () const
{
	struct timespec ts;
	time_t_wrapper ansiTimeTicks;

	ansiTimeTicks = *this;
	ts.tv_sec = ansiTimeTicks.ts;
	ts.tv_nsec = static_cast<long> (this->nSec);
	return ts;
}

//
// osiTime (const struct timespec &ts)
//
osiTime::osiTime (const struct timespec &ts)
{
	time_t_wrapper ansiTimeTicks;

    ansiTimeTicks.ts = ts.tv_sec;
    *this = osiTime (ansiTimeTicks);
    this->addNanoSec (ts.tv_nsec);
}

//
// operator struct timeval ()
//
osiTime::operator struct timeval () const
{
	struct timeval ts;
	time_t_wrapper ansiTimeTicks;

	ansiTimeTicks = *this;
	ts.tv_sec = ansiTimeTicks.ts;
    ts.tv_usec = static_cast<long> (this->nSec / nSecPerUSec);
	return ts;
}

//
// osiTime (const struct timeval &ts)
//
osiTime::osiTime (const struct timeval &ts)
{
	time_t_wrapper ansiTimeTicks;

    ansiTimeTicks.ts = ts.tv_sec;
    *this = osiTime (ansiTimeTicks);
    this->addNanoSec (ts.tv_usec * nSecPerUSec);
}

//
// operator aitTimeStamp ()
//
osiTime::operator aitTimeStamp () const
{
	aitTimeStamp ts;
	time_t_wrapper ansiTimeTicks;

	ansiTimeTicks = *this;
	ts.tv_sec = ansiTimeTicks.ts;
	ts.tv_nsec = this->nSec;
	return ts;
}

//
// osiTime (const aitTimeStamp &ts)
//
osiTime::osiTime (const aitTimeStamp &ts) 
{
    time_t_wrapper ansiTimeTicks;

    ansiTimeTicks.ts = ts.tv_sec;
    *this = osiTime (ansiTimeTicks);

    if ( ts.tv_nsec>=nSecPerSec ) {
        throw nanoSecFieldIsTooLarge ();
    }
    *this = osiTime ( this->sec, this->nSec + ts.tv_nsec );
}

//
// operator TS_STAMP ()
//
osiTime::operator struct TS_STAMP () const
{
	struct TS_STAMP ts;
	ts.secPastEpoch = this->sec;
	ts.nsec = this->nSec;
	return ts;
}

//
// osiTime (const TS_STAMP &ts)
//
osiTime::osiTime (const struct TS_STAMP &ts) 
{
	this->sec = ts.secPastEpoch;
	this->nSec = ts.nsec;
}

//
// osiTime::ntpTimeStamp ()
//
#ifdef NTP_SUPPORT
osiTime::operator ntpTimeStamp () const
{
    ntpTimeStamp ts;

    if (lti.ntpEpochOffset>=0) {
        unsigned long offset = static_cast<unsigned long> (lti.ntpEpochOffset);
        // underflow expected
        ts.l_ui = this->sec - offset;
    }
    else {
        unsigned long offset = static_cast<unsigned long> (-lti.ntpEpochOffset);
        // overflow expected
        ts.l_ui = this->sec + offset;
    }

    ts.l_uf = static_cast<unsigned long> ( ( this->nSec * ULONG_MAX_PLUS_ONE ) / nSecPerSec );

    return ts;
}
#endif

//
// osiTime::osiTime (const ntpTimeStamp &ts)
//
#ifdef NTP_SUPPORT
osiTime::osiTime (const ntpTimeStamp &ts)
{

    if (lti.ntpEpochOffset>=0) {
        unsigned long offset = static_cast<unsigned long> (lti.ntpEpochOffset);
        // overflow expected
        this->sec = ts.l_ui + this->sec + offset;
    }
    else {
        unsigned long offset = static_cast<unsigned long> (-lti.ntpEpochOffset);
        // underflow expected
        this->sec = ts.l_ui + this->sec - offset;
    }

    this->nSec = static_cast<unsigned long> ( ( ts.l_uf / ULONG_MAX_PLUS_ONE ) * nSecPerSec );
}
#endif

//
// osiTime::show (unsigned)
//
void osiTime::show (unsigned) const
{
	int status;
	char bigBuffer[256];
	tm_nano_sec tmns = *this;

	status = strftime (bigBuffer, sizeof(bigBuffer), "%a %b %d %H:%M:%S %Y", &tmns.ansi_tm);
	if (status>0) {
		printf ("osiTime: %s %g\n", bigBuffer, 
			static_cast <double> (tmns.nSec) / nSecPerSec);
	}
}

//
// osiTime::operator + (const long double &rhs)
//
// rhs has units seconds
//
osiTime osiTime::operator + (const long double &rhs) const
{
	unsigned long newSec, newNSec, secOffset, nSecOffset;
	long double fnsec;

	if (rhs >= 0) {
		secOffset = static_cast <unsigned long> (rhs);
		fnsec = rhs - secOffset;
		nSecOffset = static_cast <unsigned long> (fnsec * nSecPerSec);

		newSec = this->sec + secOffset; // overflow expected
		newNSec = this->nSec + nSecOffset;
		if (newNSec >= nSecPerSec) {
			newSec++; // overflow expected
			newNSec -= nSecPerSec;
		}
	}
	else {
		secOffset = static_cast <unsigned long> (-rhs);
		fnsec = rhs + secOffset;
		nSecOffset = static_cast <unsigned long> (-fnsec * nSecPerSec);

		newSec = this->sec - secOffset; // underflow expected
		if (this->nSec>=nSecOffset) {
			newNSec = this->nSec - nSecOffset;
		}
		else {
			// borrow
			newSec--; // underflow expected
			newNSec = this->nSec + (nSecPerSec - nSecOffset);
		}
	}
	return osiTime (newSec, newNSec);
}

//
// operator - 
//
// To make this code robust during timestamp rollover events
// time stamp differences greater than one half full scale are 
// interpreted as rollover situations:
//
// when RHS is greater than THIS:
// RHS-THIS > one half full scale => return THIS + (ULONG_MAX-RHS)
// RHS-THIS <= one half full scale => return -(RHS-THIS)
//
// when THIS is greater than or equal to RHS
// THIS-RHS > one half full scale => return -(RHS + (ULONG_MAX-THIS))
// THIS-RHS <= one half full scale => return THIS-RHS
//
long double osiTime::operator - (const osiTime &rhs) const
{
	long double nSecRes, secRes;

	//
	// first compute the difference between the nano-seconds members
	//
	// nano sec member is not allowed to be greater that 1/2 full scale
	// so the unsigned to signed conversion is ok
	//
	if (this->nSec>=rhs.nSec) {
		nSecRes = this->nSec - rhs.nSec;	
	}
	else {
		nSecRes = rhs.nSec - this->nSec;
		nSecRes = -nSecRes;
	}

	//
	// next compute the difference between the seconds memebers
	// and invert the sign of the nano seconds result if there
	// is a range violation
	//
	if (this->sec<rhs.sec) {
		secRes = rhs.sec - this->sec;
		if (secRes > ULONG_MAX/2) {
			//
			// In this situation where the difference is more than
			// 68 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" difference
			//
			secRes = 1 + (ULONG_MAX-secRes);
			nSecRes = -nSecRes;
		}
		else {
			secRes = -secRes;
		}
	}
	else {
		secRes = this->sec - rhs.sec;
		if (secRes > ULONG_MAX/2) {
			//
			// In this situation where the difference is more than
			// 68 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" difference
			//
			secRes = 1 + (ULONG_MAX-secRes);
			secRes = -secRes;
			nSecRes = -nSecRes;
		}
	}

	return secRes + nSecRes/nSecPerSec;	
}

//
// operator <=
//
bool osiTime::operator <= (const osiTime &rhs) const
{
	bool rc;

	if (this->sec<rhs.sec) {
		if (rhs.sec-this->sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years compute the expected result
			//
			rc = true;
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" result
			//
			rc = false;
		}
	}
	else if (this->sec>rhs.sec) {
		if (this->sec-rhs.sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years compute the expected result
			//
			rc = false;
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" result
			//
			rc = true;
		}
	}
	else {
		if (this->nSec<=rhs.nSec) {
			rc = true;
		}
		else {
			rc = false;
		}
	}
	return rc;
}

//
// operator <
//
bool osiTime::operator < (const osiTime &rhs) const
{
	bool	rc;

	if (this->sec<rhs.sec) {
		if (rhs.sec-this->sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years compute the expected result
			//
			rc = true;
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" result
			//
			rc = false;
		}
	}
	else if (this->sec>rhs.sec) {
		if (this->sec-rhs.sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years compute the expected result
			//
			rc = false;
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" result
			//
			rc = true;
		}
	}
	else {
		if (this->nSec<rhs.nSec) {
			rc = true;
		}
		else {
			rc = false;
		}
	}
	return rc;
}

extern "C" {
    /*
     * ANSI C interface
     *
     * its too bad that these cant be implemented with inline functions 
     * at least when running the GNU compiler
     */
    epicsShareFunc void osiTimeGetCurrent (osiTime *pDest)
    {
        *pDest = osiTime::getCurrent();
    }
    epicsShareFunc void osiTimeSynchronize ()
    {
        osiTime::synchronize ();
    }
    epicsShareFunc int osiTimeToTS_STAMP (TS_STAMP *pDest, const osiTime *pSrc)
    {
        try {
            *pDest = *pSrc;
        }
        catch (...) {
            return -1;
        }
        return 0;
    }
    epicsShareFunc int osiTimeFromTS_STAMP (osiTime *pDest, const TS_STAMP *pSrc)
    {
        try {
            *pDest = *pSrc;
        }
        catch (...) {
            return -1;
        }
        return 0;
    }
    epicsShareFunc int osiTimeToTime_t (time_t *pDest, const osiTime *pSrc)
    {
        try {
            time_t_wrapper dst;
            dst = *pSrc;
            *pDest = dst.ts;
        }
        catch (...) {
            return -1;
        }
        return 0;
    }
    epicsShareFunc int osiTimeFromTime_t (osiTime *pDest, time_t src)
    {
        try {
            time_t_wrapper dst;
            dst.ts = src;
            *pDest = dst;
        }
        catch (...) {
            return -1;
        }
        return 0;
    }
    epicsShareFunc int osiTimeToTM (tm_nano_sec *pDest, const osiTime *pSrc)
    {
        try {
            *pDest = *pSrc;
        }
        catch (...) {
            return -1;
        }
        return 0;
    }
    epicsShareFunc int osiTimeFromTM (osiTime *pDest, const tm_nano_sec *pSrc)
    {
        try {
            *pDest = *pSrc;
        }
        catch (...) {
            return -1;
        }
        return 0;
    }
    epicsShareFunc int osiTimeToTimespec (struct timespec *pDest, const osiTime *pSrc)
    {
        try {
            *pDest = *pSrc;
        }
        catch (...) {
            return -1;
        }
        return 0;
    }
    epicsShareFunc int osiTimeFromTimespec (osiTime *pDest, const struct timespec *pSrc)
    {
        try {
            *pDest = *pSrc;
        }
        catch (...) {
            return -1;
        }
        return 0;
    }
    epicsShareFunc long double osiTimeDiffInSeconds (const osiTime *pLeft, const osiTime *pRight)
    {
        return *pLeft - *pRight;
    }
    epicsShareFunc void osiTimeAddSeconds (osiTime *pDest, long double seconds)
    {
        *pDest += seconds;
    }
    epicsShareFunc int osiTimeEqual (const osiTime *pLeft, const osiTime *pRight)
    {
        return *pLeft == *pRight;
    }
    epicsShareFunc int osiTimeNotEqual (const osiTime *pLeft, const osiTime *pRight)
    {
        return *pLeft != *pRight;
    }
    epicsShareFunc int osiTimeLessThan (const osiTime *pLeft, const osiTime *pRight)
    {
        return *pLeft < *pRight;
    }
    epicsShareFunc int osiTimeLessThanEqual (const osiTime *pLeft, const osiTime *pRight)
    {
        return *pLeft <= *pRight;
    }
    epicsShareFunc int osiTimeGreaterThan (const osiTime *pLeft, const osiTime *pRight)
    {
        return *pLeft > *pRight;
    }
    epicsShareFunc int osiTimeGreaterThanEqual (const osiTime *pLeft, const osiTime *pRight)
    {
        return *pLeft >= *pRight;
    }
    epicsShareFunc void osiTimeShow (const osiTime *pTS, unsigned interestLevel)
    {
        pTS->show (interestLevel);
    }
}
