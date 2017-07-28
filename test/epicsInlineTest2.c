/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "compilerSpecific.h"
#include "epicsUnitTest.h"

static EPICS_ALWAYS_INLINE int epicsInlineTestFn1(void)
{
    return 2;
}

/* Fails to link in C99
inline int epicsInlineTestFn2(void)
{
    return 42;
}
*/

void epicsInlineTest2(void)
{
    testDiag("epicsInlineTest2()");
    testOk1(epicsInlineTestFn1()==2);
    /*testOk1(epicsInlineTestFn2()==42);*/
}
