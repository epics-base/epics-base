/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// should move the gettimeofday() proto for VMS
// into a different header
//
#include "osiSock.h"

#define epicsExportSharedSymbols
#include "epicsTime.h"
#include "generalTimeSup.h"

//
// epicsTime::osdGetCurrent ()
//
int osdTimeGetCurrent (epicsTimeStamp *pDest)
{
#   if defined(CLOCK_REALTIME)
        struct timespec ts;
        int status;
    
        status = clock_gettime (CLOCK_REALTIME, &ts);
        if (status) {
            return -1;
        }
        *pDest = epicsTime (ts);
        return 0;
#   else
    	int status;
    	struct timeval tv;
    
    	status = gettimeofday (&tv, NULL);
        if (status!=0) {
            return -1;
        }
    	*pDest = epicsTime (tv); 
        return 0;
#   endif
}

extern "C" epicsShareFunc int epicsShareAPI osdTimeInit (void)
{
#if defined (CLOCK_REALTIME)
	const char name[] = "gettimeofday";
#else
	const char name[] = "clock_gettime";
#endif

	generalTimeCurrentTpRegister(name, 150, osdTimeGetCurrent);
	return epicsTimeOK;
}
