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
// should move the time stuff into a different header
//
#include "osiSock.h"

#define epicsExportSharedSymbols
#include "osiTime.h"
 

//
// osiTime::getCurrent ()
//
osiTime osiTime::getCurrent ()
{
	int             status;
	struct timeval  tv;

	status = gettimeofday (&tv, NULL);
	assert (status==0);

	return osiTime(tv.tv_sec, tv.tv_usec * nSecPerUSec);
}

