/*************************************************************************\
* Copyright (c) 2025 Hinko Kocevar
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <osiFileName.h>
#include <epicsUnitTest.h>
#include <testMain.h>

#include "epicsReadline.h"

MAIN(readlineTest)
{
    const char *input = ".." OSI_PATH_SEPARATOR "multiline-input.txt";
    const char *expect = ".." OSI_PATH_SEPARATOR "multiline-expect.txt";

    testPlan(5);

    testDiag("open input file \"%s\"", input);
    FILE *fp_input = fopen(input, "r");
    if (fp_input == NULL) {
        testAbort("unable to open \"%s\"", input);
        return -1;
    }

    testDiag("open expect file \"%s\"", expect);
    FILE *fp_expect = fopen(expect, "r");
    if (fp_expect == NULL) {
        testAbort("unable to open \"%s\"", expect);
        return -1;
    }

    void *context = epicsReadlineBegin(fp_input);

    char *line_input = NULL;
    char c = 0;
    int icin = 0;
    char line_expect[500];

    while (1) {
        // get string from input (can be multi-line delimited by \)
        if ((line_input = epicsReadline(NULL, context)) == NULL) {
            // EOF reached or some other low level error
            break;
        }
        icin = 0;
        while ((c = line_input[icin]) && isspace(c)) {
            icin++;
        }
        // ignore empty lines and comments
        if (!c || c == '#') {
            continue;
        }
        
        do {
            // expected strings are always single line strings
            if (! fgets(line_expect, 499, fp_expect)) {
                testAbort("failed to read an expected line");
            }
            icin = 0;
            while ((c = line_expect[icin]) && isspace(c)) {
                icin++;
            }
            // ignore empty lines and comments
        } while (!c || c == '#');

        // fgets() stores \n if present; get rid of it
        size_t len_expect = strlen(line_expect);
        if (line_expect[len_expect-1] == '\n') {
            line_expect[len_expect-1] = '\0';
            len_expect -= 1;
        }
        size_t len_input = strlen(line_input);

        if (len_input != len_expect) {
            testAbort("lines are not of same length: input %ld, expected %ld", len_input, len_expect);
        }
        if (strncmp(line_input, line_expect, len_expect)) {
            testAbort("lines are not the same:\ninput:\"%s\"\nexpected: \"%s\"", line_input, line_expect);
        }
        testOk(1, "line \"%s\"", line_input);
    }

    epicsReadlineEnd(context);
    fclose(fp_input);
    fclose(fp_expect);

    return testDone();
}
