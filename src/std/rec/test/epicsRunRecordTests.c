/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Run tests as a batch.
 */

#include "epicsUnitTest.h"
#include "epicsExit.h"

int analogMonitorTest(void);
int arrayOpTest(void);
int scanEventTest(void);

void epicsRunRecordTests(void)
{
    testHarness();

    runTest(analogMonitorTest);

    runTest(arrayOpTest);

    runTest(scanEventTest);

    epicsExit(0);   /* Trigger test harness */
}
