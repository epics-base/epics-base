/* $Id$
 * assertUNIX.c
 *      Author:         Jeffrey Hill 
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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "epicsPrint.h"
#include "epicsVersion.h"
#include "epicsAssert.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "cantProceed.h"

/*
 * epicsAssert ()
 */
epicsShareFunc void epicsShareAPI 
	epicsAssert (const char *pFile, const unsigned line, 
    const char *pExp, const char *pAuthorName)
{
    epicsTimeStamp current;
    char date[64];
    int status;

	errlogPrintf (
"\n\n\nA call to \"assert (%s)\" failed in %s line %d.\n", pExp, pFile, line);

	errlogPrintf (
"EPICS release %s.\n", epicsReleaseVersion );

    status = epicsTimeGetCurrent ( & current );

    if ( status == 0 ) {

        epicsTimeToStrftime ( date, sizeof ( date ),
            "%a %b %d %Y %H:%M:%S.%f", & current );

        errlogPrintf ( 
"Current time %s.\n", date );

    }

	if ( ! pAuthorName ) {
        pAuthorName = "the author";
    }

	errlogPrintf (
"Please E-mail this message to %s or to tech-talk@aps.anl.gov\n", 
        pAuthorName );

    errlogPrintf (
"Calling epicsThreadSuspendSelf()\n" );

    epicsThreadSuspendSelf ();
}

