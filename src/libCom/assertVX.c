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

#include <vxWorks.h>
#include <taskLib.h>

#define epicsExportSharedSymbols
#include "epicsPrint.h"
#include "epicsVersion.h"
#include "epicsAssert.h"

/*
 * epicsAssert ()
 *
 * This forces assert failures into the log file and then
 * calls taskSuspend() instead of exit() so that we can debug
 * the problem.
 */
void epicsAssert (const char *pFile, const unsigned line, const char *pExp,
	const char *pAuthorName)
{
	int	taskId = taskIdSelf();

    epicsPrintf (	
"\n\n\n%s: A call to \"assert (%s)\" failed in %s at %d\n", 
		taskName (taskId),
		pExp, 
		pFile, 
		line);

	if (pAuthorName) {

        	epicsPrintf (	
"Please send a copy of the output from \"tt (0x%x)\" and a copy of this message\n",
		taskId);

        	epicsPrintf (	
"to \"%s\" (the author of this call to assert()) or \"tech-talk@aps.anl.gov\"\n", 
		pAuthorName);

	}
	else {

        	epicsPrintf (	
"Please send a copy of the output from \"tt (0x%x)\" and a copy of this message\n",
		taskId);

        	epicsPrintf (	
"to the author or \"tech-talk@aps.anl.gov\"\n");

	}
	epicsPrintf ("This problem occurred in \"%s\"\n", epicsReleaseVersion);

    taskSuspend (taskId);
}

