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

unsigned timerQueueNonThreaded::useCount;
timerQueueNonThreaded * timerQueueNonThreaded::pQueue;
epicsMutex timerQueueNonThreaded::mutex;

epicsNonThreadedTimerQueue::~epicsNonThreadedTimerQueue () {}

epicsNonThreadedTimerQueue &epicsNonThreadedTimerQueue::allocate ()
{
    return epicsNonThreadedTimerQueue::allocate ();
}

timerQueueNonThreaded &timerQueueNonThreaded::allocate ()
{
    epicsAutoMutex locker ( timerQueueNonThreaded::mutex );
    if ( timerQueueNonThreaded::pQueue ) {
        timerQueueNonThreaded::useCount++;
        return *timerQueueNonThreaded::pQueue;
    }
    else {
        timerQueueNonThreaded::pQueue = new timerQueueNonThreaded ();
        if ( ! timerQueueNonThreaded::pQueue ) {
            throw timer::noMemory ();
        }
        timerQueueNonThreaded::useCount = 1u;
        return *timerQueueNonThreaded::pQueue;
    }
}

timerQueueNonThreaded::timerQueueNonThreaded () :
    queue ( *this )
{
}

timerQueueNonThreaded::~timerQueueNonThreaded ()
{

}

void timerQueueNonThreaded::release ()
{
    epicsAutoMutex locker ( timerQueueNonThreaded::mutex );
    assert ( timerQueueNonThreaded::useCount >= 1u );
    timerQueueNonThreaded::useCount--;
    if ( timerQueueNonThreaded::useCount == 0u ) {
        delete timerQueueNonThreaded::pQueue;
        timerQueueNonThreaded::pQueue = 0;
    }
}

epicsTimer & timerQueueNonThreaded::createTimer ( epicsTimerNotify & notify )
{
    timer *pTmr = new timer ( notify, this->queue );
    if ( ! pTmr ) {
        throwWithLocation ( timer::noMemory () );
    }
    return *pTmr;
}

void timerQueueNonThreaded::reschedule ()
{
    //
    // Only useful in a multi-threaded program ...
    //
    // If a select() based file descriptor manager was waiting
    // in select () this routine should force it to wake up
    // and rescheduale its delay, but since file descriptor managers
    // typically are only be used in single-threaded programs, and
    // in fact the file descriptor manager currently supplied
    // with EPICS is not thread safe, then perhaps
    // its not worth the effort to tightly integrate this with 
    // the file descriptor manager so that this can be implemented.
    //
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
