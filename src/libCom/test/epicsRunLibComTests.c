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

int blockingSockTest(void);
int epicsAlgorithm(void);
int epicsAtomicTest(void);
int epicsCalcTest(void);
int epicsEllTest(void);
int epicsEnvTest(void);
int epicsErrlogTest(void);
int epicsEventTest(void);
int epicsExitTest(void);
int epicsMathTest(void);
int epicsMessageQueueTest(void);
int epicsMMIOTest(void);
int epicsMutexTest(void);
int epicsSockResolveTest(void);
int epicsSpinTest(void);
int epicsStackTraceTest(void);
int epicsStdioTest(void);
int epicsStdlibTest(void);
int epicsStringTest(void);
int epicsThreadHooksTest(void);
int epicsThreadOnceTest(void);
int epicsThreadPoolTest(void);
int epicsThreadPriorityTest(void);
int epicsThreadPrivateTest(void);
int epicsThreadTest(void);
int epicsTimerTest(void);
int epicsTimeTest(void);
#ifdef __rtems__
int epicsTimeZoneTest(void);
#endif
int epicsTypesTest(void);
int epicsInlineTest(void);
int ipAddrToAsciiTest(void);
int macDefExpandTest(void);
int macLibTest(void);
int ringBytesTest(void);
int ringPointerTest(void);
int taskwdTest(void);

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

    /*
     * Run the regular tests in alphabetical order
     */
    runTest(blockingSockTest);
    runTest(epicsAlgorithm);
    runTest(epicsAtomicTest);
    runTest(epicsCalcTest);
    runTest(epicsEllTest);
    runTest(epicsEnvTest);
    runTest(epicsErrlogTest);
    runTest(epicsEventTest);
    runTest(epicsInlineTest);
    runTest(epicsMathTest);
    runTest(epicsMessageQueueTest);
    runTest(epicsMMIOTest);
    runTest(epicsMutexTest);
    runTest(epicsSockResolveTest);
    runTest(epicsSpinTest);
    runTest(epicsStackTraceTest);
    runTest(epicsStdioTest);
    runTest(epicsStdlibTest);
    runTest(epicsStringTest);
    runTest(epicsThreadHooksTest);
    runTest(epicsThreadOnceTest);
    runTest(epicsThreadPoolTest);
    runTest(epicsThreadPriorityTest);
    runTest(epicsThreadPrivateTest);
    runTest(epicsTimeTest);
#ifdef __rtems__
    runTest(epicsTimeZoneTest);
#endif
    runTest(epicsTypesTest);
    runTest(ipAddrToAsciiTest);
    runTest(macDefExpandTest);
    runTest(macLibTest);
    runTest(ringBytesTest);
    runTest(ringPointerTest);
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
