//
// osiTime.cc
//
// Author: Jeff Hill
//


#define epicsExportSharedSymbols
//
// force this module to include code to convert to TS_STAMP
//
#include <tsDefs.h> 

//
// force this module to include code that can convert
// to GDD's aitTimeStamp, but dont force it
// to link with gdd
//
#define aitHelpersInclude
class aitTimeStamp {
public:
	aitTimeStamp (const unsigned long tv_secIn, const unsigned long tv_nsecIn) :
		tv_sec(tv_secIn), tv_nsec(tv_nsecIn) {}

	void get(unsigned long &tv_secOut, unsigned long &tv_nsecOut) const
	{
		tv_secOut = this->tv_sec;
		tv_nsecOut = this->tv_nsec;
	}
private:
	unsigned long tv_sec;
	unsigned long tv_nsec;
};

#include <osiTime.h>

//
// 1/1/90 20 yr (5 leap) of seconds
//
const unsigned osiTime::epicsEpochSecPast1970 = 7305 * 86400; 

