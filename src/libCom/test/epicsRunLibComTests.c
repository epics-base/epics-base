/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Run libCom tests as a batch.
 *
 * Do *not* include performance measurements here, they don't help to
 * prove functionality (which is the point of this convenience routine).
 */

#include <stdio.h>
#include <epicsThread.h>
#include <epicsUnitTest.h>

int epicsThreadTest(void);
int epicsTimerTest(void);
int epicsAlgorithm(void);
int epicsEllTest(void);
int epicsEnvTest(void);
int epicsErrlogTest(void);
int epicsCalcTest(void);
int epicsEventTest(void);
int epicsMathTest(void);
int epicsMessageQueueTest(void);
int epicsMutexTest(void);
int epicsStdioTest(void);
int epicsStringTest(void);
int epicsThreadOnceTest(void);
int epicsThreadPriorityTest(void);
int epicsThreadPrivateTest(void);
int epicsTimeTest(void);
#ifdef __rtems__
int epicsTimeZoneTest(void);
#endif
int macLibTest(void);
int macEnvExpandTest(void);
int ringPointerTest(void);
int ringBytesTest(void);
int blockingSockTest(void);
int epicsSockResolveTest(void);
int taskwdTest(void);
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

    runTest(epicsEnvTest);

    runTest(epicsErrlogTest);

    runTest(epicsCalcTest);

    runTest(epicsEventTest);

    runTest(epicsMathTest);

    runTest(epicsMessageQueueTest);

    runTest(epicsMutexTest);

    runTest(epicsStdioTest);

    runTest(epicsStringTest);

    runTest(epicsThreadOnceTest);

    runTest(epicsThreadPriorityTest);

    runTest(epicsThreadPrivateTest);

    runTest(epicsTimeTest);

#ifdef __rtems__
    runTest(epicsTimeZoneTest);
#endif

    runTest(macLibTest);

    runTest(macEnvExpandTest);

    runTest(ringPointerTest);

    runTest(ringBytesTest);

    runTest(blockingSockTest);
    
    runTest(epicsSockResolveTest);

    runTest(taskwdTest);

    /*
     * Report now in case epicsExitTest dies
     */
    testHarnessDone();

    /*
     * epicsExitTest must come last as it never returns
     */
    runTest(epicsExitTest);
}
