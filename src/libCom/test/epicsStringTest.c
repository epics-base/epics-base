/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* $Revision-Id$
 *
 *      Author  Marty Kraimer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "epicsUnitTest.h"
#include "epicsString.h"
#include "testMain.h"

void testChars(void) {
    int i;
    char input[2] = {0, 0};
    char escaped[20];
    char result[20];
    size_t s, t, needed;

    for (i = 255; i >= 0; --i) {
        input[0] = i;
        needed = epicsStrnEscapedFromRawSize(input, 1);
        s = epicsStrnEscapedFromRaw(escaped, sizeof(escaped), input, 1);
        t = epicsStrnRawFromEscaped(result, sizeof(result), escaped, s);
        testOk(needed == s && t == 1 &&
            result[0] == input[0] && result[1] == 0,
            "escaped char 0x%2.2x -> \"%s\" (%d) -> 0x%2.2x",
            input[0] & 0xff, escaped, (int) needed, result[0] & 0xff);
    }
}

MAIN(epicsStringTest)
{
    const char * const empty = "";
    const char * const space = " ";
    const char * const A     = "A";
    const char * const ABCD  = "ABCD";
    const char * const ABCDE = "ABCDE";
    const char * const a     = "a";
    const char * const abcd  = "abcd";
    const char * const abcde = "abcde";
    char *s;

    testPlan(281);

    testChars();

    testOk1(epicsStrnCaseCmp(empty, "", 0) == 0);
    testOk1(epicsStrnCaseCmp(empty, "", 1) == 0);
    testOk1(epicsStrnCaseCmp(space, empty, 1) < 0);
    testOk1(epicsStrnCaseCmp(empty, space, 1) > 0);
    testOk1(epicsStrnCaseCmp(a, A, 1) == 0);
    testOk1(epicsStrnCaseCmp(a, A, 2) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCD, 2) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCD, 4) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCD, 1000) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCDE, 2) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCDE, 4) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCDE, 1000)> 0);
    testOk1(epicsStrnCaseCmp(abcde, ABCD, 2) == 0);
    testOk1(epicsStrnCaseCmp(abcde, ABCD, 4) == 0);
    testOk1(epicsStrnCaseCmp(abcde, ABCD, 1000) < 0);

    testOk1(epicsStrCaseCmp(empty, "") == 0);
    testOk1(epicsStrCaseCmp(a, A) == 0);
    testOk1(epicsStrCaseCmp(abcd, ABCD) == 0);
    testOk1(epicsStrCaseCmp(abcd, ABCDE) != 0);
    testOk1(epicsStrCaseCmp(abcde, ABCD) != 0);
    testOk1(epicsStrCaseCmp(abcde, "ABCDF") != 0);

    s = epicsStrDup(abcd);
    testOk(strcmp(s, abcd) == 0 && s != abcd, "epicsStrDup");
    free(s);

    testOk1(epicsStrHash(abcd, 0) != epicsStrHash("bacd", 0));
    testOk1(epicsStrHash(abcd, 0) == epicsMemHash(abcde, 4, 0));
    testOk1(epicsStrHash(abcd, 0) != epicsMemHash("abcd\0", 5, 0));

    return testDone();
}
