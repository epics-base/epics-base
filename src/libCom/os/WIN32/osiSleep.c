/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <windows.h>

#define epicsExportSharedSymbols
#include "osiSleep.h"

epicsShareFunc void epicsShareAPI osiSleep (unsigned sec, unsigned uSec)
{
	unsigned milliSec;

	milliSec = uSec / 1000;
	milliSec += sec * 1000;

	if (milliSec==0u) {
		milliSec = 1;
	}

	Sleep (milliSec); // time-out interval in milliseconds 
}