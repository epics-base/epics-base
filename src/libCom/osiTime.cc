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
#ifndef assert // allow other versions of assert
#include <assert.h>
#endif

#define epicsExportSharedSymbols
#include <tsDefs.h> 
#include <osiTime.h>
#include <envDefs.h>

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
osiTime::osiTime (const unsigned long secIn, const unsigned long nSecIn) 
{
	if (nSecIn<nSecPerSec) {
		this->sec = secIn;
		this->nSec = nSecIn;
	}
	else if (nSecIn<(nSecPerSec*2)){
		this->sec = secIn + 1u;
		this->nSec = nSecIn-nSecPerSec;
	}
	else {
		this->sec = nSecIn/nSecPerSec + secIn;
		this->nSec = nSecIn%nSecPerSec;
	}
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

	long epicsEpochOffset; // integer seconds
	long double time_tTicksPerSec;
};

static const loadTimeInit lti;

//
// loadTimeInit ()
//
loadTimeInit::loadTimeInit ()
{
	long secWest;

	{
		time_t current = time (NULL);
		time_t error;
		tm date;

		gmtime_r (&current, &date);
		error = mktime (&date);
		secWest = static_cast<long> (difftime (error, current));
	}
	
	{
		time_t first = static_cast<time_t> (0);
		time_t last = static_cast<time_t> (1);
		this->time_tTicksPerSec = 1.0 / difftime (last, first);
	}

	{
		struct tm tmEpicsEpoch;
		time_t epicsEpoch;
		time_t ansiEpoch = 0;

		tmEpicsEpoch.tm_sec = 0;
		tmEpicsEpoch.tm_min = 0;
		tmEpicsEpoch.tm_hour = 0;
		tmEpicsEpoch.tm_mday = epicsEpocDayOfTheMonth;
		tmEpicsEpoch.tm_mon = epicsEpocMonth;
		tmEpicsEpoch.tm_year = epicsEpochYear-tmStructEpochYear;
		tmEpicsEpoch.tm_isdst = -1; // dont know if its daylight savings time

		epicsEpoch = mktime (&tmEpicsEpoch);
		//
		// when this happens we will need to write the code which
		// subtract the tm structures ourselves
		//
		assert (epicsEpoch!=(time_t)-1);
			
		this->epicsEpochOffset = static_cast<long> (difftime (epicsEpoch, ansiEpoch));
		this->epicsEpochOffset -= secWest;
	}
}

//
// ansiSecToInternalSec ()
//
unsigned long osiTime::time_tToInternalSec (const time_t &ansiTimeTicks)
{
	unsigned long sec;

	sec = static_cast<unsigned long> (ansiTimeTicks / lti.time_tTicksPerSec);

	// expect over / under flow
	if (lti.epicsEpochOffset>=0) {
		sec -= static_cast<unsigned long>(lti.epicsEpochOffset);
	}
	else {
		sec += static_cast<unsigned long>(-lti.epicsEpochOffset);
	}

	return sec;
}

//
// operator time_t_wrapper ()
//
osiTime::operator time_t_wrapper () const
{
	long double tmp;
	unsigned long newSec;
	time_t_wrapper wrap;

	// expect over/under flow and allow it to occur befor proceeding
	if (lti.epicsEpochOffset>=0) {
		newSec = this->sec + static_cast<unsigned long>(lti.epicsEpochOffset);
	}
	else {
		newSec = this->sec + static_cast<unsigned long>(-lti.epicsEpochOffset);
	}
	tmp = newSec * lti.time_tTicksPerSec;
	tmp += (this->nSec * lti.time_tTicksPerSec) / nSecPerSec;
	wrap.ts = static_cast<time_t> (tmp);
	return wrap;
}

//
// convert to and from ANSI C struct tm (with nano seconds)
//
osiTime::operator tm_nano_sec () const
{
	struct tm_nano_sec tm;
	time_t_wrapper ansiTimeTicks;

	ansiTimeTicks = *this;

	// from POSIX RT
	localtime_r (&ansiTimeTicks.ts, &tm.tm);

	tm.nsec = this->nSec;

	return tm;
}

//
// osiTime (const struct tm_nano_sec &tm)
//
osiTime::osiTime (const struct tm_nano_sec &tm)
{
	time_t ansiTimeTicks;
	struct tm tmp = tm.tm;

	ansiTimeTicks = mktime (&tmp);
	assert (ansiTimeTicks!=(time_t)-1);

	this->sec = osiTime::time_tToInternalSec (ansiTimeTicks);
	this->nSec = tm.nsec;
}

//
// operator struct timespec ()
//
inline osiTime::operator struct timespec () const
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
inline osiTime::osiTime (const struct timespec &ts)
{
	this->sec = osiTime::time_tToInternalSec (ts.tv_sec);
	assert (ts.tv_nsec>=0);
	unsigned long nSec = static_cast<unsigned long> (ts.tv_nsec);
	if (nSec<nSecPerSec) {
		this->nSec = nSec;
	}
	else {
		this->sec += nSec / nSecPerSec;
		this->nSec = nSec % nSecPerSec;
	}
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
	this->sec = osiTime::time_tToInternalSec (ts.tv_sec);

	if (ts.tv_nsec<nSecPerSec) {
		this->nSec = ts.tv_nsec;
	}
	else {
		this->sec += ts.tv_nsec / nSecPerSec;
		this->nSec = ts.tv_nsec % nSecPerSec;
	}
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
// osiTime::show (unsigned)
//
void osiTime::show (unsigned) const
{
	int status;
	char bigBuffer[256];
	tm_nano_sec tmns = *this;

	status = strftime (bigBuffer, sizeof(bigBuffer), "%c", &tmns.tm);
	if (status>0) {
		printf ("osiTime: %s %f\n", bigBuffer, 
			static_cast <double> (tmns.nsec) / nSecPerSec);
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
		fnsec = rhs - static_cast <long double> (secOffset);
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
		fnsec = rhs + static_cast <long double> (secOffset);
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

