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

#define nSecPerSec 1000000000u
#define nSecPerUSec 1000u
#define secPerMin 60u

#include "shareLib.h"

class epicsShareClass osiTime {
	friend inline osiTime operator+ 
		(const osiTime &lhs, const osiTime &rhs);
	friend inline osiTime operator- 
		(const osiTime &lhs, const osiTime &rhs);
	friend inline int operator>= 
		(const osiTime &lhs, const osiTime &rhs);
	friend inline int operator> 
		(const osiTime &lhs, const osiTime &rhs);
	friend inline int operator<=
		(const osiTime &lhs, const osiTime &rhs);
	friend inline int operator< 
		(const osiTime &lhs, const osiTime &rhs);
public:
	osiTime () : sec(0u), nSec(0u) {}
	osiTime (const osiTime &t) : sec(t.sec), nSec(t.nSec) {}
	osiTime (const unsigned long secIn, const unsigned long nSecIn) 
	{
		if (nSecIn<nSecPerSec) {
			this->sec = secIn;
			this->nSec = nSecIn;
		}
		else if (nSecIn<(nSecPerSec<<1u)){
			this->sec = secIn + 1u;
			this->nSec = nSecIn-nSecPerSec;
		}
		else {
			this->sec = nSecIn/nSecPerSec + secIn;
			this->nSec = nSecIn%nSecPerSec;
		}
	}

	osiTime (double t) 
	{
		double intPart;
		if (t<0.0l) {
			t = 0.0l;
		}
		this->sec = (unsigned long) t;
		intPart = (double) this->sec;
		this->nSec = (unsigned long) ((t-intPart)*nSecPerSec);
	}

	//
	// fetch value as an integer
	//
	unsigned long getSec() const
	{
		return this->sec;
	}
	unsigned long getUSec() const
	{	
		return (this->nSec/nSecPerUSec);
	}
	unsigned long getNSec() const
	{	
		return this->nSec;
	}

	//
	// non standard calls for the many strange
	// time formats that still exist
	//
	long getSecTruncToLong() const
	{
		assert (this->sec<=LONG_MAX);
		return (long) this->sec;
	}
	long getUSecTruncToLong() const
	{	
		return (long) (this->nSec/nSecPerUSec);
	}
	long getNSecTruncToLong() const
	{	
		assert (this->nSec<=LONG_MAX);
		return (long) this->nSec;
	}


	operator double() const
	{
		return ((double)this->nSec)/nSecPerSec+this->sec;
	}

	operator float() const
	{
		return ((float)this->nSec)/nSecPerSec+this->sec;
	}

	static osiTime getCurrent();

	osiTime operator+= (const osiTime &rhs);
	osiTime operator-= (const osiTime &rhs);

	void show(unsigned)
	{
		printf("osiTime: sec=%lu nSec=%lu\n", 
			this->sec, this->nSec);
	}
private:
	unsigned long sec;
	unsigned long nSec;
};

inline osiTime operator+ (const osiTime &lhs, const osiTime &rhs)
{
	return osiTime(lhs.sec + rhs.sec, lhs.nSec + rhs.nSec);	
}

inline osiTime osiTime::operator+= (const osiTime &rhs)
{
	*this = *this + rhs;
	return *this;
}

//
// like data type unsigned this assumes that the lhs > rhs
// (otherwise we assume sec wrap around)
//
inline osiTime operator- (const osiTime &lhs, const osiTime &rhs)
{
	unsigned long nSec, sec;

	if (lhs.sec<rhs.sec) {
		//
		// wrap around
		//
		sec = lhs.sec + (ULONG_MAX - rhs.sec);
	}
	else {
		sec = lhs.sec - rhs.sec;
	}

	if (lhs.nSec<rhs.nSec) {
		//
		// Borrow
		//
		nSec = lhs.nSec + (nSecPerSec - rhs.nSec);	
		sec--;
	}
	else {
		nSec = lhs.nSec - rhs.nSec;	
	}
	return osiTime(sec, nSec);	
}

inline osiTime osiTime::operator-= (const osiTime &rhs)
{
	*this = *this - rhs;
	return *this;
}

inline int operator <= (const osiTime &lhs, const osiTime &rhs)
{
	int	rc;
//
// Sun's CC -O generates bad code when this was rearranged 
//
	if (lhs.sec<rhs.sec) {
		rc = 1;
	}
	else if (lhs.sec>rhs.sec) {
		rc = 0;
	}
	else {
		if (lhs.nSec<=rhs.nSec) {
			rc = 1;
		}
		else {
			rc = 0;
		}
	}
	return rc;
}

inline int operator < (const osiTime &lhs, const osiTime &rhs)
{
	int	rc;
//
// Sun's CC -O generates bad code when this was rearranged 
//
	if (lhs.sec<rhs.sec) {
		rc = 1;
	}
	else if (lhs.sec>rhs.sec) {
		rc = 0;
	}
	else {
		if (lhs.nSec<rhs.nSec) {
			rc = 1;
		}
		else {
			rc = 0;
		}
	}
	return rc;
}

inline int operator >= (const osiTime &lhs, const osiTime &rhs)
{
	int	rc;
//
// Sun's CC -O generates bad code here
//
//
//
#if 0
	if (lhs.sec>rhs.sec) {
		return 1;
	}
	else if (lhs.sec==rhs.sec) {
		if (lhs.nSec>=rhs.nSec) {
			return 1;
		}
	}
	assert(lhs.sec<=rhs.sec);
	return 0;
#endif
	if (lhs.sec>rhs.sec) {
		rc = 1;
	}
	else if (lhs.sec<rhs.sec) {
		rc = 0;
	}
	else {
		if (lhs.nSec>=rhs.nSec) {
			rc = 1;
		}
		else {
			rc = 0;
		}
	}
	return rc;
}

inline int operator > (const osiTime &lhs, const osiTime &rhs)
{
	int	rc;
//
// Sun's CC -O generates bad code when this was rearranged 
//
	if (lhs.sec>rhs.sec) {
		rc = 1;
	}
	else if (lhs.sec<rhs.sec) {
		rc = 0;
	}
	else {
		if (lhs.nSec>rhs.nSec) {
			rc = 1;
		}
		else {
			rc = 0;
		}
	}
	return rc;
}

#endif // osiTimehInclude

