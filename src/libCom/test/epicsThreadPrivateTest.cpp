/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$ */

/* Author: Jeff Hill Date:    March 28 2001 */

#include <stdio.h>

#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsAssert.h"

static epicsThreadPrivate < bool > priv;

static bool doneFlag = false;

extern "C" void epicsThreadPrivateTestThread ( void * )
{
    assert ( 0 == priv.get () );
    static bool var;
    priv.set ( &var );
    assert ( &var == priv.get () );
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

extern "C" void epicsThreadPrivateTest ()
{
    static bool var;
    priv.set ( &var );
    assert ( &var == priv.get() );
    epicsThreadCreate ( "epicsThreadPrivateTest", epicsThreadPriorityMax, 
        epicsThreadGetStackSize ( epicsThreadStackSmall ), 
        epicsThreadPrivateTestThread, 0 );
    while ( ! doneFlag ) {
        epicsThreadSleep ( 0.1 );
    }
    assert ( &var == priv.get() );
    priv.set ( 0 );
    assert ( 0 == priv.get() );

    epicsTime begin = epicsTime::getCurrent ();
    static const unsigned N = 1000u;
    for ( unsigned i = 0u; i < N; i++ ) {
        callItTenTimesSquared ();
    }
    double delay = epicsTime::getCurrent() - begin;
    delay /= N * 100u; // convert to sec per call
    delay *= 1e6; // convert to micro sec
    printf ( "It takes %f micro sec to call epicsThreadPrivateGet()\n", delay );
}

