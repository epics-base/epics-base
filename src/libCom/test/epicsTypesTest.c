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

MAIN(epicsTypesTest)
{
    testPlan(8);

    testOk1(sizeof(epicsInt8) == 1);
    testOk1(sizeof(epicsUInt8) == 1);

    testOk1(sizeof(epicsInt16) == 2);
    testOk1(sizeof(epicsUInt16) == 2);

    testOk1(sizeof(epicsInt32) == 4);
    testOk1(sizeof(epicsUInt32) == 4);

    testOk1(sizeof(epicsFloat32) == 4);
    testOk1(sizeof(epicsFloat64) == 8);

    return testDone();
}
