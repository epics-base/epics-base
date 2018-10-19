/*************************************************************************\
* Copyright (c) 2018 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Run filter tests as a batch.
 */

#include "epicsUnitTest.h"
#include "epicsExit.h"
#include "dbmf.h"

int lnkStateTest(void);
int lnkCalcTest(void);

void epicsRunLinkTests(void)
{
    testHarness();

    runTest(lnkStateTest);
    runTest(lnkCalcTest);

    dbmfFreeChunks();

    epicsExit(0);   /* Trigger test harness */
}
