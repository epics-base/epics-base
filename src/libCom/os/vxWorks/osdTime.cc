/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/


#define epicsExportSharedSymbols
#include <osiTime.h>


#include <tickLib.h>
#include <sysLib.h>

//
// osiTime::getCurrent ()
//
osiTime osiTime::getCurrent ()
{
	ULONG	ticks;
	ULONG	sec;
	ULONG	nsec;
	ULONG	rate = sysClkRateGet();
 
	ticks = tickGet();
	sec = ticks/rate;
	nsec = (ticks%rate)*(nSecPerSec/rate); 
 
	return osiTime(sec, nsec);
}

