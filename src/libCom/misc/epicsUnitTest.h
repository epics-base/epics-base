/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Andrew Johnson
 */

#ifndef INC_epicsUnitTest_H
#define INC_epicsUnitTest_H

#include <stdarg.h>

#include "compilerDependencies.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void testPlan(int tests);
epicsShareFunc int  testOkV(int pass, const char *fmt, va_list pvar);
epicsShareFunc int  testOk(int pass, const char *fmt, ...)
						EPICS_PRINTF_STYLE(2, 3);
epicsShareFunc void testPass(const char *fmt, ...)
						EPICS_PRINTF_STYLE(1, 2);
epicsShareFunc void testFail(const char *fmt, ...)
						EPICS_PRINTF_STYLE(1, 2);
epicsShareFunc void testSkip(int skip, const char *why);
epicsShareFunc void testTodoBegin(const char *why);
epicsShareFunc void testTodoEnd(void);
epicsShareFunc int  testDiag(const char *fmt, ...)
						EPICS_PRINTF_STYLE(1, 2);
epicsShareFunc void testAbort(const char *fmt, ...)
						EPICS_PRINTF_STYLE(1, 2);
epicsShareFunc int  testDone(void);

#define testOk1(cond) testOk(cond, "%s", #cond)


typedef int (*TESTFUNC)(void);
epicsShareFunc void testHarness(void);
epicsShareFunc void testHarnessExit(void *dummy);
epicsShareFunc void runTestFunc(const char *name, TESTFUNC func);

#define runTest(func) runTestFunc(#func, func)
#define testHarnessDone() testHarnessExit(0)

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsUnitTest_H */
