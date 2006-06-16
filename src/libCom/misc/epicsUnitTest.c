/*************************************************************************\
* Copyright (c) 2006 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* $Id$
 * Author: Andrew Johnson
 *
 * Unit test module which generates Perl-compatible output.
 * See  perldoc Test::Harness  for details of output format.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "epicsMutex.h"

#define epicsExportSharedSymbols
#include "epicsUnitTest.h"

static epicsMutexId testLock = 0;
static int plan;
static int tests;
static int passed;
static int failed;
static int skipped;
static int bonus;
static const char *todo;

void testPlan(int tests) {
    if (testLock) abort();
    testLock = epicsMutexMustCreate();
    plan = tests;
    tests = passed = failed = skipped = bonus = 0;
    todo = NULL;
    if (plan) printf("1..%d\n", plan);
}

int testOk(int pass, const char *desc) {
    const char *result = "not ok";
    epicsMutexMustLock(testLock);
    tests++;
    if (pass) {
	result += 4;
	passed++;
	if (todo)
	    bonus++;
    } else {
	if (todo) 
	    passed++;
	else
	    failed++;
    }
    printf("%s %2d - %s", result, tests, desc);
    if (todo)
	printf(" # TODO %s", todo);
    puts("");
    epicsMutexUnlock(testLock);
    return pass;
}

void testPass(const char *desc) {
    testOk(1, desc);
}

void testFail(const char *desc) {
    testOk(0, desc);
}

void testSkip(int skip, const char *why) {
    epicsMutexMustLock(testLock);
    while (skip-- > 0) {
	tests++;
	passed++;
	skipped++;
	printf("ok %2d # SKIP %s\n", tests, why);
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
    printf("# ");
    vprintf(fmt, pvar);
    printf("\n");
    va_end(pvar);
    return 0;
}

void testAbort(const char *desc) {
    printf("Bail out! %s\n", desc);
    abort();
}

static void testResult(const char *result, int count) {
    printf("%12.12s: %2d = %d%%\n", result, count, 100 * count / tests);
}

int testDone(void) {
    int status = 0;
    char *value = getenv("HARNESS_ACTIVE");
    
    epicsMutexMustLock(testLock);
    if (value) {
	if (!plan) printf("1..%d\n", tests);
    } else {
	if (plan && tests > plan) {
	    printf("\nRan %d tests but only planned for %d!\n", tests, plan);
	    status = 2;
	} else if (plan && tests < plan) {
	    printf("\nPlanned %d tests but only ran %d\n", tests, plan);
	    status = 2;
	}
	printf("\n    Results\n    =======\n       Tests: %d\n", tests);
	if (tests) {
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
