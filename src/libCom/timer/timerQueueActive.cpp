/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <stdio.h>

#define epicsExportSharedSymbols
#include "timerPrivate.h"

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class epicsSingleton < timerQueueActiveMgr >;

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif

epicsSingleton < timerQueueActiveMgr > timerQueueMgrEPICS;

epicsTimerQueueActive::~epicsTimerQueueActive () {}

epicsTimerQueueActive & epicsTimerQueueActive::allocate ( bool okToShare, unsigned threadPriority )
{
    epicsSingleton < timerQueueActiveMgr >::reference pMgr = 
        timerQueueMgrEPICS.getReference ();
    return pMgr->allocate ( okToShare, threadPriority );
}

timerQueueActive::timerQueueActive ( bool okToShareIn, unsigned priority ) :
    queue ( *this ), thread ( *this, "timerQueue", 
        epicsThreadGetStackSize ( epicsThreadStackMedium ), priority ),
    sleepQuantum ( epicsThreadSleepQuantum() ), okToShare ( okToShareIn ), 
    exitFlag ( false ), terminateFlag ( false )
{
    this->thread.start ();
}

timerQueueActive::~timerQueueActive ()
{
    this->terminateFlag = true;
    this->rescheduleEvent.signal ();
    while ( ! this->exitFlag ) {
        this->exitEvent.wait ( 1.0 );
    }
    // in case other threads are waiting here also
    this->exitEvent.signal ();
}

void timerQueueActive::run ()
{
    this->exitFlag = false;
    while ( ! this->terminateFlag ) {
        double delay = this->queue.process ( epicsTime::getCurrent() );
        debugPrintf ( ( "timer thread sleeping for %g sec (max)\n", delay ) );
        this->rescheduleEvent.wait ( delay );
    }
    this->exitFlag = true; 
    this->exitEvent.signal (); // no access to queue after exitEvent signal
}

epicsTimer & timerQueueActive::createTimer ()
{
    return this->queue.createTimer();
}

epicsTimerForC & timerQueueActive::createTimerForC ( epicsTimerCallback pCallback, void * pArg )
{
    return this->queue.createTimerForC ( pCallback, pArg );
}

void timerQueueActive::reschedule ()
{
    this->rescheduleEvent.signal ();
}

double timerQueueActive::quantum ()
{
    return this->sleepQuantum;
}

void timerQueueActive::show ( unsigned int level ) const
{
    printf ( "EPICS threaded timer queue at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level >=1u ) {
        this->queue.show ( level - 1u );
        printf ( "reschedule event\n" );
        this->rescheduleEvent.show ( level - 1u );
        printf ( "exit event\n" );
        this->exitEvent.show ( level - 1u );
        printf ( "exitFlag = %c, terminateFlag = %c\n",
            this->exitFlag ? 'T' : 'F',
            this->terminateFlag ? 'T' : 'F' );
    }
}

epicsTimerQueue & timerQueueActive::getEpicsTimerQueue () 
{
    return static_cast < epicsTimerQueue &> ( * this );
}

