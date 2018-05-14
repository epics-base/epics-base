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
#include "epicsThread.h"
#include "dbScan.h"

void scanEventTest_registerRecordDeviceDriver(struct dbBase *);

/* test name to event number:
    num = 0 is no event,
    num > 0 is numeric event
    num < 0 is string event (use same unique number for aliases)
*/
const struct {char* name; int num;} events[] = {
/* No events */
    {NULL,            0},
    {"",              0},
    {"       ",       0},
    {"0",             0},
    {"0.000000",      0},
    {"-0.00000",      0},
    {"0.9",           0},
/* Numeric events */
    {"2",             2},
    {"2.000000",      2},
    {"2.5",           2},
    {" 2.5  ",        2},
    {"+0x02",         2},
    {"3",             3},
/* Named events */
    {"info 1",       -1},
    {"   info 1  ",  -1},
    {"-0.9",         -2},
    {"-2",           -3},
    {"-2.000000",    -4},
    {"-2.5",         -5},
    {" -2.5  ",      -5},
    {"nan",          -6},
    {"NaN",          -7},
    {"-NAN",         -8},
    {"-inf",         -9},
    {"inf",         -10},
};

MAIN(scanEventTest)
{
    int i, e;
    int aliases[512] ;
    int expected_count[512];
    #define INDX(i) 256-events[i].num
    #define MAXEV 5

    testPlan(NELEMENTS(events)*2+(MAXEV+1)*5);

    testdbPrepare();

    memset(aliases, 0, sizeof(aliases));
    memset(expected_count, 0, sizeof(expected_count));

    testdbReadDatabase("scanEventTest.dbd", NULL, NULL);

    scanEventTest_registerRecordDeviceDriver(pdbbase);
    for (i = 0; i < NELEMENTS(events); i++) {
        char substitutions[256];
        sprintf(substitutions, "N=%d,EVENT=%s", i, events[i].name);
        testdbReadDatabase("scanEventTest.db", NULL, substitutions);
    }
    testIocInitOk();
    testDiag("Test if eventNameToHandle() strips spaces and handles numeric events");
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
        post_event(events[i].num); /* triggers numeric events only */
        postEvent(pev);
    }

    testDiag("Check calculated numeric events (backward compatibility)");
    for (e = 0; e <= MAXEV; e++) {
        testdbPutFieldOk("eventnum", DBR_LONG, e);
        testdbGetFieldEqual("e1", DBR_LONG, e);
        testdbGetFieldEqual("e2", DBR_LONG, e);
        testdbPutFieldOk("e3", DBR_LONG, e);
        testdbPutFieldOk("e3.PROC", DBR_LONG, 1);
        for (i = 0; i < NELEMENTS(events); i++)
            if (e > 0 && e < 256 && events[i].num == e) { /* numeric events */
                expected_count[INDX(i)]+=3; /* +1 for eventnum->e1, +1 for e2<-eventnum, +1 for e3 */
                break;
            }
    }
    /* Allow records to finish processing */
    testSyncCallback();
    testDiag("Check if events have been processed the expected number of times");
    for (i = 0; i < NELEMENTS(events); i++) {
        char pvname[100];
        sprintf(pvname, "c%d", i);
        testDiag("Event \"%s\" expected %d times", events[i].name, expected_count[INDX(i)]);
        testdbGetFieldEqual(pvname, DBR_LONG, expected_count[INDX(i)]);
    }

    testIocShutdownOk();

    testdbCleanup();

    return testDone();
}
