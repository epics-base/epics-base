/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#define epicsExportSharedSymbols
#include "epicsStdio.h"

#ifndef SEEK_END
#define SEEK_END 2
#endif

/*
 * truncate to specified size (we dont use truncate()
 * because it is not portable)
 */
epicsShareFunc enum TF_RETURN  truncateFile (const char *pFileName, unsigned long size)
{
	char tmpName[256>L_tmpnam?256:L_tmpnam];
	long filePos;
	FILE *pFile;
	FILE *ptmp;
	int status;
	int c;
	unsigned charNo;

	/*
	 * see cast of size to long below
	 */
	if (size>LONG_MAX) {
		return TF_ERROR;
	}
	
	pFile = fopen(pFileName, "r");
	if (!pFile) {
		fprintf (stderr,
			"File access problems to `%s' because `%s'\n", 
			pFileName,
			strerror(errno));
		return TF_ERROR;
	}

	/*
	 * This is not required under UNIX but does appear
	 * to be required under WIN32. 
	 */
	status = fseek (pFile, 0L, SEEK_END);
	if (status!=TF_OK) {
		fclose (pFile);
		return TF_ERROR;
	}

	filePos = ftell(pFile);
	if (filePos <= (long) size) {
		fclose (pFile);
		return TF_OK;
	}

    epicsTempName ( tmpName, sizeof (tmpName) );
	if ( tmpName[0] == '\0' ) {
		fprintf (stderr,"Unable to create tmp file name?\n");
		fclose (pFile);
		return TF_ERROR;
	}

	ptmp = fopen (tmpName, "w");
	if (!ptmp) {
		fprintf (stderr,
			"File access problems to `%s' because `%s'\n", 
			tmpName,
			strerror(errno));
		fclose (pFile);
		return TF_ERROR;
	}
	rewind (pFile);
	charNo = 0u;
	while (charNo<size) {
		c = getc (pFile);
		if (c==EOF) {
			fprintf (stderr,
				"File access problems to `%s' because `%s'\n", 
				pFileName,
				strerror(errno));
			fclose (pFile);
			fclose (ptmp);
			remove (tmpName);
			return TF_ERROR;
		}
		status = putc (c, ptmp);
		if (status==EOF) {
			fprintf(stderr,
				"File access problems to `%s' because `%s'\n", 
				tmpName,
				strerror(errno));
			fclose (pFile);
			fclose (ptmp);
			remove (tmpName);
			return TF_ERROR;
		}
		charNo++;
	}
	fclose (pFile);
	fclose (ptmp);
	status = remove (pFileName);
	if (status!=TF_OK) {
		fprintf (stderr,
			"Unable to remove `%s' during truncate because `%s'\n", 
			pFileName,
			strerror(errno));
		remove (tmpName);
		return TF_ERROR;
	}
	status = rename (tmpName, pFileName);
	if (status!=TF_OK) {
		fprintf (stderr,
			"Unable to rename %s to `%s' because `%s'\n", 
			tmpName,
			pFileName,
			strerror(errno));
		remove (tmpName);
		return TF_ERROR;
	}
	return TF_OK;
}

