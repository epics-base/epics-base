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
 *
 */

#ifndef osiTimehInclude
#define osiTimehInclude

//
// ANSI C
//
#include <time.h>

#include "shareLib.h"

struct TS_STAMP; // EPICS
class aitTimeStamp; // GDD
struct timespec; // POSIX real time

//
// an extended ANSI C RTL "struct tm" which includes nano seconds. 
//
struct tm_nano_sec {
	tm ansi_tm; // ANSI C time details
	unsigned long nsec; // nano seconds extension
};

//
// wrapping this in a struct allows conversion to and
// from ANSI time_t but does not allow unexpected
// conversions to occur
//
struct time_t_wrapper {
	time_t ts;
};

//
// class osiTime
//
// high resolution osiTime stamp
//
class epicsShareClass osiTime {
public:

	//
	// fetch the current time
	//
	static osiTime getCurrent();

	//
	// some systems have a high resolution time source which
	// gradually drifts away from the master time base. Calling
	// this routine will cause class "time" to realign the high
	// resolution result from getCurrent() with some master 
	// time base. 
	//
	// if this routine has not been called at least once so far
	// by the current process then it is called the first time 
	// that getCurrent() is called
	//
	static void synchronize();

	//
	// create an osiTime for the EPICS epoch
	//
	osiTime ();
	osiTime (const osiTime &t);

	//
	// convert to and from EPICS TS_STAMP format
	//
	operator struct TS_STAMP () const;
	osiTime (const struct TS_STAMP &ts);
	osiTime operator = (const struct TS_STAMP &rhs);

	//
	// convert to and from ANSI C's "time_t"
	//
	// "time_t" is wrapped in another structure to avoid
	// unsuspected type conversions
	// fetch value as an integer
	//
	operator time_t_wrapper () const;
	osiTime (const time_t_wrapper &tv);
	osiTime operator = (const time_t_wrapper &rhs);

	//
	// convert to and from ANSI C's "struct tm" (with nano seconds)
	//
	operator struct tm_nano_sec () const;
	osiTime (const struct tm_nano_sec &ts);
	osiTime operator = (const struct tm_nano_sec &rhs);

	//
	// convert to and from POSIX RT's "struct timespec"
	//
	operator struct timespec () const;
	osiTime (const struct timespec &ts);
	osiTime operator = (const struct timespec &rhs);

	//
	// convert to and from GDD's aitTimeStamp format
	//
	operator aitTimeStamp () const;
	osiTime (const aitTimeStamp &ts);
	osiTime operator = (const aitTimeStamp &rhs);

	//
	// arithmetic operators
	//
	long double operator- (const osiTime &rhs) const; // returns seconds
	osiTime operator+ (const long double &rhs) const; // add rhs seconds
	osiTime operator- (const long double &rhs) const; // subtract rhs seconds
	osiTime operator+= (const long double &rhs); // add rhs seconds
	osiTime operator-= (const long double &rhs); // subtract rhs seconds

	//
	// comparison operators
	//
	bool operator == (const osiTime &rhs) const;
	bool operator != (const osiTime &rhs) const;
	bool operator <= (const osiTime &rhs) const;
	bool operator < (const osiTime &rhs) const;
	bool operator >= (const osiTime &rhs) const;
	bool operator > (const osiTime &rhs) const;

	//
	// dump current state to standard out
	//
	void show (unsigned interestLevel) const;

	//
	// useful public constants
	//
	static const unsigned secPerMin;
	static const unsigned mSecPerSec;
	static const unsigned uSecPerSec;
	static const unsigned nSecPerSec;
	static const unsigned nSecPerUSec;

private:
	unsigned long sec; /* seconds since O000 Jan 1, 1990 */
	unsigned long nSec; /* nanoseconds within second */

	static osiTime osdGetCurrent();
	static unsigned long time_tToInternalSec (const time_t &tv);

	//
	// private because:
	// a) application does not break when EPICS epoch is changed
	// b) no assumptions about internal storage or internal precision
	// in the application
	// c) it would be easy to forget which argument is nanoseconds
	// and which argument is seconds (no help from compiler)
	//
	osiTime (unsigned long sec, unsigned long nSec);
};

/////////////////////////////////////
//
// time inline member functions
//
/////////////////////////////////////

inline osiTime::osiTime () : sec(0u), nSec(0u) {}	

inline osiTime::osiTime (const osiTime &t) : sec(t.sec), nSec(t.nSec) {}

inline osiTime osiTime::operator - (const long double &rhs) const
{
	return osiTime::operator + (-rhs);
}

inline osiTime osiTime::operator += (const long double &rhs)
{
	*this = osiTime::operator + (rhs);
	return *this;
}

inline osiTime osiTime::operator -= (const long double &rhs)
{
	*this = osiTime::operator + (-rhs);
	return *this;
}

inline bool osiTime::operator == (const osiTime &rhs) const
{
	if (this->sec == rhs.sec && this->nSec == rhs.nSec) {
		return true;
	}
	else {
		return false;
	}
}

//
// operator !=
//
inline bool osiTime::operator != (const osiTime &rhs) const
{
	return !osiTime::operator == (rhs);
}

//
// operator >= (const osiTime &lhs, const osiTime &rhs)
//
inline bool osiTime::operator >= (const osiTime &rhs) const
{
	return !(*this < rhs);
}

//
// operator > (const osiTime &lhs, const osiTime &rhs)
//
inline bool osiTime::operator > (const osiTime &rhs) const
{
	return !(*this <= rhs);
}

//
// osiTime operator = (const struct tm_nano_sec &rhs)
//
inline osiTime osiTime::operator = (const struct tm_nano_sec &rhs)
{
	return *this = osiTime (rhs);
}

//
// operator = (const struct timespec &rhs)
//
inline osiTime osiTime::operator = (const struct timespec &rhs)
{
	*this = osiTime (rhs);
	return *this;
}

//
// operator = (const aitTimeStamp &rhs)
//
inline osiTime osiTime::operator = (const aitTimeStamp &rhs)
{
	*this = osiTime (rhs);
	return *this;
}


inline osiTime osiTime::operator = (const struct TS_STAMP &rhs)
{
	*this = osiTime (rhs);
	return *this;
}

//
// osiTime (const time_t_wrapper &tv)
//
inline osiTime::osiTime (const time_t_wrapper &ansiTimeTicks)
{
	this->sec = osiTime::time_tToInternalSec (ansiTimeTicks.ts);
	this->nSec = 0;
}

//
// osiTime operator = (const time_t_wrapper &rhs)
// operator >= (const osiTime &lhs, const osiTime &rhs)
//
inline osiTime osiTime::operator = (const time_t_wrapper &rhs)
{
	*this = osiTime (rhs);
	return *this;
}

#endif // osiTimehInclude

