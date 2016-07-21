/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author: Jeff Hill Date:    March 28 2001 */

#include <stdio.h>

#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

static epicsThreadPrivate < bool > priv;

extern "C" void epicsThreadPrivateTestThread ( void * )
{
    testOk1 ( NULL == priv.get () );
    bool var = true;
    priv.set ( &var );
    testOk1 ( &var == priv.get () );
}

MAIN(epicsThreadPrivateTest)
{
    testPlan(5);

    bool var = false;
    priv.set ( &var );
    testOk1 ( &var == priv.get() );

    epicsThreadCreate ( "epicsThreadPrivateTest", epicsThreadPriorityMax, 
        epicsThreadGetStackSize ( epicsThreadStackSmall ), 
        epicsThreadPrivateTestThread, 0 );
    epicsThreadSleep ( 1.0 );
    testOk1 ( &var == priv.get() );

    priv.set ( NULL );
    testOk1 ( NULL == priv.get() );

    return testDone();
}

