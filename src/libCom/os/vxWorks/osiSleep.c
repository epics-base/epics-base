/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <taskLib.h>
#include <sysLib.h>

#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "osiSleep.h"

epicsShareFunc void epicsShareAPI osiSleep (unsigned sec, unsigned uSec)
{
	ULONG ticks;
	int status;
	
	ticks = (uSec*sysClkRateGet())/1000000;
	ticks += sec*sysClkRateGet();

	if (ticks==0) {
		ticks = 1;
	}

	status = taskDelay (ticks);
	assert (status==OK);
}