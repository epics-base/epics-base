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
 * Modification Log: 
 * -----------------
 * $Log$
 * Revision 1.5  1996/06/19 17:14:12  jhill
 * print out the EPICS release when there is an assert failure
 *
 * Revision 1.4  1995/12/19  19:40:07  jhill
 * added author name
 *
 * Revision 1.3  1995/08/12  01:00:07  jhill
 *  use $log in header
 *
 ***************************************************************************
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <vxWorks.h>
#include <taskLib.h>

#include <epicsAssert.h>
#include <epicsPrint.h>
#include <epicsVersion.h>


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

