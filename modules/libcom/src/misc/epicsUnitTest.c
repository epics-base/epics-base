/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Andrew Johnson
 *
 * Unit test module which generates output in the Test Anything Protocol
 * format.  See  perldoc Test::Harness  for details of output format.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#  include <crtdbg.h>
#  if !defined(_MSC_VER) || _MSC_VER>=1700
#    include <errhandlingapi.h>
/* provided by mingw and MSVC >= 2012 */
#    define HAVE_SETERROMODE
#  endif
#endif

#ifdef __rtems__
#  include <syslog.h>
#  ifndef RTEMS_LEGACY_STACK
#    include <rtems/bsd/bsd.h>
#  endif
#endif

#include "epicsThread.h"
#include "epicsMutex.h"
#include "epicsUnitTest.h"
#include "epicsExit.h"
#include "epicsTime.h"
#include "ellLib.h"
#include "errlog.h"
#include "cantProceed.h"
#include "epicsStackTrace.h"

typedef struct {
    ELLNODE node;
    const char *name;
    int tests;
    int failures;
    int skips;
} testFailure;

static epicsMutexId testLock = 0;
static int perlHarness;
static int planned;
static int tested;
static int passed;
static int failed;
static int skipped;
static int bonus;
static const char *todo;

epicsTimeStamp started;
static int Harness;
static int Programs;
static int Tests;

ELLLIST faults;
const char *testing = NULL;

static epicsThreadOnceId onceFlag = EPICS_THREAD_ONCE_INIT;

#ifdef _WIN32
/*
 * if we return FALSE, _CrtDbgReport is called to print to file etc
 * if we return TRUE, we are the only function called
 */
static int testReportHook(int reportType, char *message, int *returnValue)
{
    int nRet = 0;
    switch (reportType)
    {
        case _CRT_ASSERT:
        case _CRT_ERROR:
            epicsStackTrace();
            break;

        default:
            break;
   }
   if (returnValue)
   {
      *returnValue = 0;
   }
   return nRet;
}
#endif

static void testOnce(void *dummy) {
    testLock = epicsMutexMustCreate();
    perlHarness = (getenv("HARNESS_ACTIVE") != NULL);
#ifdef __rtems__
    // syslog() on RTEMS prints to stdout, which will interfere with test output.
    // setlogmask() ignores empty mask, so must allow at least one level.
    (void)setlogmask(LOG_MASK(LOG_CRIT));
    printf("# mask syslog() output\n");
#ifndef RTEMS_LEGACY_STACK
    // with libbsd  setlogmask() is a no-op :(
    // name strings in sys/syslog.h
    rtems_bsd_setlogpriority("crit");
#endif
#endif

#ifdef _WIN32
#ifdef HAVE_SETERROMODE
    /* SEM_FAILCRITICALERRORS - Don't display modal dialog
     * !SEM_NOALIGNMENTFAULTEXCEPT - auto-fix unaligned access
     * !SEM_NOGPFAULTERRORBOX - enable Windows Error Reporting (also enables postmortem debugger hooks)
     * SEM_NOOPENFILEERRORBOX - Don't display modal dialog
     */
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#endif
    /* Disable dialog for assertion failures */
    _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE |_CRTDBG_MODE_DEBUG );
    _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
    _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE |_CRTDBG_MODE_DEBUG );
    _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
    _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE |_CRTDBG_MODE_DEBUG );
    _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
    _CrtSetReportHook2( _CRT_RPTHOOK_INSTALL, testReportHook );
#endif
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
        result += 4;    /* skip "not " */
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
    fflush(stdout);
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
    fflush(stdout);
    epicsMutexUnlock(testLock);
}

void testTodoBegin(const char *why) {
    epicsMutexMustLock(testLock);
    todo = why;
    epicsMutexUnlock(testLock);
}

void testTodoEnd(void) {
    testTodoBegin(NULL);
}

int testDiag(const char *fmt, ...) {
    va_list pvar;
    va_start(pvar, fmt);
    epicsMutexMustLock(testLock);
    printf("# ");
    vprintf(fmt, pvar);
    putchar('\n');
    fflush(stdout);
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
    fflush(stdout);
    va_end(pvar);
    abort();
}

static void testResult(const char *result, int count) {
    printf("%12.12s: %3d = %5.2f%%\n", result, count, 100.0 * count / tested);
}

int testDone(void) {
    int status = 0;

    epicsMutexMustLock(testLock);
    if (perlHarness) {
        if (!planned)
            printf("1..%d\n", tested);
        else if (tested != planned)
            status = 2;
        if (failed)
            status |= 1;
    } else {
        if (planned && tested > planned) {
            printf("\nRan %d tests but only planned for %d!\n", tested, planned);
            status = 2;
        } else if (planned && tested < planned) {
            printf("\nPlanned %d tests but only ran %d\n", planned, tested);
            status = 2;
        }
        printf("\n    Results\n    =======\n       Tests: %-3d\n", tested);
        if (tested) {
            testResult("Passed", passed);
            if (bonus) testResult("Todo Passes", bonus);
            if (failed) {
                testResult("Failed", failed);
                status |= 1;
            }
            if (skipped) testResult("Skipped", skipped);
        }
    }
    if (Harness) {
        if (failed) {
            testFailure *fault = callocMustSucceed(1, sizeof(testFailure),
                "testDone calloc");
            fault->name     = testing;
            fault->tests    = tested;
            fault->failures = failed;
            fault->skips    = skipped;
            ellAdd(&faults, &fault->node);
        }
        Programs++;
        Tests += tested;
    }
    epicsMutexUnlock(testLock);
    return (status);
}

static int impreciseTiming;

int testImpreciseTiming(void)
{
    if(impreciseTiming==0) {
        const char* env = getenv("EPICS_TEST_IMPRECISE_TIMING");

        impreciseTiming = (env && strcmp(env, "YES")==0) ? 1 : -1;
    }
    return impreciseTiming>0;
}

/* Our test harness, for RTEMS and vxWorks */

void testHarnessExit(void *dummy) {
    epicsTimeStamp ended;
    int Faulty;

    if (!Harness) return;

    epicsTimeGetCurrent(&ended);

    printf("\n\n    EPICS Test Harness Results"
             "\n    ==========================\n\n");

    Faulty = ellCount(&faults);
    if (!Faulty)
        printf("All tests successful.\n");
    else {
        int Failures = 0;
        testFailure *f;

        printf("Failing Program           Tests  Faults\n"
               "---------------------------------------\n");
        while ((f = (testFailure *)ellGet(&faults))) {
            Failures += f->failures;
            printf("%-25s %5d   %5d\n", f->name, f->tests, f->failures);
            if (f->skips)
                printf("%d subtests skipped\n", f->skips);
            free(f);
        }
        printf("\nFailed %d/%d test programs. %d/%d subtests failed.\n",
               Faulty, Programs, Failures, Tests);
    }

    printf("Programs=%d, Tests=%d, %.0f wallclock secs\n\n",
           Programs, Tests, epicsTimeDiffInSeconds(&ended, &started));
}

void testHarness(void) {
    epicsThreadOnce(&onceFlag, testOnce, NULL);
    epicsAtExit(testHarnessExit, NULL);
    Harness = 1;
    Programs = 0;
    Tests = 0;
    ellInit(&faults);
    epicsTimeGetCurrent(&started);
}

void runTestFunc(const char *name, TESTFUNC func) {
    printf("\n***** %s *****\n", name);
    testing = name;
    func();     /* May not return */
    epicsThreadSleep(1.0);
}
