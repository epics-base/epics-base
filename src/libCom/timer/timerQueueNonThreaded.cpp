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
#include "epicsTimerPrivate.h"

epicsTimerQueueNonThreaded::~epicsTimerQueueNonThreaded () {}

epicsTimerQueueNonThreaded &epicsTimerQueueNonThreaded::create ( epicsTimerQueueNotify &notify )
{
    timerQueueNonThreaded *pQueue = new timerQueueNonThreaded ( notify );
    if ( ! pQueue ) {
        throwWithLocation ( timer::noMemory () );
    }
    return *pQueue;
}

timerQueueNonThreaded::timerQueueNonThreaded ( epicsTimerQueueNotify &notifyIn ) :
    queue ( notifyIn ) {}

timerQueueNonThreaded::~timerQueueNonThreaded () {}

epicsTimer & timerQueueNonThreaded::createTimer ( epicsTimerNotify & notifyIn )
{
    timer *pTmr = new timer ( notifyIn, this->queue );
    if ( ! pTmr ) {
        throwWithLocation ( timer::noMemory () );
    }
    return *pTmr;
}

void timerQueueNonThreaded::process ()
{
    this->queue.process ();
}

double timerQueueNonThreaded::getNextExpireDelay () const
{
    return this->queue.delayToFirstExpire ();
}

void timerQueueNonThreaded::show ( unsigned int level ) const
{
    printf ( "EPICS non-threaded timer queue at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level >=1u ) {
        this->queue.show ( level - 1u );
    }
}
