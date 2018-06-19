/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
// epicsAlgorithmTest.cpp
//	Authors: Jeff Hill & Andrew Johnson

#include "epicsUnitTest.h"
#include "epicsAlgorithm.h"
#include "epicsMath.h"
#include "testMain.h"

MAIN(epicsAlgorithm)
{
    testPlan(22);
    
    float f1 = 3.3f;
    float f2 = 3.4f;
    float Inf = epicsINF;
    float NaN = epicsNAN;
    
    testOk(epicsMin(f1, f2) == f1,     "epicsMin(f1, f2)");
    testOk(epicsMin(f1, -Inf) == -Inf, "epicsMin(f1, -Inf)");
    testOk(isnan(epicsMin(f1, NaN)),   "epicsMin(f1, NaN)");
    testOk(epicsMin(f1, Inf) == f1,    "epicsMin(f1, Inf)");
    
    testOk(epicsMin(f2, f1) == f1,     "epicsMin(f2, f1)");
    testOk(epicsMin(-Inf, f1) == -Inf, "epicsMin(-Inf, f1)");
    testOk(isnan(epicsMin(NaN, f1)),   "epicsMin(NaN, f1)");
    testOk(epicsMin(Inf, f1) == f1,    "epicsMin(Inf, f1)");
    
    testOk(epicsMax(f2, f1) == f2,     "epicsMax(f2, f1)");
    testOk(epicsMax(-Inf, f1) == f1,   "epicsMax(-Inf, f1)");
    testOk(isnan(epicsMax(NaN, f1)),   "epicsMax(NaN, f1)");
    testOk(epicsMax(Inf, f1) == Inf,   "epicsMax(Inf, f1)");
    
    testOk(epicsMax(f1, f2) == f2,     "epicsMax(f1, f2)");
    testOk(epicsMax(f1, -Inf) == f1,   "epicsMax(f1, -Inf)");
    testOk(isnan(epicsMax(f1, NaN)),   "epicsMax(f1, NaN)");
    testOk(epicsMax(f1, Inf) == Inf,   "epicsMax(f1, Inf)");
    
    epicsSwap(f1, f2);
    testOk(f1==3.4f && f2==3.3f, "epicsSwap(f1, f2)");
    
    int i1 = 3;
    int i2 = 4;
    
    testOk(epicsMin(i1, i2) == i1, "epicsMin(i1,i2)");
    testOk(epicsMin(i2, i1) == i1, "epicsMin(i2,i1)");
    
    testOk(epicsMax(i1, i2) == i2, "epicsMax(i1,i2)");
    testOk(epicsMax(i2, i1) == i2, "epicsMax(i2,i1)");
    
    epicsSwap(i1, i2);
    testOk(i1 == 4 && i2 == 3, "epicsSwap(i1, i2)");
    
    return testDone();
}

