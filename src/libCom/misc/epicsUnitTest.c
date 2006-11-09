/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* $Id$
 * Author: Andrew Johnson
 *
 * Unit test module which generates output in the Test Anything Protocol
 * format.  See  perldoc Test::Harness  for details of output format.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "epicsThread.h"
#include "epicsMutex.h"

#define epicsExportSharedSymbols
#include "epicsUnitTest.h"

static epicsMutexId testLock = 0;
static int planned;
static int tested;
static int passed;
static int failed;
static int skipped;
static int bonus;
static const char *todo;

static epicsThreadOnceId onceFlag = EPICS_THREAD_ONCE_INIT;

static void testOnce(void *dummy) {
    testLock = epicsMutexMustCreate();
}

void testPlan(int plan) {
    epicsThreadOnce(&onceFlag, testOnce, NULL);
    epicsMutexMustLock(testLock);
    planned = plan;
    tested = passed = failed = skipped = bonus = 0;
    todo = NULL;
    if (plan) printf("1..%d\n", plan);
    epicsMutexUnlock(testLock);
}

int testOkV(int pass, const char *fmt, va_list pvar) {
    const char *result = "not ok";
    epicsMutexMustLock(testLock);
    tested++;
    if (pass) {
	result += 4;	/* skip "not " */
	passed++;
	if (todo)
	    bonus++;
    } else {
	if (todo) 
	    passed++;
	else
	    failed++;
    }
    printf("%s %2d - ", result, tested);
    vprintf(fmt, pvar);
    if (todo)
	printf(" # TODO %s", todo);
    putchar('\n');
    epicsMutexUnlock(testLock);
    return pass;
}

int testOk(int pass, const char *fmt, ...) {
    va_list pvar;
    va_start(pvar, fmt);
    testOkV(pass, fmt, pvar);
    va_end(pvar);
    return pass;
}

void testPass(const char *fmt, ...) {
    va_list pvar;
    va_start(pvar, fmt);
    testOkV(1, fmt, pvar);
    va_end(pvar);
}

void testFail(const char *fmt, ...) {
    va_list pvar;
    va_start(pvar, fmt);
    testOkV(0, fmt, pvar);
    va_end(pvar);
}

void testSkip(int skip, const char *why) {
    epicsMutexMustLock(testLock);
    while (skip-- > 0) {
	tested++;
	passed++;
	skipped++;
	printf("ok %2d # SKIP %s\n", tested, why);
    }
    epicsMutexUnlock(testLock);
}

void testTodoBegin(const char *why) {
    epicsMutexMustLock(testLock);
    todo = why;
    epicsMutexUnlock(testLock);
}

void testTodoEnd() {
    todo = NULL;
}

int testDiag(const char *fmt, ...) {
    va_list pvar;
    va_start(pvar, fmt);
    epicsMutexMustLock(testLock);
    printf("# ");
    vprintf(fmt, pvar);
    putchar('\n');
    epicsMutexUnlock(testLock);
    va_end(pvar);
    return 0;
}

void testAbort(const char *fmt, ...) {
    va_list pvar;
    va_start(pvar, fmt);
    printf("Bail out! ");
    vprintf(fmt, pvar);
    putchar('\n');
    va_end(pvar);
    abort();
}

static void testResult(const char *result, int count) {
    printf("%12.12s: %2d = %d%%\n", result, count, 100 * count / tested);
}

int testDone(void) {
    int status = 0;
    char *harness = getenv("HARNESS_ACTIVE");
    
    epicsMutexMustLock(testLock);
    if (harness) {
	if (!planned) printf("1..%d\n", tested);
    } else {
	if (planned && tested > planned) {
	    printf("\nRan %d tests but only planned for %d!\n", tested, planned);
	    status = 2;
	} else if (planned && tested < planned) {
	    printf("\nPlanned %d tests but only ran %d\n", planned, tested);
	    status = 2;
	}
	printf("\n    Results\n    =======\n       Tests: %d\n", tested);
	if (tested) {
	    testResult("Passed", passed);
	    if (bonus) testResult("Todo Passes", bonus);
	    if (failed) {
		testResult("Failed", failed);
		status = 1;
	    }
	    if (skipped) testResult("Skipped", skipped);
	}
    }
    epicsMutexUnlock(testLock);
    return (status);
}
