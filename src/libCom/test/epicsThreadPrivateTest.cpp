/* $Id$ */

/* Author: Jeff Hill Date:    March 28 2001 */


#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsAssert.h"

static bool doneFlag = false;
void epicsThreadPrivateTestThread ( void *pParm )
{
    epicsThreadPrivateId id = static_cast < epicsThreadPrivateId > ( pParm );
    assert ( 0 == epicsThreadPrivateGet ( id ) );
    static bool var;
    epicsThreadPrivateSet ( id, &var );
    assert ( &var == epicsThreadPrivateGet ( id ) );
    doneFlag = true;
}

inline void callItTenTimes ( const epicsThreadPrivateId &id )
{
    void *pParm;
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
    pParm = epicsThreadPrivateGet ( id );
}

inline void callItTenTimesSquared ( const epicsThreadPrivateId &id )
{
    callItTenTimes ( id );
    callItTenTimes ( id );
    callItTenTimes ( id );
    callItTenTimes ( id );
    callItTenTimes ( id );
    callItTenTimes ( id );
    callItTenTimes ( id );
    callItTenTimes ( id );
    callItTenTimes ( id );
    callItTenTimes ( id );
}

void epicsThreadPrivateTest ()
{
    epicsThreadPrivateId id = epicsThreadPrivateCreate ();
    assert ( id );
    static bool var;
    epicsThreadPrivateSet ( id, &var );
    assert ( &var == epicsThreadPrivateGet ( id ) );
    epicsThreadCreate ( "epicsThreadPrivateTest", epicsThreadPriorityMax, 
        epicsThreadStackSmall, epicsThreadPrivateTestThread, id );
    while ( ! doneFlag ) {
        epicsThreadSleep ( 0.01 );
    }
    assert ( &var == epicsThreadPrivateGet ( id ) );
    epicsThreadPrivateSet ( id, 0 );
    assert ( 0 == epicsThreadPrivateGet ( id ) );

    epicsTime begin = epicsTime::getCurrent ();
    static const unsigned N = 100000u;
    for ( unsigned i = 0u; i < N; i++ ) {
        callItTenTimesSquared ( id );
    }
    double delay = epicsTime::getCurrent() - begin;
    delay /= N * 100u; // convert to sec per call
    delay *= 1e6; // convert to micro sec
    printf ( "It takes %f micro sec to call epicsThreadPrivateGet()\n", delay );

    epicsThreadPrivateDelete ( id );
}

