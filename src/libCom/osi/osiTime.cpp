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

#include <stddef.h>
#include <stdio.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "envDefs.h"
#include "osiTime.h"
#include "tsStamp.h"

//
// useful public constants
//
const unsigned osiTime::mSecPerSec = 1000u;
const unsigned osiTime::uSecPerSec = 1000u*osiTime::mSecPerSec;
const unsigned osiTime::nSecPerSec = 1000u*osiTime::uSecPerSec;
const unsigned osiTime::nSecPerUSec = 1000u;
const unsigned osiTime::secPerMin = 60u;

static const unsigned ntpEpochYear = 1900;
static const unsigned ntpEpocMonth = 0; // January
static const unsigned ntpEpocDayOfTheMonth = 1; // the 1st day of the month
static const double ULONG_MAX_PLUS_ONE = (static_cast<double>(ULONG_MAX) + 1.0);

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
	this->secPastEpoch = nSecIn/nSecPerSec + secIn;
	this->nSec = nSecIn%nSecPerSec;
}

//
// loadTimeInit
//
class loadTimeInit {
public:
	loadTimeInit ();

	double epicsEpochOffset; // seconds
#ifdef NTP_SUPPORT
    double ntpEpochOffset; // seconds
#endif
	double time_tSecPerTick; // seconds (both NTP and EPICS use int sec)
};

static const loadTimeInit lti;

//
// loadTimeInit ()
//
loadTimeInit::loadTimeInit ()
{
    static const time_t ansiEpoch = 0;
	double secWest;

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
    double secAdj = static_cast <double> (nSecAdj) / nSecPerSec;
    *this += secAdj;
}

//
// osiTime (const time_t_wrapper &tv)
//
osiTime::osiTime (const time_t_wrapper &ansiTimeTicks)
{
    static double uLongMax = static_cast<double> (ULONG_MAX);
	double sec;

    //
    // map time_t, which ansi C defines as some arithmetic type, into type double 
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

    this->secPastEpoch = static_cast <unsigned long> (sec);
	this->nSec = static_cast <unsigned long> ( (sec-this->secPastEpoch) * nSecPerSec );
}

//
// operator time_t_wrapper ()
//
osiTime::operator time_t_wrapper () const
{
	double tmp;
	time_t_wrapper wrap;

    tmp = (this->secPastEpoch + lti.epicsEpochOffset) / lti.time_tSecPerTick;
	tmp += (this->nSec / lti.time_tSecPerTick) / nSecPerSec;

    //
    // map type double into time_t which ansi C defines as some arithmetic type
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

	ansiTimeTicks = *this;

    //
	// reentrant version of localtime() - from POSIX RT
    //
    // WRS returns int and others return &tm.ansi_tm on
    // succes?
    //
	localtime_r (&ansiTimeTicks.ts, &tm.ansi_tm);

	tm.nSec = this->nSec;

	return tm;
}

//
// osiTime (const tm_nano_sec &tm)
//
osiTime::osiTime (const tm_nano_sec &tm)
{
    static const time_t mktimeFailure = static_cast<time_t> (-1);
	time_t_wrapper ansiTimeTicks;
	struct tm tmp = tm.ansi_tm;

	ansiTimeTicks.ts = mktime (&tmp);
    if (ansiTimeTicks.ts ==  mktimeFailure) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else            
            throw formatProblemWithStructTM ();
#       endif
    }

    *this = osiTime (ansiTimeTicks);

    unsigned long nSecAdj = tm.nSec % nSecPerSec;
    unsigned long secAdj = tm.nSec / nSecPerSec;
    *this = osiTime (this->secPastEpoch+secAdj, this->nSec+nSecAdj);
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

    unsigned long secAdj = ts.tv_nsec / nSecPerSec;
    unsigned long nSecAdj = ts.tv_nsec % nSecPerSec;
    *this = osiTime (this->secPastEpoch+secAdj, this->nSec+nSecAdj);
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
        ts.l_ui = this->secPastEpoch - offset;
    }
    else {
        unsigned long offset = static_cast<unsigned long> (-lti.ntpEpochOffset);
        // overflow expected
        ts.l_ui = this->secPastEpoch + offset;
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
        this->secPastEpoch = ts.l_ui + this->secPastEpoch + offset;
    }
    else {
        unsigned long offset = static_cast<unsigned long> (-lti.ntpEpochOffset);
        // underflow expected
        this->secPastEpoch = ts.l_ui + this->secPastEpoch - offset;
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
// osiTime::operator + (const double &rhs)
//
// rhs has units seconds
//
osiTime osiTime::operator + (const double &rhs) const
{
	unsigned long newSec, newNSec, secOffset, nSecOffset;
	double fnsec;

	if (rhs >= 0) {
		secOffset = static_cast <unsigned long> (rhs);
		fnsec = rhs - secOffset;
		nSecOffset = static_cast <unsigned long> (fnsec * nSecPerSec);

		newSec = this->secPastEpoch + secOffset; // overflow expected
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

		newSec = this->secPastEpoch - secOffset; // underflow expected
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
double osiTime::operator - (const osiTime &rhs) const
{
	double nSecRes, secRes;

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
	if (this->secPastEpoch<rhs.secPastEpoch) {
		secRes = rhs.secPastEpoch - this->secPastEpoch;
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
		secRes = this->secPastEpoch - rhs.secPastEpoch;
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

	if (this->secPastEpoch<rhs.secPastEpoch) {
		if (rhs.secPastEpoch-this->secPastEpoch < ULONG_MAX/2) {
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
	else if (this->secPastEpoch>rhs.secPastEpoch) {
		if (this->secPastEpoch-rhs.secPastEpoch < ULONG_MAX/2) {
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

	if (this->secPastEpoch<rhs.secPastEpoch) {
		if (rhs.secPastEpoch-this->secPastEpoch < ULONG_MAX/2) {
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
	else if (this->secPastEpoch>rhs.secPastEpoch) {
		if (this->secPastEpoch-rhs.secPastEpoch < ULONG_MAX/2) {
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
    //
    // ANSI C interface
    //
    // its too bad that these cant be implemented with inline functions 
    // at least when running the GNU compiler
    //
    epicsShareFunc int epicsShareAPI tsStampToTime_t (time_t *pDest, const TS_STAMP *pSrc)
    {
#       ifdef noExceptionsFromCXX
            time_t_wrapper dst = osiTime (*pSrc);
            *pDest = dst.ts;
#       else
            try {
                time_t_wrapper dst = osiTime (*pSrc);
                *pDest = dst.ts;
            }
            catch (...) {
                return tsStampERROR;
            }
#       endif
        return tsStampOK;
    }
    epicsShareFunc int epicsShareAPI tsStampFromTime_t (TS_STAMP *pDest, time_t src)
    {
        time_t_wrapper dst;
        dst.ts = src;
#       ifdef noExceptionsFromCXX
            *pDest = osiTime (dst);
#       else
            try {
                *pDest = osiTime (dst);
            }
            catch (...) {
                return tsStampERROR;
            }
#       endif
        return tsStampOK;
    }
    epicsShareFunc int epicsShareAPI tsStampToTM (struct tm *pDest, unsigned long *pNSecDest, const TS_STAMP *pSrc)
    {
        tm_nano_sec tmns;
#       ifdef noExceptionsFromCXX
            tmns = osiTime (*pSrc);
#       else
            try {
                tmns = osiTime (*pSrc);
            }
            catch (...) {
                return tsStampERROR;
            }
#       endif       
        *pDest = tmns.ansi_tm;
        *pNSecDest = tmns.nSec;
        return tsStampOK;
    }
    epicsShareFunc int epicsShareAPI tsStampFromTM (TS_STAMP *pDest, const struct tm *pSrc, unsigned long nSecSrc)
    {
        tm_nano_sec tmns;
        tmns.ansi_tm = *pSrc;
        tmns.nSec = nSecSrc;

#       ifdef noExceptionsFromCXX
            *pDest = osiTime (tmns);
#       else
            try {
                *pDest = osiTime (tmns);
            }
            catch (...) {
                return tsStampERROR;
            }
#       endif
        return tsStampOK;
    }
    epicsShareFunc int epicsShareAPI tsStampToTimespec (struct timespec *pDest, const TS_STAMP *pSrc)
    {
#       ifdef noExceptionsFromCXX
            *pDest = osiTime (*pSrc);
#       else
            try {
                *pDest = osiTime (*pSrc);
            }
            catch (...) {
                return tsStampERROR;
            }
#       endif
        return tsStampOK;
    }
    epicsShareFunc int epicsShareAPI tsStampFromTimespec (TS_STAMP *pDest, const struct timespec *pSrc)
    {
#       ifdef noExceptionsFromCXX
            *pDest = osiTime (*pSrc);
#       else
            try {
                *pDest = osiTime (*pSrc);
            }
            catch (...) {
                return tsStampERROR;
            }
#       endif
        return tsStampOK;
    }
    epicsShareFunc double epicsShareAPI tsStampDiffInSeconds (const TS_STAMP *pLeft, const TS_STAMP *pRight)
    {
        return osiTime (*pLeft) - osiTime (*pRight);
    }
    epicsShareFunc void epicsShareAPI tsStampAddSeconds (TS_STAMP *pDest, double seconds)
    {
        *pDest = osiTime (*pDest) + seconds;
    }
    epicsShareFunc int epicsShareAPI tsStampEqual (const TS_STAMP *pLeft, const TS_STAMP *pRight)
    {
        return osiTime (*pLeft) == osiTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI tsStampNotEqual (const TS_STAMP *pLeft, const TS_STAMP *pRight)
    {
        return osiTime (*pLeft) != osiTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI tsStampLessThan (const TS_STAMP *pLeft, const TS_STAMP *pRight)
    {
        return osiTime (*pLeft) < osiTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI tsStampLessThanEqual (const TS_STAMP *pLeft, const TS_STAMP *pRight)
    {
        return osiTime (*pLeft) <= osiTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI tsStampGreaterThan (const TS_STAMP *pLeft, const TS_STAMP *pRight)
    {
        return osiTime (*pLeft) > osiTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI tsStampGreaterThanEqual (const TS_STAMP *pLeft, const TS_STAMP *pRight)
    {
        return osiTime (*pLeft) >= osiTime (*pRight);
    }
    epicsShareFunc void epicsShareAPI tsStampShow (const TS_STAMP *pTS, unsigned interestLevel)
    {
        osiTime(*pTS).show (interestLevel);
    }
}
