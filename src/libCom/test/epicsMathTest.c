/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsMathTest.c
 *
 *      Author  Marty Kraimer
 */

#include "epicsUnitTest.h"
#include "epicsMath.h"

int epicsMathTest ()
{
    double a,b,c;
    
    testPlan(18);
    
    a = 0.0;
    b = 1.0;
    c = a / b;
    testOk(!isnan(c), "!isnan(0.0 / 1.0)");
    testOk(!isinf(c), "!isinf(0.0 / 1.0)");
    testOk(c == 0.0, "0.0 / 1.0 == 0.0");
    
    a = 1.0;
    b = 0.0;
    c = a / b;
    testOk(!isnan(c), "!isnan(1.0 / 0.0)");
    testOk(isinf(c), "isinf(1.0 / 0.0)");
    testOk(c == c, "1.0 / 0.0 == 1.0 / 0.0");
    
    a = 0.0;
    b = 0.0;
    c = a / b;
    testOk(isnan(c), "isnan(0.0 / 0.0)");
    testOk(!isinf(c), "!isinf(0.0 / 0.0)");
    testOk(c != c, "0.0 / 0.0 != 0.0 / 0.0");
    
    a = 1e300;
    b = 1e-300;
    c = a / b;
    testOk(!isnan(c), "!isnan(1e300 / 1e-300)");
    testOk(isinf(c), "isinf(1e300 / 1e-300)");
    testOk(c > 0.0, "1e300 / 1e-300 > 0.0");
    
    a = -1e300;
    b = 1e-300;
    c = a / b;
    testOk(!isnan(c), "!isnan(-1e300 / 1e-300)");
    testOk(isinf(c), "isinf(-1e300 / 1e-300)");
    testOk(c < 0.0, "-1e300 / 1e-300 < 0.0");
    
    a = 1e300;
    b = 1e300;
    c = a / b;
    testOk(!isnan(c), "!isnan(1e300 / 1e300)");
    testOk(!isinf(c), "!isinf(1e300 / 1e300)");
    testOk(c == 1.0, "1e300 / 1e300 == 1.0");
    
    return testDone();
}
