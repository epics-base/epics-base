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
 * Modification Log: 
 * -----------------
 * $Log$
 * Revision 1.7  1997/01/22 22:06:49  jhill
 * doc
 *
 * Revision 1.6  1996/11/02 01:56:55  jhill
 * improved message
 *
 * Revision 1.5  1996/06/19 17:14:11  jhill
 * print out the EPICS release when there is an assert failure
 *
 * Revision 1.4  1995/12/19  19:40:04  jhill
 * added author name
 *
 * Revision 1.3  1995/08/12  00:59:11  jhill
 * use $log in header
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "epicsAssert.h"
#include "epicsPrint.h"
#include "epicsVersion.h"


/*
 * epicsAssert ()
 */
void epicsAssert (const char *pFile, const unsigned line, const char *pMsg,
	const char *pAuthorName)
{
	epicsPrintf (
"\n\n\nA call to \"assert (%s)\" failed in %s line %d.\n", pMsg, pFile, line);

	epicsPrintf (
"The file \"core\" will be created in the current working directory.\n");

	epicsPrintf (
"Please save this file and the text of this message in order to assist\n");

	epicsPrintf (
"in diagnosing this problem.\n");

	if (pAuthorName) {

		epicsPrintf (
"Please send the text of this message to \"%s\"\n", pAuthorName);

		epicsPrintf (
"(the author of this call to assert()) or to \"tech-talk@aps.anl.gov\"\n");

	}
	else {

		epicsPrintf (
"Please contact the author of this software or else send the text of\n");

		epicsPrintf (
"this message to \"tech-talk@aps.anl.gov\"\n");

	}
	epicsPrintf ("This problem occurred in \"%s\"\n", epicsReleaseVersion);

	abort ();
}

