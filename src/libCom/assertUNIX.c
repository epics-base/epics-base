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


/*
 * epicsAssert ()
 */
epicsShareFunc void epicsShareAPI 
	epicsAssert (const char *pFile, const unsigned line, const char *pMsg,
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

