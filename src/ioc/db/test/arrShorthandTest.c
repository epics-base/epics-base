/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

 /*
  * Test the shorthand array notation  [ start : incr : end ]
  * by registering a thin fake arr plugin
  * and checking if values are forwarded correctly
  */

#include <string.h>

#include "chfPlugin.h"
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "registry.h"
#include "errlog.h"
#include "epicsExit.h"
#include "dbUnitTest.h"
#include "testMain.h"
#include "osiFileName.h"

typedef struct myStruct {
    epicsInt32 start;
    epicsInt32 incr;
    epicsInt32 end;
} myStruct;

static const
chfPluginArgDef opts[] = {
    chfInt32 (myStruct, start, "s", 0, 1),
    chfInt32 (myStruct, incr, "i", 0, 1),
    chfInt32 (myStruct, end, "e", 0, 1),
    chfPluginArgEnd
};

static myStruct my;

static void * allocPvt(void)
{
    my.start = 0;
    my.incr = 1;
    my.end = -1;
    return &my;
}

static chfPluginIf myPif = {
    allocPvt,
    NULL, /* freePvt, */

    NULL, /* parse_error, */
    NULL, /* parse_ok, */

    NULL, /* channel_open, */
    NULL, /* channelRegisterPre, */
    NULL, /* channelRegisterPost, */
    NULL, /* channel_report, */
    NULL  /* channel_close */
};

static int checkValues(epicsUInt32 s, epicsUInt32 i, epicsUInt32 e) {
    if (s == my.start && i == my.incr && e == my.end)
        return 1;
    else
        return 0;
}

static void testHead (char* title) {
    testDiag("--------------------------------------------------------");
    testDiag("%s", title);
    testDiag("--------------------------------------------------------");
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(arrShorthandTest)
{
    dbChannel *pch;

    testPlan(26);

    testdbPrepare();

    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);

    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("xRecord.db", NULL, NULL);

    testHead("Register plugin");
    testOk(!chfPluginRegister("arr", &myPif, opts), "register fake arr plugin");

    eltc(0);
    testIocInitOk();
    eltc(1);

#define TESTBAD(Title, Expr) \
    testDiag(Title); \
    testOk(!(pch = dbChannelCreate("x." Expr)), "dbChannelCreate (" Expr ") fails"); \
    if (pch) dbChannelDelete(pch);

#define TESTGOOD(Title, Expr, Start, Incr, End) \
    testDiag(Title); \
    testOk(!!(pch = dbChannelCreate("x." Expr)), "dbChannelCreate (" Expr ")"); \
    testOk(checkValues(Start, Incr, End), "parameters set correctly: s=%d i=%d e=%d", Start, Incr, End); \
    if (pch) dbChannelDelete(pch);

    TESTBAD("no parameters []", "[]");
    TESTBAD("invalid char at beginning [x", "[x");
    TESTBAD("invalid char after 1st arg [2x", "[2x");
    TESTBAD("invalid char after 2nd arg [2:3x", "[2:3x");
    TESTBAD("invalid char after 3rd arg [2:3:4x", "[2:3:4x");

    TESTGOOD("one element [index]",         "[2]",     2, 1, 2);
    TESTGOOD("to end [s:]",                 "[2:]",    2, 1, -1);
    TESTGOOD("to end [s::]",                "[2::]",   2, 1, -1);
    TESTGOOD("to end with incr [s:i:]",     "[2:3:]",  2, 3, -1);
    TESTGOOD("from beginning [:e]",         "[:2]",    0, 1, 2);
    TESTGOOD("from beginning [::e]",        "[::2]",   0, 1, 2);
    TESTGOOD("from begin with incr [:i:e]", "[:3:2]",  0, 3, 2);
    TESTGOOD("range [s:e]",                 "[2:4]",   2, 1, 4);
    TESTGOOD("range [s::e]",                "[2::4]",  2, 1, 4);
    TESTGOOD("range with incr [s:i:e]",     "[2:3:4]", 2, 3, 4);

    testIocShutdownOk();
    testdbCleanup();

    return testDone();
}
