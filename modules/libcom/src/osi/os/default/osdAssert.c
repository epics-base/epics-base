/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author:         Jeffrey Hill
 *      Date:           02-27-95
 */

#define epicsExportSharedSymbols
#include "dbDefs.h"
#include "epicsPrint.h"
#include "epicsVersion.h"
#include "epicsAssert.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "cantProceed.h"
#include "epicsStackTrace.h"


void epicsAssert (const char *pFile, const unsigned line,
    const char *pExp, const char *pAuthorName)
{
    epicsTimeStamp current;

    errlogPrintf("\n\n\n"
        "A call to 'assert(%s)'\n"
        "    by thread '%s' failed in %s line %u.\n",
        pExp, epicsThreadGetNameSelf(), pFile, line);

    epicsStackTrace();

    errlogPrintf("EPICS Release %s.\n", epicsReleaseVersion);

    if (epicsTimeGetCurrent(&current) == 0) {
        char date[64];

        epicsTimeToStrftime(date, sizeof(date),
            "%Y-%m-%d %H:%M:%S.%f %Z", &current);
        errlogPrintf("Local time is %s\n", date);
    }

    if (!pAuthorName) {
        pAuthorName = "the author";
    }
    errlogPrintf("Please E-mail this message to %s or to tech-talk@aps.anl.gov\n",
        pAuthorName);

    errlogPrintf("Calling epicsThreadSuspendSelf()\n");
    epicsThreadSuspendSelf ();
}
