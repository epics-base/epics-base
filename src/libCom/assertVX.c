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
void epicsAssert (const char *pFile, const unsigned line, const char *pMsg,
	const char *pAuthorName)
{
	int	taskId = taskIdSelf();

        epicsPrintf (	
"\n\n\n%s: A call to \"assert (%s)\" failed in %s at %d\n", 
		taskName (taskId),
		pMsg, 
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

