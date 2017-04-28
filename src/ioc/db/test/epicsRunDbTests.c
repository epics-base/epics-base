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

int testdbConvert(void);
int callbackTest(void);
int callbackParallelTest(void);
int dbStateTest(void);
int dbCaStatsTest(void);
int dbShutdownTest(void);
int dbScanTest(void);
int scanIoTest(void);
int dbLockTest(void);
int dbPutLinkTest(void);
int dbStaticTest(void);
int dbCaLinkTest(void);
int testDbChannel(void);
int chfPluginTest(void);
int arrShorthandTest(void);
int recGblCheckDeadbandTest(void);

void epicsRunDbTests(void)
{
    testHarness();

    runTest(testdbConvert);
    runTest(callbackTest);
    runTest(callbackParallelTest);
    runTest(dbStateTest);
    runTest(dbCaStatsTest);
    runTest(dbShutdownTest);
    runTest(dbScanTest);
    runTest(scanIoTest);
    runTest(dbLockTest);
    runTest(dbPutLinkTest);
    runTest(dbStaticTest);
    runTest(dbCaLinkTest);
    runTest(testDbChannel);
    runTest(arrShorthandTest);
    runTest(recGblCheckDeadbandTest);
    runTest(chfPluginTest);

    dbmfFreeChunks();

    epicsExit(0);   /* Trigger test harness */
}
