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
#include "dbmf.h"

int callbackTest(void);
int dbStateTest(void);
int testDbChannel(void);
int chfPluginTest(void);
int arrShorthandTest(void);

void epicsRunDbTests(void)
{
    testHarness();

    runTest(callbackTest);
    runTest(dbStateTest);
    runTest(testDbChannel);
    runTest(chfPluginTest);
    runTest(arrShorthandTest);

    dbmfFreeChunks();

    epicsExit(0);   /* Trigger test harness */
}
