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
 * History
 * $Log$
 * Revision 1.8  1999/05/03 16:22:29  jhill
 * allow osiTime to convert toaitTimeStamp without binding to gdd
 *
 * Revision 1.7  1999/04/30 00:02:02  jhill
 * added getCurrentEPICS()
 *
 * Revision 1.6  1997/06/13 09:31:46  jhill
 * fixed warnings
 *
 * Revision 1.5  1997/04/10 19:45:41  jhill
 * API changes and include with  not <>
 *
 * Revision 1.4  1996/11/02 02:06:00  jhill
 * const param => #define
 *
 * Revision 1.3  1996/09/04 21:53:36  jhill
 * allow use with goofy vxWorks 5.2 time spec - which has unsigned sec and
 * signed nsec
 *
 * Revision 1.2  1996/07/09 23:01:04  jhill
 * added new operators
 *
 * Revision 1.1  1996/06/26 22:14:11  jhill
 * added new src files
 *
 * Revision 1.2  1996/06/21 02:03:40  jhill
 * added stdio.h include
 *
 * Revision 1.1.1.1  1996/06/20 22:15:56  jhill
 * installed  ca server templates
 *
 *
 */


#ifndef osiTimehInclude
#define osiTimehInclude

#include <limits.h>
#include <stdio.h>
#ifndef assert // allows use of epicsAssert.h
#include <assert.h> 
#endif

#include "shareLib.h"

struct timespec;
struct TS_STAMP;
class aitTimeStamp;

//
// class osiTime
//
// NOTE: this is an unsigned data type. It is not possible
// to store a negative time value using this class.
//
class epicsShareClass osiTime {
public:
	//
	// fetch the current time
	//
	// (always returns a time value that is greater than or equal
	// to all previous time values returned)
	//
	static osiTime getCurrent();

	//
	// some systems have a high resolution time source which
	// gradually drifts away from the master time base. Calling
	// this routine will cause osiTime to realign the high
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
	osiTime () : sec(0u), nSec(0u) {}

	//
	// create an osiTime from another osiTime
	//
	osiTime (const osiTime &t) : sec(t.sec), nSec(t.nSec) {}

	//
	// create an osiTime from sec and fractional nano-seconds
	//
	osiTime (const unsigned long secIn, const unsigned long nSecIn);

	//
	// convert to and from floating point
	//
	osiTime (double t);
	operator double() const;
	operator float() const;

	//
	// convert to and from EPICS TS_STAMP format
	//
	operator struct TS_STAMP () const;
	osiTime (const struct TS_STAMP &ts);
	operator = (const struct TS_STAMP &rhs);

	//
	// convert to and from GDD's aitTimeStamp format
	//
	operator aitTimeStamp () const;
	osiTime (const aitTimeStamp &ts);
	operator = (const aitTimeStamp &rhs);

	//
	// convert to and from POSIX RT's "struct timespec"
	//
	operator struct timespec () const;
	osiTime (const struct timespec &ts);
	operator = (const struct timespec &rhs);

	//
	// arithmetic operators
	//
	osiTime operator- (const osiTime &rhs) const;
	osiTime operator+ (const osiTime &rhs) const;
	osiTime operator+= (const osiTime &rhs);
	osiTime operator-= (const osiTime &rhs);

	//
	// comparison operators
	//
	int operator == (const osiTime &rhs) const;
	int operator != (const osiTime &rhs) const;
	int operator <= (const osiTime &rhs) const;
	int operator < (const osiTime &rhs) const;
	int operator >= (const osiTime &rhs) const;
	int operator > (const osiTime &rhs) const;

	//
	// dump current state to standard out
	//
	void show (unsigned interestLevel) const;

	//
	// useful public constants
	//
	static const unsigned nSecPerSec;
	static const unsigned mSecPerSec;
	static const unsigned nSecPerUSec;
	static const unsigned secPerMin;

	//
	// fetch value as an unsigned integer
	//
	unsigned long getSec() const;
	unsigned long getUSec() const;
	unsigned long getNSec() const;

	//
	// non standard calls for the many different
	// time formats that exist
	//
	long getSecTruncToLong() const;
	long getUSecTruncToLong() const;
	long getNSecTruncToLong() const;

private:
	unsigned long sec;
	unsigned long nSec;

	static const unsigned epicsEpochSecPast1970;
	static osiTime osdGetCurrent();
};

inline unsigned long osiTime::getSec() const
{
	return this->sec;
}

inline unsigned long osiTime::getUSec() const
{	
	return (this->nSec/nSecPerUSec);
}

inline unsigned long osiTime::getNSec() const
{	
	return this->nSec;
}

inline osiTime::osiTime (double t) 
{
	assert (t>0.0);
	this->sec = (unsigned long) t;
	this->nSec = (unsigned long) ((t-this->sec)*nSecPerSec);
}

inline osiTime::operator double() const
{
	return ((double)this->nSec)/nSecPerSec+this->sec;
}

inline osiTime::operator float() const
{
	return ((float)this->nSec)/nSecPerSec+this->sec;
}

inline osiTime osiTime::operator + (const osiTime &rhs) const
{
	return osiTime(this->sec + rhs.sec, this->nSec + rhs.nSec);	
}

inline osiTime osiTime::operator += (const osiTime &rhs)
{
	*this = *this + rhs;
	return *this;
}

inline osiTime osiTime::operator -= (const osiTime &rhs)
{
	*this = *this - rhs;
	return *this;
}

inline int osiTime::operator == (const osiTime &rhs) const
{
	if (this->sec == rhs.sec && this->nSec == rhs.nSec) {
		return 1;
	}
	else {
		return 0;
	}
}

inline int osiTime::operator != (const osiTime &rhs) const
{
	if (this->sec != rhs.sec || this->nSec != rhs.nSec) {
		return 1;
	}
	else {
		return 0;
	}
}

//
// operator >= (const osiTime &lhs, const osiTime &rhs)
//
inline int osiTime::operator >= (const osiTime &rhs) const
{
	return !(*this < rhs);
}

//
// operator > (const osiTime &lhs, const osiTime &rhs)
//
inline int osiTime::operator > (const osiTime &rhs) const
{
	return !(*this <= rhs);
}

inline long osiTime::getSecTruncToLong() const
{
	assert (this->sec<=LONG_MAX);
	return (long) this->sec;
}

inline long osiTime::getUSecTruncToLong() const
{	
	return (long) (this->nSec/nSecPerUSec);
}

inline long osiTime::getNSecTruncToLong() const
{	
	return (long) this->nSec;
}

#endif // osiTimehInclude

