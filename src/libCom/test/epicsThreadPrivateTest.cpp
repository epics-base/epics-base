/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* $Id$ */

/* Author: Jeff Hill Date:    March 28 2001 */

#include <stdio.h>

#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

static epicsThreadPrivate < bool > priv;

static bool doneFlag = false;

extern "C" void epicsThreadPrivateTestThread ( void * )
{
    testOk1 ( 0 == priv.get () );
    static bool var;
    priv.set ( &var );
    testOk1 ( &var == priv.get () );
    doneFlag = true;
}

inline void callItTenTimes ()
{
    bool *pFlag;
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
    pFlag = priv.get ();
}

inline void callItTenTimesSquared ()
{
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
    callItTenTimes ();
}

MAIN(epicsThreadPrivateTest)
{
    testPlan(5);

    static bool var;
    priv.set ( &var );
    testOk1 ( &var == priv.get() );

    epicsThreadCreate ( "epicsThreadPrivateTest", epicsThreadPriorityMax, 
        epicsThreadGetStackSize ( epicsThreadStackSmall ), 
        epicsThreadPrivateTestThread, 0 );
    while ( ! doneFlag ) {
        epicsThreadSleep ( 0.1 );
    }
    testOk1 ( &var == priv.get() );

    priv.set ( 0 );
    testOk1 ( 0 == priv.get() );

    epicsTime begin = epicsTime::getCurrent ();
    static const unsigned N = 1000u;
    for ( unsigned i = 0u; i < N; i++ ) {
        callItTenTimesSquared ();
    }
    double delay = epicsTime::getCurrent() - begin;
    delay /= N * 100u; // convert to sec per call
    delay *= 1e6; // convert to micro sec
    testDiag("epicsThreadPrivateGet() takes %f microseconds", delay);

    return testDone();
}

