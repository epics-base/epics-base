//
// osiTime.cc
//
// Author: Jeff Hill
//

#include <stdio.h>
#include <limits.h>
#include <time.h>

#define epicsExportSharedSymbols
#include <tsDefs.h> 
#include <osiTime.h>

//
// 1/1/90 20 yr (5 leap) of seconds
//
const unsigned osiTime::epicsEpochSecPast1970 = 7305 * 86400; 

const unsigned osiTime::mSecPerSec = 1000u;
const unsigned osiTime::nSecPerSec = 1000000000u;
const unsigned osiTime::nSecPerUSec = 1000u;
const unsigned osiTime::secPerMin = 60u;

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
// this is part of the POSIX RT standard but some OS 
// still do not define this so this code intentionally
// does not include header files that do define this
//
struct timespec {
    time_t tv_sec; /* seconds */
    long tv_nsec;  /* nanoseconds */
};

//
// operator struct timespec ()
//
osiTime::operator struct timespec () const
{
	struct timespec ts;
	assert (this->sec<=LONG_MAX-osiTime::epicsEpochSecPast1970);
	ts.tv_sec = this->sec + osiTime::epicsEpochSecPast1970;
	ts.tv_nsec = this->nSec;
	return ts;
}

//
// osiTime (const struct timespec &ts)
//
osiTime::osiTime (const struct timespec &ts)
{
	assert (ts.tv_sec >= osiTime::epicsEpochSecPast1970);
	this->sec = ((unsigned long)ts.tv_sec) - osiTime::epicsEpochSecPast1970;
	assert (ts.tv_nsec>=0 && ts.tv_nsec<osiTime::nSecPerSec);
	this->nSec = (unsigned long) ts.tv_nsec;
}

//
// operator = (const struct timespec &rhs)
//
osiTime::operator = (const struct timespec &rhs)
{
	assert (rhs.tv_sec >= osiTime::epicsEpochSecPast1970);
	this->sec = ((unsigned long)rhs.tv_sec) - osiTime::epicsEpochSecPast1970;
	assert (rhs.tv_nsec>=0 && rhs.tv_nsec<osiTime::nSecPerSec);
	this->nSec = (unsigned long) rhs.tv_nsec;
}

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

//
// operator aitTimeStamp ()
//
osiTime::operator aitTimeStamp () const
{
	aitTimeStamp ts;
	assert (this->sec<=ULONG_MAX-osiTime::epicsEpochSecPast1970);
	ts.tv_sec = this->sec + osiTime::epicsEpochSecPast1970;
	ts.tv_nsec = this->nSec;
	return ts;
}

//
// osiTime (const aitTimeStamp &ts)
//
osiTime::osiTime (const aitTimeStamp &ts) 
{
	assert (ts.tv_sec >= osiTime::epicsEpochSecPast1970);
	this->sec = ts.tv_sec - osiTime::epicsEpochSecPast1970;
	assert (ts.tv_nsec<osiTime::nSecPerSec);
	this->nSec = ts.tv_nsec;
}

//
// operator = (const aitTimeStamp &rhs)
//
osiTime::operator = (const aitTimeStamp &rhs)
{
	assert (rhs.tv_sec >= osiTime::epicsEpochSecPast1970);
	this->sec = rhs.tv_sec - osiTime::epicsEpochSecPast1970;
	assert (rhs.tv_nsec<osiTime::nSecPerSec);
	this->nSec = rhs.tv_nsec;
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
// operator = (const TS_STAMP &rhs)
//
osiTime::operator = (const struct TS_STAMP &rhs)
{
	this->sec = rhs.secPastEpoch;
	this->nSec = rhs.nsec;
}

//
// osiTime::show (unsigned)
//
void osiTime::show (unsigned) const
{
	//
	// lame way to print the time ...
	//
	printf ("osiTime: sec=%lu nSec=%lu\n", 
		this->sec, this->nSec);
}

//
// operator - 
//
// Since negative discontinuities in time are of particular 
// concern, the "A-B" operator has special code executed
// as follows:
//
// when B is greater than A:
// B-A > one half full scale => assume time stamp wrap around
// B-A <= one half full scale => return a difference of zero
//
// when A is greater than or equal to B
// A-B > one half full scale => return a difference of zero
// A-B <= one half full scale => return A-B
//
osiTime osiTime::operator - (const osiTime &rhs) const
{
	unsigned long nSec, sec;

	if (this->sec<rhs.sec) {
		if (rhs.sec-this->sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years assume that someone adjusted the time 
			// backwards slightly. This would happen if a high
			// resolution counter drift was realigned to some master
			// time source
			//
			return osiTime ();	
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" difference
			//
			sec = 1 + this->sec + (ULONG_MAX - rhs.sec);
		}
	}
	else {
		sec = this->sec - rhs.sec;
		if (sec > ULONG_MAX/2) {
			//
			// assume that someone adjusted the time 
			// backwards slightly. This would happen if a high
			// resolution counter drift was realigned to some master
			// time source
			//
			return osiTime ();	
		}
	}

	if (this->nSec>=rhs.nSec) {
		nSec = this->nSec - rhs.nSec;	
	}
	else {

		if (sec>0) {
			//
			// Borrow
			//	
			nSec = this->nSec + (osiTime::nSecPerSec - rhs.nSec);	
			sec--;
		}
		else {
			//
			// In this situation where the difference is less than
			// 69 years assume that someone adjusted the time 
			// backwards slightly. This would happen if the high
			// resolution counter drift was realigned to some master
			// time source
			//
			return osiTime ();
		}
	}
	return osiTime (sec, nSec);	
}

//
// operator <=
//
int osiTime::operator <= (const osiTime &rhs) const
{
	int	rc;

	if (this->sec<rhs.sec) {
		if (rhs.sec-this->sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years compute the expected result
			//
			rc = 1;
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" result
			//
			rc = 0;
		}
	}
	else if (this->sec>rhs.sec) {
		if (this->sec-rhs.sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years compute the expected result
			//
			rc = 0;
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" result
			//
			rc = 1;
		}
	}
	else {
		if (this->nSec<=rhs.nSec) {
			rc = 1;
		}
		else {
			rc = 0;
		}
	}
	return rc;
}

//
// operator <
//
int osiTime::operator < (const osiTime &rhs) const
{
	int	rc;

	if (this->sec<rhs.sec) {
		if (rhs.sec-this->sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years compute the expected result
			//
			rc = 1;
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" result
			//
			rc = 0;
		}
	}
	else if (this->sec>rhs.sec) {
		if (this->sec-rhs.sec < ULONG_MAX/2) {
			//
			// In this situation where the difference is less than
			// 69 years compute the expected result
			//
			rc = 0;
		}
		else {
			//
			// In this situation where the difference is more than
			// 69 years assume that the seconds counter has rolled 
			// over and compute the "wrap around" result
			//
			rc = 1;
		}
	}
	else {
		if (this->nSec<rhs.nSec) {
			rc = 1;
		}
		else {
			rc = 0;
		}
	}
	return rc;
}
