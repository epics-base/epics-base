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

#include <string.h>

#include "dbState.h"
#include "epicsUnitTest.h"
#include "testMain.h"

MAIN(dbStateTest)
{
    dbStateId red, red2, blue, blue2;
    int i;

    testPlan(20);

    testOk(!dbStateFind("y"), "Finding nonexisting state fails");

    testOk(!!(red = dbStateCreate("red")), "Create state 'red'");
    testOk((red2 = dbStateFind("red")) == red, "Find 'red' returns correct id");
    testOk((red2 = dbStateCreate("red")) == red, "Create existing 'red' returns correct id");
    testOk(!dbStateFind("y"), "Finding nonexisting state still fails");

    testOk(!!(blue = dbStateCreate("blue")), "Create state 'blue'");
    testOk((blue2 = dbStateFind("blue")) == blue, "Find 'blue' returns correct id");
    testOk((blue2 = dbStateCreate("blue")) == blue, "Create existing 'blue' returns correct id");
    testOk(!dbStateFind("y"), "Finding nonexisting state still fails");

    testOk((i = dbStateGet(red)) == 0, "Default 'red' state is 0");
    testOk((i = dbStateGet(blue)) == 0, "Default 'blue' state is 0");
    dbStateSet(red);
    testOk((i = dbStateGet(red)) == 1, "After setting, 'red' state is 1");
    testOk((i = dbStateGet(blue)) == 0, "'blue' state is 0");
    dbStateSet(blue);
    testOk((i = dbStateGet(blue)) == 1, "After setting, 'blue' state is 1");
    testOk((i = dbStateGet(red)) == 1, "'red' state is 1");
    dbStateClear(blue);
    testOk((i = dbStateGet(blue)) == 0, "After clearing, 'blue' state is 0");
    testOk((i = dbStateGet(red)) == 1, "'red' state is 1");
    dbStateClear(red);
    testOk((i = dbStateGet(red)) == 0, "After clearing, 'red' state is 0");
    testOk((i = dbStateGet(blue)) == 0, "'red' state is 0");

    testOk(!dbStateFind("y"), "Finding nonexisting state still fails");

    return testDone();
}
