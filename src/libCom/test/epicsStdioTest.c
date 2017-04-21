/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
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
#include "epicsUnitTest.h"
#include "testMain.h"


#define LINE_1 "# This is first line of sample report\n"
#define LINE_2 "# This is second and last line of sample report\n"

static void testEpicsSnprintf(void) {
    char exbuffer[80], buffer[80];
    const int ivalue = 1234;
    const float fvalue = 1.23e4f;
    const char *svalue = "OneTwoThreeFour";
    const char *format = "int %d float %8.2e string %s";
    const char *expected = exbuffer;
    int size;
    int rtn, rlen;

#ifdef _WIN32
#if (defined(_MSC_VER) && _MSC_VER < 1900) || \
    (defined(_MINGW) && defined(_TWO_DIGIT_EXPONENT))
    _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
#endif

    sprintf(exbuffer, format, ivalue, fvalue, svalue);
    rlen = strlen(expected)+1;
    
    strcpy(buffer, "AAAA");
    
    for (size = 1; size < strlen(expected) + 5; ++size) {
        rtn = epicsSnprintf(buffer, size, format, ivalue, fvalue, svalue);
        testOk(rtn <= rlen-1, "epicsSnprintf(size=%d) = %d", size, rtn);
        if (rtn != rlen-1)
            testDiag("Return value does not indicate buffer size needed");
        testOk(strncmp(buffer, expected, size - 1) == 0,
            "buffer = '%s'", buffer);
        rtn = strlen(buffer);
        testOk(rtn == (size < rlen ? size : rlen) - 1,
            "length = %d", rtn);
    }
}

void testStdoutRedir (const char *report)
{
    FILE *realStdout = stdout;
    FILE *stream = 0;
    char linebuf[80];
    size_t buflen = sizeof linebuf;
    
    testOk1(epicsGetStdout() == stdout);

    errno = 0;
    if (!testOk1((stream = fopen(report, "w")) != NULL)) {
        testDiag("'%s' could not be opened for writing: %s",
                 report, strerror(errno));
        testSkip(11, "Can't create stream file");
        return;
    }

    epicsSetThreadStdout(stream);
    testOk1(stdout == stream);

    printf(LINE_1);
    printf(LINE_2);

    epicsSetThreadStdout(0);
    testOk1(epicsGetStdout() == realStdout);
    testOk1(stdout == realStdout);

    errno = 0;
    if (!testOk1(!fclose(stream))) {
        testDiag("fclose error: %s", strerror(errno));
#ifdef vxWorks
        testDiag("The above test fails if you don't cd to a writable directory");
        testDiag("before running the test. The next test will also fail...");
#endif
    }

    if (!testOk1((stream = fopen(report, "r")) != NULL)) {
        testDiag("'%s' could not be opened for reading: %s",
                 report, strerror(errno));
        testSkip(6, "Can't reopen stream file.");
        return;
    }

    if (!testOk1(fgets(linebuf, buflen, stream) != NULL)) {
        testDiag("File read error: %s", strerror(errno));
        testSkip(5, "Read failed.");
        fclose(stream);
        return;
    }
    testOk(strcmp(linebuf, LINE_1) == 0, "First line correct");

    if (!testOk1(fgets(linebuf, buflen, stream) != NULL)) {
        testDiag("File read error: %s", strerror(errno));
        testSkip(1, "No line to compare.");
    } else
        testOk(strcmp(linebuf, LINE_2) == 0, "Second line");

    testOk(!fgets(linebuf, buflen, stream), "File ends");

    if (!testOk1(!fclose(stream)))
        testDiag("fclose error: %s\n", strerror(errno));
}

MAIN(epicsStdioTest)
{
    testPlan(163);
    testEpicsSnprintf();
#ifdef __rtems__
    /* ensure there is a writeable area */
    mkdir( "/tmp", S_IRWXU );
    testStdoutRedir("/tmp/report");
#else
    testStdoutRedir("report");
#endif
    return testDone();
}
