/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Run libCom tests as a batch
 *
 * This is part of the work being done to provide a unified set of automated
 * tests for EPICS.  Many more changes will be forthcoming.
 */
#include <stdio.h>
#include <epicsThread.h>
#include <epicsUnitTest.h>

int epicsThreadTest(void);
int epicsTimerTest(void);
int epicsAlgorithm(void);
int epicsEllTest(void);
int epicsCalcTest(void);
int epicsEventTest(void);
int epicsExceptionTest(void);
int epicsMathTest(void);
int epicsMessageQueueTest(void);
int epicsMutexTest(void);
int epicsStdioTest(void);
int epicsStringTest(void);
int epicsThreadPriorityTest(void);
int epicsThreadPrivateTest(void);
int epicsTimeTest(void);
int macLibTest(void);
int macEnvExpandTest(void);
int ringPointerTest(void);
int ringBytesTest(void);
int blockingSockTest(void);
int taskwdTest(void);
int cvtFastPerform(void);
int epicsExitTest(void);

void epicsRunLibComTests(void)
{
    testHarness();

    /*
     * Thread startup sets some internal variables so do it first
     */
    runTest(epicsThreadTest);

    /*
     * Timer tests get confused if run after some of the other tests
     */
    runTest(epicsTimerTest);

    runTest(epicsAlgorithm);

    runTest(epicsEllTest);

    runTest(epicsCalcTest);

    runTest(epicsEventTest);

    runTest(epicsExceptionTest);

    runTest(epicsMathTest);

    runTest(epicsMessageQueueTest);

    runTest(epicsMutexTest);

    runTest(epicsStdioTest);

    runTest(epicsStringTest);

    runTest(epicsThreadPriorityTest);

    runTest(epicsThreadPrivateTest);

    runTest(epicsTimeTest);

    runTest(macLibTest);

    runTest(macEnvExpandTest);

    runTest(ringPointerTest);

    runTest(ringBytesTest);

    runTest(blockingSockTest);

    runTest(taskwdTest);

    runTest(cvtFastPerform);

    /*
     * Exit must come last as it never returns
     */
    runTest(epicsExitTest);
}
