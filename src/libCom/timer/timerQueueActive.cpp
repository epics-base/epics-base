/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#define epicsExportSharedSymbols
#include "timerPrivate.h"

timerQueueActiveMgr queueMgr;

epicsTimerQueueActive::~epicsTimerQueueActive () {}

epicsTimerQueueActive &epicsTimerQueueActive::allocate ( bool okToShare, int threadPriority )
{
    return queueMgr.allocate ( okToShare, threadPriority );
}

timerQueueActive::timerQueueActive ( bool okToShareIn, unsigned priority ) :
    queue ( *this ), thread ( *this, "epicsTimerQueueActive",
        epicsThreadGetStackSize ( epicsThreadStackMedium ), priority ),
    okToShare ( okToShareIn ), exitFlag ( false ), terminateFlag ( false )
{
    this->thread.start ();
}

timerQueueActive::~timerQueueActive ()
{
    this->terminateFlag = true;
    this->rescheduleEvent.signal ();
    while ( ! this->exitFlag && ! this->thread.isSuspended () ) {
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
    timer *pTmr = new timer ( this->queue );
    if ( ! pTmr ) {
        throwWithLocation ( timer::noMemory () );
    }
    return *pTmr;
}

void timerQueueActive::reschedule ()
{
    this->rescheduleEvent.signal ();
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
