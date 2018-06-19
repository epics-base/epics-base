/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author  Marty Kraimer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "epicsUnitTest.h"
#include "epicsString.h"
#include "testMain.h"

static
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

static
void testGlob(void) {
    testOk1(epicsStrGlobMatch("xyz","xyz"));
    testOk1(!epicsStrGlobMatch("xyz","xyzm"));
    testOk1(!epicsStrGlobMatch("xyzm","xyz"));
    testOk1(!epicsStrGlobMatch("","xyzm"));
    testOk1(!epicsStrGlobMatch("xyz",""));
    testOk1(epicsStrGlobMatch("",""));

    testOk1(epicsStrGlobMatch("","*"));
    testOk1(!epicsStrGlobMatch("","?"));
    testOk1(!epicsStrGlobMatch("","?*"));

    testOk1(epicsStrGlobMatch("hello","h*o"));
    testOk1(!epicsStrGlobMatch("hello","h*x"));
    testOk1(!epicsStrGlobMatch("hellx","h*o"));

    testOk1(epicsStrGlobMatch("hello","he?lo"));
    testOk1(!epicsStrGlobMatch("hello","he?xo"));
    testOk1(epicsStrGlobMatch("hello","he??o"));
    testOk1(!epicsStrGlobMatch("helllo","he?lo"));

    testOk1(epicsStrGlobMatch("hello world","he*o w*d"));
    testOk1(!epicsStrGlobMatch("hello_world","he*o w*d"));
    testOk1(epicsStrGlobMatch("hello world","he**d"));

    testOk1(epicsStrGlobMatch("hello hello world","he*o w*d"));

    testOk1(!epicsStrGlobMatch("hello hello xorld","he*o w*d"));

    testOk1(epicsStrGlobMatch("hello","he*"));
    testOk1(epicsStrGlobMatch("hello","*lo"));
    testOk1(epicsStrGlobMatch("hello","*"));
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
    char result[8];
    char *s;
    int status;

    testPlan(406);

    testChars();

    testOk1(epicsStrnCaseCmp(empty, "", 0) == 0);
    testOk1(epicsStrnCaseCmp(empty, "", 1) == 0);
    testOk1(epicsStrnCaseCmp(space, empty, 1) > 0);
    testOk1(epicsStrnCaseCmp(empty, space, 1) < 0);
    testOk1(epicsStrnCaseCmp(a, A, 1) == 0);
    testOk1(epicsStrnCaseCmp(a, A, 2) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCD, 2) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCD, 4) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCD, 1000) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCDE, 2) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCDE, 4) == 0);
    testOk1(epicsStrnCaseCmp(abcd, ABCDE, 1000) < 0);
    testOk1(epicsStrnCaseCmp(abcde, ABCD, 2) == 0);
    testOk1(epicsStrnCaseCmp(abcde, ABCD, 4) == 0);
    testOk1(epicsStrnCaseCmp(abcde, ABCD, 1000) > 0);

    testOk1(epicsStrCaseCmp(empty, "") == 0);
    testOk1(epicsStrCaseCmp(a, A) == 0);
    testOk1(epicsStrCaseCmp(abcd, ABCD) == 0);
    testOk1(epicsStrCaseCmp(abcd, ABCDE) < 0);
    testOk1(epicsStrCaseCmp(abcde, ABCD) > 0);
    testOk1(epicsStrCaseCmp(abcde, "ABCDF") < 0);

    s = epicsStrDup(abcd);
    testOk(strcmp(s, abcd) == 0 && s != abcd, "epicsStrDup");
    free(s);

    testOk1(epicsStrHash(abcd, 0) != epicsStrHash("bacd", 0));
    testOk1(epicsStrHash(abcd, 0) == epicsMemHash(abcde, 4, 0));
    testOk1(epicsStrHash(abcd, 0) != epicsMemHash("abcd\0", 5, 0));

    testOk1(epicsStrnLen("abcd", 5)==4);
    testOk1(epicsStrnLen("abcd", 4)==4);
    testOk1(epicsStrnLen("abcd", 3)==3);
    testOk1(epicsStrnLen("abcd", 0)==0);

    testGlob();

    memset(result, 'x', sizeof(result));
    status = epicsStrnEscapedFromRaw(result, 0, ABCD, 4);
    testOk(status == 4,      "epicsStrnEscapedFromRaw(out, 0) -> %d (exp. 4)", status);
    testOk(result[0] == 'x', "  No output");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 0, ABCD, 4);
    testOk(status == 0,      "epicsStrnRawFromEscaped(out, 0) -> %d (exp. 0)", status);
    testOk(result[0] == 'x', "  No output");

    memset(result, 'x', sizeof(result));
    status = epicsStrnEscapedFromRaw(result, 1, ABCD, 4);
    testOk(status == 4,      "epicsStrnEscapedFromRaw(out, 1) -> %d (exp. 4)", status);
    testOk(result[0] == 0,   "  0-terminated");
    testOk(result[1] == 'x', "  No overrun");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 1, ABCD, 4);
    testOk(status == 0,      "epicsStrnRawFromEscaped(out, 1) -> %d (exp. 0)", status);
    testOk(result[0] == 0,   "  0-terminated");
    testOk(result[1] == 'x', "  No overrun");

    testDiag("Testing size = epicsStrnEscapedFromRawSize");

    status = epicsStrnEscapedFromRawSize(ABCD, 3);
    testOk(status == 3, "size(\"ABCD\", 3) -> %d (exp. 3)", status);
    status = epicsStrnEscapedFromRawSize(ABCD, 4);
    testOk(status == 4, "size(\"ABCD\", 4) -> %d (exp. 4)", status);
    status = epicsStrnEscapedFromRawSize(ABCD, 5);
    testOk(status == 8, "size(\"ABCD\", 5) -> %d (exp. 8)", status);

    testDiag("Testing esc = epicsStrnEscapedFromRaw(out, 4, ...)");

    memset(result, 'x', sizeof(result));
    status = epicsStrnEscapedFromRaw(result, 4, ABCD, 3);
    testOk(status == 3,      "esc(\"ABCD\", 3) -> %d (exp. 3)", status);
    testOk(result[3] == 0,   "  0-terminated");
    testOk(result[4] == 'x', "  No overrun");

    memset(result, 'x', sizeof(result));
    status = epicsStrnEscapedFromRaw(result, 4, ABCD, 4);
    testOk(status == 4,      "esc(\"ABCD\", 4) -> %d (exp. 4)", status);
    testOk(result[3] == 0,   "  0-terminated");
    testOk(result[4] == 'x', "  No overrun");

    memset(result, 'x', sizeof(result));
    status = epicsStrnEscapedFromRaw(result, 4, ABCD, 5);
    testOk(status == 8,      "esc(\"ABCD\", 5) -> %d (exp. 8)", status);
    testOk(result[3] == 0,   "  0-terminated");
    testOk(result[4] == 'x', "  No overrun");

    testDiag("Testing raw = epicsStrnRawFromEscaped(out, 4, ...)");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, ABCD, 0);
    testOk(status == 0,      "raw(\"ABCD\", 0) -> %d (exp. 0)", status);
    testOk(result[0] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, ABCD, 1);
    testOk(status == 1,      "raw(\"ABCD\", 1) -> %d (exp. 1)", status);
    testOk(result[0] == 'A', "  Char '%c' (exp. 'A')", result[0]);
    testOk(result[1] == 0,   "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, ABCD, 2);
    testOk(status == 2,      "raw(\"ABCD\", 2) -> %d (exp. 2)", status);
    testOk(result[0] == 'A', "  Char '%c' (exp. 'A')", result[0]);
    testOk(result[1] == 'B', "  Char '%c' (exp. 'B')", result[1]);
    testOk(result[2] == 0,   "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, ABCD, 3);
    testOk(status == 3,      "raw(\"ABCD\", 3) -> %d (exp. 3)", status);
    testOk(result[0] == 'A', "  Char '%c' (exp. 'A')", result[0]);
    testOk(result[1] == 'B', "  Char '%c' (exp. 'B')", result[1]);
    testOk(result[2] == 'C', "  Char '%c' (exp. 'C')", result[2]);
    testOk(result[3] == 0,   "  0-terminated");
    testOk(result[4] == 'x', "  No write outside buffer");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, ABCD, 4);
    testOk(status == 3,      "raw(\"ABCD\", 4) -> %d (exp. 3)", status);
    testOk(result[0] == 'A', "  Char '%c' (exp. 'A')", result[0]);
    testOk(result[1] == 'B', "  Char '%c' (exp. 'B')", result[1]);
    testOk(result[2] == 'C', "  Char '%c' (exp. 'C')", result[2]);
    testOk(result[3] == 0,   "  0-terminated");
    testOk(result[4] == 'x', "  No write outside buffer");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, ABCDE, 5);
    testOk(status == 3,      "raw(\"ABCDE\", 5) -> %d (exp. 3)", status);
    testOk(result[0] == 'A', "  Char '%c' (exp. 'A')", result[0]);
    testOk(result[1] == 'B', "  Char '%c' (exp. 'B')", result[1]);
    testOk(result[2] == 'C', "  Char '%c' (exp. 'C')", result[2]);
    testOk(result[3] == 0,   "  0-terminated");
    testOk(result[4] == 'x', "  No write outside buffer");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "A", 2);
    testOk(status == 1,      "raw(\"A\", 2) -> %d (exp. 1)", status);
    testOk(result[0] == 'A', "  Char '%c' (exp. 'A')", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\123", 1);
    testOk(status == 0,      "raw(\"\\123\", 1) -> %d (exp. 0)", status);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\123", 2);
    testOk(status == 1,      "raw(\"\\123\", 2) -> %d (exp. 1)", status);
    testOk(result[0] == 1,   "  Octal escape (got \\%03o)", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\123", 3);
    testOk(status == 1,      "raw(\"\\123\", 3) -> %d (exp. 1)", status);
    testOk(result[0] == 012, "  Octal escape (got \\%03o)", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\123", 4);
    testOk(status == 1,      "raw(\"\\123\", 4) -> %d (exp. 1)", status);
    testOk(result[0] == 0123, "  Octal escape (got \\%03o)", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\812", 2);
    testOk(status == 1,      "raw(\"\\812\", 2) -> %d (exp. 1)", status);
    testOk(result[0] == '8', "  Escaped '%c')", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\182", 3);
    testOk(status == 2,      "raw(\"\\182\", 3) -> %d (exp. 2)", status);
    testOk(result[0] == 1,   "  Octal escape (got \\%03o)", result[0]);
    testOk(result[1] == '8', "  Terminated with '%c'", result[1]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\128", 4);
    testOk(status == 2,      "raw(\"\\128\", 4) -> %d (exp. 2)", status);
    testOk(result[0] == 012, "  Octal escape (got \\%03o)", result[0]);
    testOk(result[1] == '8', "  Terminator char got '%c'", result[1]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x12", 1);
    testOk(status == 0,      "raw(\"\\x12\", 1) -> %d (exp. 0)", status);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x12", 2);
    testOk(status == 0,      "raw(\"\\x12\", 2) -> %d (exp. 0)", status);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x12", 3);
    testOk(status == 1,      "raw(\"\\x12\", 3) -> %d (exp. 1)", status);
    testOk(result[0] == 1,   "  Hex escape (got \\x%x)", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x12", 4);
    testOk(status == 1,      "raw(\"\\x12\", 4) -> %d (exp. 1)", status);
    testOk(result[0] == 0x12,"  Hex escape (got \\x%x)", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\xaF", 4);
    testOk(status == 1,      "raw(\"\\xaF\", 4) -> %d (exp. 1)", status);
    testOk(result[0] == '\xaF',"  Hex escape (got \\x%x)", result[0] & 0xFF);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x012", 5);
    testOk(status == 1,      "raw(\"\\x012\", 5) -> %d (exp. 1)", status);
    testOk(result[0] == 0x12,"  Hex escape (got \\x%x)", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x0012", 6);
    testOk(status == 1,      "raw(\"\\x0012\", 6) -> %d (exp. 1)", status);
    testOk(result[0] == 0x12,"  Hex escape (got \\x%x)", result[0]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x1g", 4);
    testOk(status == 2,      "raw(\"\\x1g\", 4) -> %d (exp. 2)", status);
    testOk(result[0] == 1, "  Hex escape (got \\x%x)", result[0]);
    testOk(result[1] == 'g', "  Terminator char got '%c'", result[1]);
    testOk(result[status] == 0, "  0-terminated");

    return testDone();
}
