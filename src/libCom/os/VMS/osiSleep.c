/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <string.h>

#include "osiSock.h"
#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "osiSleep.h"

/*
 * should work correctly on VMS, but there
 * is probably a more efficent native call ...
 */
epicsShareFunc void epicsShareAPI osiSleep (unsigned sec, unsigned uSec)
{
	int status;
	struct timeval tval;

	assert (uSec<1000000);
	tval.tv_sec = sec;
	tval.tv_usec = uSec;
	status = select (0, NULL, NULL, NULL, &tval);
	if (status<0) {
		fprintf (stderr, "error from select in osiDelayMicroSec() was %s\n",
			SOCKERRSTR);
	}
}