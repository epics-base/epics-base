/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsTypesTest.c
 *
 * Size-check epicsTypes
 *
 */

#include "epicsUnitTest.h"
#include "epicsTypes.h"
#include "testMain.h"
#include "epicsAssert.h"

/*
 * Might as well check at compile-time too, since we can.
 */

STATIC_ASSERT(sizeof(epicsInt8) == 1);
STATIC_ASSERT(sizeof(epicsUInt8) == 1);
STATIC_ASSERT(sizeof(epicsInt16) == 2);
STATIC_ASSERT(sizeof(epicsUInt16) == 2);
STATIC_ASSERT(sizeof(epicsInt32) == 4);
STATIC_ASSERT(sizeof(epicsUInt32) == 4);
STATIC_ASSERT(sizeof(epicsInt64) == 8);
STATIC_ASSERT(sizeof(epicsUInt64) == 8);
STATIC_ASSERT(sizeof(epicsFloat32) == 4);
STATIC_ASSERT(sizeof(epicsFloat64) == 8);

MAIN(epicsTypesTest)
{
    testPlan(10);

    testOk1(sizeof(epicsInt8) == 1);
    testOk1(sizeof(epicsUInt8) == 1);

    testOk1(sizeof(epicsInt16) == 2);
    testOk1(sizeof(epicsUInt16) == 2);

    testOk1(sizeof(epicsInt32) == 4);
    testOk1(sizeof(epicsUInt32) == 4);

    testOk1(sizeof(epicsInt64) == 8);
    testOk1(sizeof(epicsUInt64) == 8);

    testOk1(sizeof(epicsFloat32) == 4);
    testOk1(sizeof(epicsFloat64) == 8);

    return testDone();
}
