/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 * assertVX.c
 *      Author:         Jeff Hill
 *      Date:           02-27-95 
 ***************************************************************************
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <taskLib.h>

#define epicsExportSharedSymbols
#include "epicsThread.h"
#include "epicsPrint.h"
#include "epicsVersion.h"
#include "epicsAssert.h"
#include "epicsTime.h"

/*
 * epicsAssert ()
 *
 * This forces assert failures into the log file and then
 * calls epicsThreadSuspendSelf() instead of exit() so that we can debug
 * the problem.
 */
epicsShareFunc void epicsShareAPI epicsAssert (const char *pFile, const unsigned line, const char *pExp,
	const char *pAuthorName)
{
    epicsThreadId threadid = epicsThreadGetIdSelf();
    epicsTimeStamp current;
    char date[64];
    int status;

    epicsPrintf (	
"\n\n\n%s: A call to \"assert (%s)\" failed in %s at %d\n", 
		taskName ((int)threadid),
		pExp, 
		pFile, 
		line);

    status = epicsTimeGetCurrent ( & current );
    if ( status == 0 ) {
        epicsTimeToStrftime ( date, sizeof ( date ),
            "%a %b %d %Y %H:%M:%S.%f", & current );
        epicsPrintf ( 
"Current time %s.\n", date );
    }

	epicsPrintf (
"EPICS Release %s.\n", epicsReleaseVersion );

	if ( ! pAuthorName ) {
        pAuthorName = "the author";
    }

    epicsPrintf (	
"Please E-mail this message and the output from \"tt (%p)\"\n",
		threadid );

    epicsPrintf (	
"to %s or to tech-talk@aps.anl.gov\n", 
		pAuthorName);

    epicsThreadSuspendSelf ();
}

