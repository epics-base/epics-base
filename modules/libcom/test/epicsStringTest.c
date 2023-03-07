/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
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
#include "epicsMath.h"
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

/* epicsStrnGlobMatch */

    testOk1(epicsStrnGlobMatch("xyzq",3,"xyz"));
    testOk1(!epicsStrnGlobMatch("xyzq",3,"xyzm"));
    testOk1(!epicsStrnGlobMatch("xyzm",3,"xyzm"));
    testOk1(!epicsStrnGlobMatch("xyzm",0,"xyzm"));
    testOk1(!epicsStrnGlobMatch("xyzq",3,""));
    testOk1(epicsStrnGlobMatch("xyz",0,""));

    testOk1(epicsStrnGlobMatch("xyz",0,"*"));
    testOk1(!epicsStrnGlobMatch("xyz",0,"?"));
    testOk1(!epicsStrnGlobMatch("xyz",0,"?*"));

    testOk1(epicsStrnGlobMatch("hello!",5,"h*o"));
    testOk1(!epicsStrnGlobMatch("hello!",5,"h*x"));
    testOk1(!epicsStrnGlobMatch("hellxo",5,"h*o"));

    testOk1(epicsStrnGlobMatch("hello!",5,"he?lo"));
    testOk1(!epicsStrnGlobMatch("hello!",5,"he?xo"));
    testOk1(epicsStrnGlobMatch("hello!",5,"he??o"));
    testOk1(!epicsStrnGlobMatch("helllo!",5,"he?lo"));

    testOk1(!epicsStrnGlobMatch("hello world!",10,"he*o w*d"));
    testOk1(epicsStrnGlobMatch("hello world!",11,"he*o w*d"));
    testOk1(!epicsStrnGlobMatch("hello world!",12,"he*o w*d"));
    testOk1(!epicsStrnGlobMatch("hello_world!",11,"he*o w*d"));
    testOk1(epicsStrnGlobMatch("hello world!",11,"he**d"));

    testOk1(epicsStrnGlobMatch("hello hello world!!!!!!!!!!!!!!!!!!!!",17,"he*o w*d"));

    testOk1(!epicsStrnGlobMatch("hello hello world",15,"he*o w*d"));

    testOk1(epicsStrnGlobMatch("hello!!",5,"he*"));
    testOk1(epicsStrnGlobMatch("hello!!",5,"*lo"));
    testOk1(epicsStrnGlobMatch("hello!!",5,"*"));

}

static
void testDistance(void) {
    double dist;
    testDiag("testDistance()");

#define TEST(EXPECT, A, B) dist = epicsStrSimilarity(A, B); testOk(fabs(dist-(EXPECT))<0.01, "distance \"%s\", \"%s\" %f ~= %f", A, B, dist, EXPECT)

    TEST(1.00, "", "");
    TEST(1.00, "A", "A");
    TEST(0.00, "A", "B");
    TEST(1.00, "exact", "exact");
    TEST(0.90, "10 second", "10 seconds");
    TEST(0.71, "Passive", "Pensive");
    TEST(0.11, "10 second", "Pensive");
    TEST(0.97, "Set output to IVOV", "Set output To IVOV");

    /* we modify Levenshtein to give half weight to case insensitive matches */

    /* totally unrelated except for 'i' ~= 'I' */
    TEST(0.06, "Passive", "I/O Intr");
    TEST(0.06, "I/O Intr", "Pensive");
    /* 2x subst and 1x case subst, max distance 2xlen("YES") */
    TEST(0.50, "YES", "yes");
    TEST(0.00, "YES", "NO");
    TEST(0.67, "YES", "Yes");
    TEST(0.67, "Tes", "yes");
#undef TEST
}

static
void testTok(const char *inp,
             const char *delim,
             const char *save,
             const char *expectTok,
             const char *expectSave)
{
    char *scratch = !inp ? NULL : strdup(inp);
    char *allocSave = !save ? NULL : strdup(save);
    char *actualSave = allocSave;
    char *actualTok;

    if((!inp || scratch) && (!save || allocSave)) {

        actualTok = epicsStrtok_r(scratch, delim, &actualSave);

#define COMPSTR(A, B) ((!(A) && !(B)) || ((A) && (B) && strcmp(A,B)==0))

#define ORNIL(S) (S ? S : "(nil)")

        testOk(COMPSTR(expectSave, actualSave) && COMPSTR(expectTok, actualTok),
               "inp: \"%s\" delim: \"%s\" tok: \"%s\"==\"%s\" save: \"%s\"==\"%s\"",
               ORNIL(inp), ORNIL(delim), ORNIL(expectTok), ORNIL(actualTok), ORNIL(expectSave), ORNIL(actualSave));
#undef COMPSTR
#undef ORNIL

    } else {
        testFail("nomem");
    }
    free(scratch);
    free(allocSave);
}

static
void testStrTok(void)
{
    testDiag("testStrTok()");

    testTok("", " \t", NULL, NULL, NULL);
    testTok("  \t", " \t", NULL, NULL, NULL);
    testTok(NULL, " \t", "", NULL, NULL);
    testTok(NULL, " \t", " ", NULL, NULL);
    testTok(NULL, " \t", "  \t", NULL, NULL);

    testTok("a", " \t", NULL, "a", NULL);
    testTok("  a", " \t", NULL, "a", NULL);
    testTok("a  ", " \t", NULL, "a", " ");
    testTok("\ta  ", " \t", NULL, "a", " ");

    testTok("aaa bbb ", " \t", NULL, "aaa", "bbb ");
    testTok("aaa\tbbb ", " \t", NULL, "aaa", "bbb ");
    testTok(NULL, " \t", "bbb ", "bbb", "");
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

    testPlan(439);

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
    testOk(status == 6, "size(\"ABCD\", 5) -> %d (exp. 8)", status);

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
    testOk(status == 6,      "esc(\"ABCD\", 5) -> %d (exp. 8)", status);
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
    testOk(status == 2,      "raw(\"\\x012\", 5) -> %d (exp. 2)", status);
    testOk(result[0] == 0x1,"  Hex escape (got \\x%x)", result[0]);
    testOk(result[1] == '2', "  Terminator char got '%c'", result[1]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x0012", 6);
    testOk(status == 3,      "raw(\"\\x0012\", 6) -> %d (exp. 3)", status);
    testOk(result[0] == 0,"  Hex escape (got \\x%x)", result[0]);
    testOk(result[1] == '1', "  Terminator char got '%c'", result[1]);
    testOk(result[2] == '2', "  Following char got '%c'", result[2]);
    testOk(result[status] == 0, "  0-terminated");

    memset(result, 'x', sizeof(result));
    status = epicsStrnRawFromEscaped(result, 4, "\\x1g", 4);
    testOk(status == 2,      "raw(\"\\x1g\", 4) -> %d (exp. 2)", status);
    testOk(result[0] == 1, "  Hex escape (got \\x%x)", result[0]);
    testOk(result[1] == 'g', "  Terminator char got '%c'", result[1]);
    testOk(result[status] == 0, "  0-terminated");

    testDistance();
    testStrTok();

    return testDone();
}
