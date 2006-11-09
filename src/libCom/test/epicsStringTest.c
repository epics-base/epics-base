/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* $Id$
 *
 *      Author  Marty Kraimer
 */

#include <stdio.h>

#include "epicsUnitTest.h"
#include "epicsString.h"
#include "testMain.h"

MAIN(epicsStringTest)
{
    testPlan(21);
    
    testOk1(epicsStrnCaseCmp("","",0)==0);
    testOk1(epicsStrnCaseCmp("","",1)==0);
    testOk1(epicsStrnCaseCmp(" ","",1)<0);
    testOk1(epicsStrnCaseCmp(""," ",1)>0);
    testOk1(epicsStrnCaseCmp("a","A",1)==0);
    testOk1(epicsStrnCaseCmp("a","A",2)==0);
    testOk1(epicsStrnCaseCmp("abcd","ABCD",2)==0);
    testOk1(epicsStrnCaseCmp("abcd","ABCD",4)==0);
    testOk1(epicsStrnCaseCmp("abcd","ABCD",1000)==0);
    testOk1(epicsStrnCaseCmp("abcd","ABCDE",2)==0);
    testOk1(epicsStrnCaseCmp("abcd","ABCDE",4)==0);
    testOk1(epicsStrnCaseCmp("abcd","ABCDE",1000)>0);
    testOk1(epicsStrnCaseCmp("abcde","ABCD",2)==0);
    testOk1(epicsStrnCaseCmp("abcde","ABCD",4)==0);
    testOk1(epicsStrnCaseCmp("abcde","ABCD",1000)<0);

    testOk1(epicsStrCaseCmp("","")==0);
    testOk1(epicsStrCaseCmp("a","A")==0);
    testOk1(epicsStrCaseCmp("abcd","ABCD")==0);
    testOk1(epicsStrCaseCmp("abcd","ABCDE")!=0);
    testOk1(epicsStrCaseCmp("abcde","ABCD")!=0);
    testOk1(epicsStrCaseCmp("abcde","ABCDF")!=0);

    return testDone();
}
