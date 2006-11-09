/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Run tests as a batch
 *
 * This is part of the work being done to provide a unified set of automated
 * tests for EPICS.  Many more changes will be forthcoming.
 */
#include <stdio.h>
#include <epicsThread.h>
#include <epicsExit.h>

int epicsThreadTest(void);
int epicsTimerTest(void);
int epicsAlgorithm(void);
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
int macEnvExpandTest(void);
int ringPointerTest(void);
int blockingSockTest(void);
int epicsExitTest(void);

void
epicsRunLibComTests(void)
{
	/*
	 * Thread startup sets some internal variables so do it first
	 */
	printf("\n****** Thread Test *****\n");
	epicsThreadTest ();
	epicsThreadSleep (1.0);

	/*
	 * Timer tests get confused if run after some of the other tests
	 */
	printf("\n****** Timer Test *****\n");
	epicsTimerTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Algorithm Test *****\n");
	epicsAlgorithm ();
	epicsThreadSleep (1.0);

	printf("\n****** Calculation Test *****\n");
	epicsCalcTest();
	epicsThreadSleep (1.0);

	printf("\n****** Event Test *****\n");
	epicsEventTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Exception Test *****\n");
	epicsExceptionTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Math Test *****\n");
	epicsMathTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Message Queue Test *****\n");
	epicsMessageQueueTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Mutex Test *****\n");
	epicsMutexTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Stdio Test *****\n");
	epicsStdioTest ();
	epicsThreadSleep (1.0);

	printf("\n****** String Test *****\n");
	epicsStringTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Thread Priority Test *****\n");
	epicsThreadPriorityTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Thread Private Test *****\n");
	epicsThreadPrivateTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Time Test *****\n");
	epicsTimeTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Macro Environment Variable Expansion Test *****\n");
	macEnvExpandTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Ring Pointer Test *****\n");
	ringPointerTest ();
	epicsThreadSleep (1.0);

	printf("\n****** Check socket behaviour *****\n");
	blockingSockTest();
	epicsThreadSleep (1.0);

	/*
	 * Must come last
	 */
	printf("\n****** EpicsExit Test *****\n");
	epicsExitTest();    /* Never returns */
}
