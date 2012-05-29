/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Run Db tests as a batch.
 *
 * Do not include performance measurements here, they don't prove
 * functionality (which is the purpose of this convenience routine).
 */

#include "epicsUnitTest.h"
#include "epicsExit.h"

int callbackTest(void);

void epicsRunDbTests(void)
{
    testHarness();

    runTest(callbackTest);

    epicsExit(0);   /* Trigger test harness */
}
