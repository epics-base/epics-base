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

void threadTest(int ntasks,int verbose);
void epicsTimerTest();
int epicsAlgorithm(int /*argc*/, char* /*argv[]*/);
int epicsCalcTest(int /*argc*/, char* /*argv[]*/);
void epicsEventTest(int nthreads,int verbose);
int epicsExceptionTest();
int epicsMathTest();
void epicsMessageQueueTest(void *parm);
void epicsMutexTest(int nthreads,int verbose);
void epicsOkToBlockTest(void);
int epicsStdioTest (const char *report);
int epicsStringTest();
void epicsThreadPriorityTest(void *);
void epicsThreadPrivateTest();
int epicsTimeTest(void);
int macEnvExpandTest(void);
void ringPointerTest();
void blockingSockTest (void);

void
epicsRunLibComTests(void)
{
    /*
     * Thread startup sets some internal variables so do it first
     */
	printf("\n****** Thread Test *****\n");
	threadTest (2, 0);
 	epicsThreadSleep (1.0);

    /*
     * Timer tests get confused if run after some of the other tests
     */
	printf("\n****** Timer Test *****\n");
	epicsTimerTest ();
 	epicsThreadSleep (1.0);

	printf("\n****** Algorithm Test *****\n");
	epicsAlgorithm (0, NULL);
 	epicsThreadSleep (1.0);

	printf("\n****** Calculation Test *****\n");
    epicsCalcTest(0, NULL);
 	epicsThreadSleep (1.0);

	printf("\n****** Event Test *****\n");
	epicsEventTest (2,0);
 	epicsThreadSleep (1.0);

	printf("\n****** Exception Test *****\n");
  	epicsExceptionTest ();
 	epicsThreadSleep (1.0);

	printf("\n****** Math Test *****\n");
	epicsMathTest ();
 	epicsThreadSleep (1.0);

	printf("\n****** Message Queue Test *****\n");
	epicsMessageQueueTest (NULL);
 	epicsThreadSleep (1.0);

	printf("\n****** Mutex Test *****\n");
	epicsMutexTest (2, 0);
 	epicsThreadSleep (1.0);

	printf("\n****** OK to Block Test *****\n");
	epicsOkToBlockTest ();
 	epicsThreadSleep (1.0);

	printf("\n****** Stdio Test *****\n");
	epicsStdioTest ("report");
 	epicsThreadSleep (1.0);

	printf("\n****** String Test *****\n");
	epicsStringTest ();
 	epicsThreadSleep (1.0);

	printf("\n****** Thread Priority Test *****\n");
	epicsThreadPriorityTest (NULL);
 	epicsThreadSleep (1.0);

	printf("\n****** Thread Private Test *****\n");
	epicsThreadPrivateTest (NULL);
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

	printf("\n****** Tests Completed *****\n");
 	epicsThreadSleep (1.0);
}
