/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* This test checks the variations on inline function defintions.
 *
 *  "static inline int func(void) {...}"
 *
 * Consistent meaning in C89, C99, and C++ (98 and 11).
 * If not inline'd results in a private symbol in each compilation unit.
 * Thus the non-inline'd version is duplicated in each compilation unit.
 * However, definitions in different compilation units may be different.
 *
 *  "inline int func(void) {...}"
 * Warning: Not consistent, avoid use in headers meant for C or C++
 *
 * In C++ this may be safely defined in more than one compilation unit.
 * Where not inlined it will result in a weak public symbol.
 * Thus non-inline'd version isn't duplicated, but must be the same
 * in all compilation units.
 *
 */

#include "compilerSpecific.h"
#include "epicsUnitTest.h"

#include "testMain.h"

static EPICS_ALWAYS_INLINE int epicsInlineTestFn1(void)
{
    return 1;
}

/* Fails to link in C99
inline int epicsInlineTestFn2(void)
{
    return 42;
}
*/

void epicsInlineTest1(void)
{
    testDiag("epicsInlineTest1()");
    testOk1(epicsInlineTestFn1()==1);
    /*testOk1(epicsInlineTestFn2()==42);*/
}

void epicsInlineTest2(void);
void epicsInlineTest3(void);
void epicsInlineTest4(void);

MAIN(epicsInlineTest)
{
  testPlan(6);
  testDiag("Test variation on inline int func()");
  epicsInlineTest1();
  epicsInlineTest2();
  epicsInlineTest3();
  epicsInlineTest4();
  return testDone();
}
