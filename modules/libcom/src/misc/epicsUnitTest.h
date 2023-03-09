/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsUnitTest.h
 * \brief Unit test routines
 * \author Andrew Johnson
 *
 * The unit test routines make it easy for a test program to generate output
 * that is compatible with the Test Anything Protocol and can thus be used with
 * Perl's automated Test::Harness as well as generating human-readable output.
 * The routines detect whether they are being run automatically and print a
 * summary of the results at the end if not.

 * A test program starts with a call to testPlan(), announcing how many tests
 * are to be conducted. If this number is not known a value of zero can be
 * used during development, but it is recommended that the correct value be
 * substituted after the test program has been completed.
 *
 * Individual test results are reported using any of testOk(), testOk1(),
 * testOkV(), testPass() or testFail(). The testOk() call takes and also
 * returns a logical pass/fail result (zero means failure, any other value
 * is success) and a printf-like format string and arguments which describe
 * the test. The convenience macro testOk1() is provided which stringifies its
 * single condition argument, reducing the effort needed when writing test
 * programs. The individual testPass() and testFail() routines can be used when
 * the test program takes a different path on success than on failure, but one
 * or other must always be called for any particular test. The testOkV() routine
 * is a varargs form of testOk() included for internal purposes which may prove
 * useful in some cases.
 *
 * If some program condition or failure makes it impossible to run some tests,
 * the testSkip() routine can be used to indicate how many tests are being omitted
 * from the run, thus keeping the test counts correct; the constant string why is
 * displayed as an explanation to the user (this string is not printf-like).
 *
 * If some tests are expected to fail because functionality in the module under
 * test has not yet been fully implemented, these tests may still be executed,
 * wrapped between calls to testTodoBegin() and testTodoEnd(). testTodoBegin()
 * takes a constant string indicating why these tests are not expected to
 * succeed. This modifies the counting of the results so the wrapped tests will not
 * be recorded as failures.
 *
 * Additional information can be supplied using the testDiag() routine, which
 * displays the relevant information as a comment in the result output. None of
 * the printable strings passed to any testXxx() routine should contain a newline
 * '\\n' character, newlines will be added by the test routines as part of the
 * Test Anything Protocol. For multiple lines of diagnostic output, call
 * testDiag() as many times as necessary.
 *
 * If at any time the test program is unable to continue for some catastrophic
 * reason, calling testAbort() with an appropriate message will ensure that the
 * test harness understands this. testAbort() does not return, but calls the ANSI
 * C routine abort() to cause the program to stop immediately.
 *
 * After all of the tests have been completed, the return value from
 * testDone() can be used as the return status code from the program's main()
 * routine.
 *
 * On vxWorks and RTEMS, an alternative test harness can be used to run a
 * series of tests in order and summarize the results from them all at the end
 * just like the Perl harness does. The routine testHarness() is called once at
 * the beginning of the test harness program. Each test program is run by
 * passing its main routine name to the runTest() macro which expands into a call
 * to the runTestFunc() routine. The last test program or the harness program
 * itself must finish by calling testHarnessDone() which triggers the summary
 * mechanism to generate its result outputs (from an epicsAtExit() callback
 * routine).
 *
 * ### IOC Testing
 *
 * Some tests require the context of an IOC to be run. This conflicts with the
 * idea of running multiple tests within a test harness, as iocInit() is only
 * allowed to be called once, and some parts of the full IOC (e.g. the rsrv CA
 * server) can not be shut down cleanly. The function iocBuildIsolated() allows
 * to start an IOC without its Channel Access parts, so that it can be shutdown
 * quite cleanly using iocShutdown(). This feature is only intended to be used
 * from test programs, do not use it on production IOCs. After building the
 * IOC using iocBuildIsolated() or iocBuild(), it has to be started by calling
 * iocRun(). The suggested call sequence in a test program that needs to run the
 * IOC without Channel Access is:
\code
#include "iocInit.h"

MAIN(iocTest)
{
    testPlan(0);
    testdbPrepare();
    testdbReadDatabase("<dbdname>.dbd", 0, 0);
    <dbdname>_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("some.db", 0, 0);
    ... test code before iocInit().  eg. dbGetString() ...
    testIocInitOk();
    ... test code with IOC running.  eg. dbGet()
    testIocShutdownOk();
    testdbCleanup();
    return testDone();
}
\endcode

 * The part from iocBuildIsolated() to iocShutdown() can be repeated to
 * execute multiple tests within one executable or harness.
 *
 * To make it easier to create a single test program that can be built for
 * both the embedded and workstation operating system harnesses, the header file
 * testMain.h provides a convenience macro MAIN() that adjusts the name of the
 * test program according to the platform it is running on: main() on
 * workstations and a regular function name on embedded systems.
 *
 * ### Example
 *
 * The following is a simple example of a test program using the epicsUnitTest
 * routines:
\code
#include <math.h>
#include "epicsUnitTest.h"
#include "testMain.h"

MAIN(mathTest)
{
    testPlan(3);
    testOk(sin(0.0) == 0.0, "Sine starts");
    testOk(cos(0.0) == 1.0, "Cosine continues");
    if (!testOk1(M_PI == 4.0*atan(1.0)))
        testDiag("4 * atan(1) = %g", 4.0 * atan(1.0));
    return testDone();
}
\endcode

 * The output from running the above program looks like this:
\code
1..3
ok  1 - Sine starts
ok  2 - Cosine continues
ok  3 - M_PI == 4.0*atan(1.0)

    Results
    =======
       Tests: 3
      Passed:  3 = 100%
\endcode
 */

#ifndef INC_epicsUnitTest_H
#define INC_epicsUnitTest_H

#include <stdarg.h>

#include "compilerDependencies.h"
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Declare the test plan, required.
 * \param tests Number of tests to be run. May be zero if not known but the
 * test harness then can't tell if the program dies prematurely.
 */
LIBCOM_API void testPlan(int tests);

/** \name Announcing Test Results
 * Routines that declare individual test results.
 */
/** @{ */
/** \brief Test result with printf-style description.
 * \param pass True/False value indicating result.
 * \param fmt A printf-style format string describing the test.
 * \param ... Any parameters required for the format string.
 * \return The value of \p pass.
 */
LIBCOM_API int  testOk(int pass, EPICS_PRINTF_FMT(const char *fmt), ...)
    EPICS_PRINTF_STYLE(2, 3);
/** \brief Test result using expression as description
 * \param cond Expression to be evaluated and displayed.
 * \return The value of \p cond.
 */
#define testOk1(cond) testOk(cond, "%s", #cond)
/** \brief Test result with var-args description.
 * \param pass True/False value indicating result.
 * \param fmt A printf-style format string describing the test.
 * \param pvar A var-args pointer to any parameters for the format string.
 * \return The value of \p pass.
 */
LIBCOM_API int  testOkV(int pass, const char *fmt, va_list pvar);
/** \brief Passing test result with printf-style description.
 * \param fmt A printf-style format string describing the test.
 * \param ... Any parameters required for the format string.
 */
LIBCOM_API void testPass(EPICS_PRINTF_FMT(const char *fmt), ...)
    EPICS_PRINTF_STYLE(1, 2);
/** \brief Failing test result with printf-style description.
 * \param fmt A printf-style format string describing the test.
 * \param ... Any parameters required for the format string.
 */
LIBCOM_API void testFail(EPICS_PRINTF_FMT(const char *fmt), ...)
    EPICS_PRINTF_STYLE(1, 2);
/** @} */

/** \name Missing or Failing Tests
 * Routines for handling special situations.
 */
/** @{ */

/** \brief Place-holders for tests that can't be run.
 * \param skip How many tests are being skipped.
 * \param why Reason for skipping these tests.
 */
LIBCOM_API void testSkip(int skip, const char *why);
/** \brief Mark the start of a group of tests that are expected to fail
 * \param why Reason for expected failures.
 */
LIBCOM_API void testTodoBegin(const char *why);
/** \brief Mark the end of a failing test group.
 */
LIBCOM_API void testTodoEnd(void);
/** \brief Stop testing, program cannot continue.
 * \param fmt A printf-style format string giving the reason for stopping.
 * \param ... Any parameters required for the format string.
 */
LIBCOM_API void testAbort(EPICS_PRINTF_FMT(const char *fmt), ...)
    EPICS_PRINTF_STYLE(1, 2);
/** @} */

/** \brief Output additional diagnostics
 * \param fmt A printf-style format string containing diagnostic information.
 * \param ... Any parameters required for the format string.
 */
LIBCOM_API int  testDiag(EPICS_PRINTF_FMT(const char *fmt), ...)
    EPICS_PRINTF_STYLE(1, 2);
/** \brief Mark the end of testing.
 */
LIBCOM_API int  testDone(void);

/** \brief Return non-zero in shared/oversubscribed testing environments
 *
 * May be used to testSkip(), or select longer timeouts, for some cases
 * when the test process may be preempted for arbitrarily long times.
 * This is common in shared CI environments.
 *
 * The environment variable $EPICS_TEST_IMPRECISE_TIMING=YES should be
 * set in by such testing environments.
 */
LIBCOM_API
int testImpreciseTiming(void);

/** \name Test Harness for Embedded OSs
 * These routines are used to create a test-harness that can run
 * multiple test programs, collect their names and results, then
 * display a summary at the end of testing.
 */
/** @{ */

typedef int (*TESTFUNC)(void);
/** \brief Initialize test harness
 */
LIBCOM_API void testHarness(void);
/** \brief End of testing
 */
LIBCOM_API void testHarnessExit(void *dummy);
/** \brief Run a single test program
 * \param name Program name
 * \param func Function implementing test program
 */
LIBCOM_API void runTestFunc(const char *name, TESTFUNC func);

/** \brief Run a test program
 * \param func Name of the test program.
 */
#define runTest(func) runTestFunc(#func, func)
/** \brief Declare all test programs finished
 */
#define testHarnessDone() testHarnessExit(0)
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsUnitTest_H */
