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

tsFreeList < class timerQueuePassive, 0x8 > timerQueuePassive::freeList;
epicsMutex timerQueuePassive::freeListMutex;

epicsTimerQueuePassive::~epicsTimerQueuePassive () {}

epicsTimerQueuePassive &epicsTimerQueuePassive::create ( epicsTimerQueueNotify &notify )
{
    timerQueuePassive *pQueue = new timerQueuePassive ( notify );
    if ( ! pQueue ) {
        throwWithLocation ( timer::noMemory () );
    }
    return *pQueue;
}

timerQueuePassive::timerQueuePassive ( epicsTimerQueueNotify &notifyIn ) :
    queue ( notifyIn ) {}

timerQueuePassive::~timerQueuePassive () {}

epicsTimer & timerQueuePassive::createTimer ()
{
    timer *pTmr = new timer ( this->queue );
    if ( ! pTmr ) {
        throwWithLocation ( timer::noMemory () );
    }
    return *pTmr;
}

double timerQueuePassive::process ( const epicsTime & currentTime )
{
    return this->queue.process ( currentTime );
}

void timerQueuePassive::show ( unsigned int level ) const
{
    printf ( "EPICS non-threaded timer queue at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level >=1u ) {
        this->queue.show ( level - 1u );
    }
}
