//
//      $Id$
//
//      Author  Jeffrey O. Hill
//              johill@lanl.gov
//              505 665 1831
//
//      Experimental Physics and Industrial Control System (EPICS)
//
//      Copyright 1991, the Regents of the University of California,
//      and the University of Chicago Board of Governors.
//
//      This software was produced under  U.S. Government contracts:
//      (W-7405-ENG-36) at the Los Alamos National Laboratory,
//      and (W-31-109-ENG-38) at Argonne National Laboratory.
//
//      Initial development by:
//              The Controls and Automation Group (AT-8)
//              Ground Test Accelerator
//              Accelerator Technology Division
//              Los Alamos National Laboratory
//
//      Co-developed with
//              The Controls and Computing Group
//              Accelerator Systems Division
//              Advanced Photon Source
//              Argonne National Laboratory
//
//
//

#ifndef osiTimehInclude
#define osiTimehInclude

//
// ANSI
//
#include <String>
#include <time.h>

#include "shareLib.h"
#include "epicsTypes.h"
#include "tsStamp.h"
#include "epicsAssert.h"

struct timespec; // POSIX real time 
struct timeval; // BSD 
class aitTimeStamp; // GDD

//
// an extended ANSI C RTL "struct tm" which includes 
// additional nano seconds within a second. 
//
struct tm_nano_sec {
	struct tm ansi_tm; /* ANSI C time details */
	unsigned long nSec; /* nano seconds extension */
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
// This comment is from the NTP header files:
//
// NTP uses two fixed point formats.  The first (l_fp) is the "long"
// format and is 64 bits long with the decimal between bits 31 and 32.
// This is used for time stamps in the NTP packet header (in network
// byte order) and for internal computations of offsets (in local host
// byte order). We use the same structure for both signed and unsigned
// values, which is a big hack but saves rewriting all the operators
// twice. Just to confuse this, we also sometimes just carry the
// fractional part in calculations, in both signed and unsigned forms.
// Anyway, an l_fp looks like:
//
//    0			  1		      2			  3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |			       Integral Part			     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |			       Fractional Part			     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//
struct ntpTimeStamp {
    epicsUInt32 l_ui; /* sec past NTP epoch */
    epicsUInt32 l_uf; /* fractional seconds */
};

class osiTime;

//
// type osiTimeEvent
//
class epicsShareClass osiTimeEvent
{
    friend class osiTime;
public:
    osiTimeEvent (const int &eventName);
private:
    unsigned eventNumber;
};

//
// type osiTime
//
// high resolution osiTime stamp class (struct) which can be used
// from both C++ and also from ANSI C
//
class epicsShareClass osiTime
{
public:

    //
    // exceptions
    //
    class unableToFetchCurrentTime {};
    class formatProblemWithStructTM {};

	//
	// fetch the current time
	//
	static osiTime getEvent (const osiTimeEvent &event);
	static osiTime getCurrent ();

	//
	// create an osiTime for the EPICS epoch
	//
	osiTime ();
	osiTime (const osiTime &t);

	//
	// convert to and from EPICS TS_STAMP format
	//
	operator TS_STAMP () const;
	osiTime (const TS_STAMP &ts);
	osiTime operator = (const TS_STAMP &rhs);

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
	operator tm_nano_sec () const;
	osiTime (const tm_nano_sec &ts);
	osiTime operator = (const tm_nano_sec &rhs);

	//
	// convert to and from POSIX RT's "struct timespec"
	//
	operator struct timespec () const;
	osiTime (const struct timespec &ts);
	osiTime operator = (const struct timespec &rhs);

	//
	// convert to and from BSD's "struct timeval"
	//
	operator struct timeval () const;
	osiTime (const struct timeval &ts);
	osiTime operator = (const struct timeval &rhs);

    //
    // convert to and from NTP timestamp format
    //
	operator ntpTimeStamp () const;
	osiTime (const ntpTimeStamp &ts);
	osiTime operator = (const ntpTimeStamp &rhs);

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

	// depricated
	static void synchronize ();

private:

	//
	// private because:
	// a) application does not break when EPICS epoch is changed
	// b) no assumptions about internal storage or internal precision
	// in the application
	// c) it would be easy to forget which argument is nanoseconds
	// and which argument is seconds (no help from compiler)
	//
	osiTime (const unsigned long secPastEpoch, const unsigned long nSec);
    void addNanoSec (long nanoSecAdjust);

	unsigned long secPastEpoch; // seconds since O000 Jan 1, 1990 
	unsigned long nSec; // nanoseconds within second 
};

//
// type osiTime inline member functions
//

//
// getCurrent ()
// 
inline osiTime osiTime::getCurrent ()
{
    TS_STAMP current;
    int status;

	status = tsStampGetCurrent (&current);
    if (status) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else
            throw unableToFetchCurrentTime ();
#       endif
    }

    return osiTime (current);
}

inline osiTime osiTime::getEvent (const osiTimeEvent &event)
{
    TS_STAMP current;
    int status;

	status = tsStampGetEvent (&current, event.eventNumber);
    if (status) {
#       ifdef noExceptionsFromCXX
            assert (0);
#       else
            throw unableToFetchCurrentTime ();
#       endif
    }

    return osiTime (current);
}

// depricated
inline void osiTime::synchronize () {}

inline osiTime::osiTime () : secPastEpoch(0u), nSec(0u) {}	

inline osiTime::osiTime (const osiTime &t) : secPastEpoch (t.secPastEpoch), nSec (t.nSec) {}

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
	if (this->secPastEpoch == rhs.secPastEpoch && this->nSec == rhs.nSec) {
		return true;
	}
	else {
		return false;
	}
}

inline bool osiTime::operator != (const osiTime &rhs) const
{
	return !osiTime::operator == (rhs);
}

inline bool osiTime::operator >= (const osiTime &rhs) const
{
	return !(*this < rhs);
}

inline bool osiTime::operator > (const osiTime &rhs) const
{
	return !(*this <= rhs);
}

inline osiTime osiTime::operator = (const tm_nano_sec &rhs)
{
	return *this = osiTime (rhs);
}

inline osiTime osiTime::operator = (const struct timespec &rhs)
{
	*this = osiTime (rhs);
	return *this;
}

inline osiTime osiTime::operator = (const aitTimeStamp &rhs)
{
	*this = osiTime (rhs);
	return *this;
}

inline osiTime::osiTime (const TS_STAMP &ts) 
{
	this->secPastEpoch = ts.secPastEpoch;
	this->nSec = ts.nsec;
}

inline osiTime osiTime::operator = (const TS_STAMP &rhs)
{
	*this = osiTime (rhs);
	return *this;
}

inline osiTime::operator TS_STAMP () const
{
    TS_STAMP ts;
    ts.secPastEpoch = this->secPastEpoch;
    ts.nsec = this->nSec;
    return ts;
}

#ifdef NTP_SUPPORT
inline osiTime osiTime::operator = (const ntpTimeStamp &rhs)
{
    *this = osiTime (rhs);
    return *this;
}
#endif

inline osiTime osiTime::operator = (const time_t_wrapper &rhs)
{
	*this = osiTime (rhs);
	return *this;
}

#endif // osiTimehInclude 

