/* $Id$
 * assertVX.c
 *      Author:         Jeff Hill
 *      Date:           02-27-95 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
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

