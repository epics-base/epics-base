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
 * assertUNIX.c
 *      Author:         Jeffrey Hill 
 *      Date:           02-27-95
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
"EPICS Release %s.\n", epicsReleaseVersion );

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

