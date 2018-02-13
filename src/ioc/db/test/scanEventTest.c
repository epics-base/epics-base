/*************************************************************************\
* Copyright (c) 2018 Paul Scherrer Institut
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Dirk Zimoch <dirk.zimoch@psi.ch>
 */

#include <string.h>
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "dbUnitTest.h"
#include "testMain.h"
#include "osiFileName.h"
#include "dbScan.h"

void scanEventTest_registerRecordDeviceDriver(struct dbBase *);

/* test name to event number:
    num = 0 is no event,
    num > 0 is numeric event
    num < 0 is string event (use same unique number for aliases)
*/
const struct {char* name; int num;} events[] = {
    {"",              0},
    {"       ",       0},
    {"0",             0},
    {"0.000000",      0},
    {"-0.00000",      0},
    {"0.9",           0},
    {"nan",           0},
    {"NaN",           0},
    {"-NAN",          0},
    {"-inf",          0},
    {"inf",           0},
    {"info 1",       -1},
    {"   info 1  ",  -1},
    {"-0.9",         -2},
    {"2",             2},
    {"2.000000",      2},
    {"2.5",           2},
    {" 2.5  ",        2},
    {"+0x02",         2},
    {"-2",           -3},
    {"-2.000000",    -4},
    {"-2.5",         -5},
    {" -2.5  ",      -5},
};

MAIN(scanEventTest)
{
    unsigned int i;
    int aliases[512] ;
    int expected_count[512];
    #define INDX(i) 256-events[i].num
    
    testPlan(NELEMENTS(events)*2);
    
    memset(aliases, 0, sizeof(aliases));
    memset(expected_count, 0, sizeof(expected_count));

    if (dbReadDatabase(&pdbbase, "scanEventTest.dbd",
            "." OSI_PATH_LIST_SEPARATOR ".." OSI_PATH_LIST_SEPARATOR
            "../O.Common" OSI_PATH_LIST_SEPARATOR "O.Common", NULL))
        testAbort("Database description 'scanEventTest.dbd' not found");

    scanEventTest_registerRecordDeviceDriver(pdbbase);
    for (i = 0; i < NELEMENTS(events); i++) {
        char substitutions[256];
        sprintf(substitutions, "N=%d,EVENT=%s", i, events[i].name);
        if (dbReadDatabase(&pdbbase, "scanEventTest.db",
                "." OSI_PATH_LIST_SEPARATOR "..", substitutions))
            testAbort("Error reading test database 'scanEventTest.db'");
    }
    testIocInitOk();
    for (i = 0; i < NELEMENTS(events); i++) {
        EVENTPVT pev = eventNameToHandle(events[i].name);
        /* test that some names are not events (num=0) */
        if (events[i].num == 0)
            testOk(pev == NULL, "\"%s\" -> no event", events[i].name);
        else
        {
            expected_count[INDX(i)]++; /* +1 for postEvent */
            if (events[i].num > 0)
            {
                testOk(pev != NULL, "\"%s\" -> numeric event %d", events[i].name, events[i].num);
                expected_count[INDX(i)]++; /* +1 for post_event */
            }
            else
            {
                 /* test that some strings resolve the same event (num!=0) */
                 if (!aliases[INDX(i)])
                 {
                     aliases[INDX(i)] = i;
                     testOk(pev != NULL, "\"%s\" -> new named event", events[i].name);
                 }
                 else
                 {
                     testOk(pev == eventNameToHandle(events[aliases[INDX(i)]].name),
                         "\"%s\" alias for \"%s\"", events[i].name, events[aliases[INDX(i)]].name);
                 }
            }
        }
        post_event(events[i].num);
        postEvent(pev);
    }
    for (i = 0; i < NELEMENTS(events); i++) {
        char pvname[100];
        sprintf(pvname, "c%d", i);
        testDiag("Check if %s (event \"%s\") has processed %d times", pvname, events[i].name, expected_count[INDX(i)]);
        testdbGetFieldEqual(pvname, DBR_LONG, expected_count[INDX(i)]);
    }

    return testDone();
}
