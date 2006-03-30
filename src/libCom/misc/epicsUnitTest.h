/*************************************************************************\
* Copyright (c) 2006 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* $Id$
 * Author: Andrew Johnson
 */

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void testPlan(int tests);
epicsShareFunc int  testOk(int pass, const char *desc);
epicsShareFunc void testPass(const char *desc);
epicsShareFunc void testFail(const char *desc);
epicsShareFunc void testSkip(int skip, const char *why);
epicsShareFunc void testTodoBegin(const char *why);
epicsShareFunc void testTodoEnd();
epicsShareFunc int  testDiag(const char *fmt, ...);
epicsShareFunc void testAbort(const char *desc);
epicsShareFunc int  testDone(void);

#define testOk1(cond) testOk(cond, #cond)

#ifdef __cplusplus
}
#endif
