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

#include <limits.h>

#define epicsExportSharedSymbols
#include "timerPrivate.h"

timerQueueActiveMgr::~timerQueueActiveMgr ()
{
    epicsAutoMutex locker ( this->mutex );
}
    
epicsTimerQueueActiveForC & timerQueueActiveMgr::allocate (
        bool okToShare, int threadPriority )
{
    epicsAutoMutex locker ( this->mutex );
    if ( okToShare ) {
        tsDLIterBD < epicsTimerQueueActiveForC > iter = this->sharedQueueList.firstIter ();
        while ( iter.valid () ) {
            if ( iter->threadPriority () == threadPriority ) {
                assert ( iter->timerQueueActiveMgrPrivate::referenceCount < UINT_MAX );
                iter->timerQueueActiveMgrPrivate::referenceCount++;
                return *iter;
            }
        }
    }
    epicsTimerQueueActiveForC *pQueue = new epicsTimerQueueActiveForC ( okToShare, threadPriority );
    if ( ! pQueue ) {
        throwWithLocation ( timer::noMemory () );
    }
    pQueue->timerQueueActiveMgrPrivate::referenceCount = 1u;
    if ( okToShare ) {
        this->sharedQueueList.add ( *pQueue );
    }
    return *pQueue;
}

void timerQueueActiveMgr::release ( epicsTimerQueueActiveForC &queue )
{
    epicsAutoMutex locker ( this->mutex );
    assert ( queue.timerQueueActiveMgrPrivate::referenceCount > 0u );
    queue.timerQueueActiveMgrPrivate::referenceCount--;
    if ( queue.timerQueueActiveMgrPrivate::referenceCount == 0u ) {
        if ( queue.sharingOK () ) {
            this->sharedQueueList.remove ( queue );
        }
        timerQueueActiveMgrPrivate *pPriv = &queue;
        delete pPriv;
    }
}

timerQueueActiveMgrPrivate::timerQueueActiveMgrPrivate () :
    referenceCount ( 0u )
{
}

timerQueueActiveMgrPrivate::~timerQueueActiveMgrPrivate () 
{
}
