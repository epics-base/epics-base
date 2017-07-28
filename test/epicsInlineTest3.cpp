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
    return 3;
}

inline int epicsInlineTestFn2(void)
{
    return 42;
}

extern "C"
void epicsInlineTest3(void)
{
    testDiag("epicsInlineTest3()");
    testOk1(epicsInlineTestFn1()==3);
    testOk1(epicsInlineTestFn2()==42);
}
