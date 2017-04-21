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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

//
// Note, a free list for this class is not currently used because of 
// entanglements between the file scope free list destructor and a
// file scope fdManager destructor which is trying to call a 
// destructor for a passive timer queue which is no longer valid
// in pool.
// 

#include <stdio.h>

#define epicsExportSharedSymbols
#include "timerPrivate.h"

epicsTimerQueuePassive::~epicsTimerQueuePassive () {}

epicsTimerQueuePassive & epicsTimerQueuePassive::create ( epicsTimerQueueNotify &notify )
{
    return * new timerQueuePassive ( notify );
}

timerQueuePassive::timerQueuePassive ( epicsTimerQueueNotify &notifyIn ) :
    queue ( notifyIn ) {}

timerQueuePassive::~timerQueuePassive () {}

epicsTimer & timerQueuePassive::createTimer ()
{
    return this->queue.createTimer ();
}

epicsTimerForC & timerQueuePassive::createTimerForC ( epicsTimerCallback pCallback, void * pArg )
{
    return this->queue.createTimerForC ( pCallback, pArg );
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

epicsTimerQueue & timerQueuePassive::getEpicsTimerQueue () 
{
    return static_cast < epicsTimerQueue &> ( * this );
}

