/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsStdioTest.c
 *
 *      Author  Marty Kraimer
 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>

#include "epicsStdio.h"
#include "epicsStdioRedirect.h"
#include "epicsUnitTest.h"

static void testEpicsSnprintf() {
    const int ivalue = 1234;
    const float fvalue = 1.23e4;
    const char *svalue = "OneTwoThreeFour";
    const char *format = "int %d float %8.2e string %s";
    const char *result = "int 1234 float 1.23e+04 string OneTwoThreeFour";
    char buffer[80];
    int size, rtn;
    int rlen = strlen(result)+1;
    
    strcpy(buffer, "AAAA");
    
    for (size = 0; size < strlen(result) + 5; ++size) {
	rtn = epicsSnprintf(buffer, size, format, ivalue, fvalue, svalue);
	testOk1(rtn == rlen-1);
	if (size) {
	    testOk(strncmp(buffer, result, size-1) == 0, buffer);
	    testOk(strlen(buffer) == (size < rlen ? size : rlen) -1, "length");
	} else {
	    testOk(strcmp(buffer, "AAAA") == 0, "Buffer unmodified, size=0");
	}
    }
}

void testStdoutRedir (const char *report)
{
    FILE *realStdout = stdout;
    FILE *stream = 0;
    
    testOk1(epicsGetStdout() == stdout);
    if(report && strlen(report)>0) {
	int fd;

	errno = 0;
	fd = open(report, O_CREAT | O_WRONLY | O_TRUNC, 0644 );
	if(fd<0) {
	    fprintf(stderr,"%s could not be created %s\n",
		report,strerror(errno));
	} else {
	    stream = fdopen(fd,"w");
	    if(!stream) {
		fprintf(stderr,"%s could not be opened for output %s\n",
		    report,strerror(errno));
	    } else {
		epicsSetThreadStdout(stream);
		testOk1(stdout == stream);
	    }
	}
    }
    printf("# This is first line of sample report\n");
    printf("# This is second and last line of sample report\n");
    errno = 0;
    if(stream) {
	epicsSetThreadStdout(0);
	if(fclose(stream)) {
	    fprintf(stderr,"fclose failed %s\n",strerror(errno));
	}
    } else {
	fflush(stdout);
    }
    testOk1(epicsGetStdout() == realStdout);
    testOk1(stdout == realStdout);
}

int epicsStdioTest (const char *report)
{
    testPlan(0);
    testEpicsSnprintf();
    testStdoutRedir("report");
    return testDone();
}
