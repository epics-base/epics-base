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
#include "epicsTimer.h"
#include "epicsTimerPrivate.h"

void epicsTimerNotify::show ( unsigned /* level */ ) const {}

class epicsTimerNotifyForC : public epicsTimerNotify {
public:
    epicsTimerNotifyForC ( epicsTimerCallback, void *pPrivateIn );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~epicsTimerNotifyForC ();
private:
    epicsTimerCallback pCallBack;
    void * pPrivate;
    expireStatus expire ();
    static tsFreeList < epicsTimerNotifyForC > freeList;
};

tsFreeList < epicsTimerNotifyForC > epicsTimerNotifyForC::freeList;

inline void * epicsTimerNotifyForC::operator new ( size_t size )
{ 
    return epicsTimerNotifyForC::freeList.allocate ( size );
}

inline void epicsTimerNotifyForC::operator delete ( void *pCadaver, size_t size )
{ 
    epicsTimerNotifyForC::freeList.release ( pCadaver, size );
}

epicsTimerNotifyForC::epicsTimerNotifyForC ( epicsTimerCallback pCBIn, void *pPrivateIn ) :
    pCallBack ( pCBIn ), pPrivate ( pPrivateIn ) {}

    epicsTimerNotifyForC::~epicsTimerNotifyForC () {}

epicsTimerNotify::expireStatus epicsTimerNotifyForC::expire ()
{
    ( *this->pCallBack ) ( this->pPrivate );
    return noRestart;
}

extern "C" epicsNonThreadedTimerQueueId epicsShareAPI
    epicsNonThreadedTimerQueueAllocate ()
{
    try {
        epicsNonThreadedTimerQueue &queue = 
            epicsNonThreadedTimerQueue::allocate ();
        return &queue;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsShareAPI 
    epicsNonThreadedTimerQueueRelease ( epicsNonThreadedTimerQueueId pQueue )
{
    pQueue->release ();
}

extern "C" void epicsShareAPI 
    epicsNonThreadedTimerQueueProcess ( epicsNonThreadedTimerQueueId pQueue )
{
    pQueue->process ();
}

extern "C" double epicsShareAPI 
    epicsNonThreadedTimerQueueGetDelayToNextExpire (
        epicsNonThreadedTimerQueueId pQueue )
{
    return pQueue->getNextExpireDelay ();
}

extern "C" epicsTimerId epicsShareAPI epicsNonThreadedTimerQueueCreateTimer (
    epicsNonThreadedTimerQueueId pQueue,
    epicsTimerCallback pCallback, void *pArg )
{
    try {
        epicsTimerNotifyForC *pNotify = new epicsTimerNotifyForC ( pCallback, pArg );
        if ( ! pNotify ) {
            throw timer::noMemory ();
        }
        epicsTimer &tmr = pQueue->createTimer ( *pNotify );
        return &tmr;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void  epicsShareAPI epicsNonThreadedTimerQueueShow (
    epicsNonThreadedTimerQueueId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" epicsThreadedTimerQueueId epicsShareAPI
    epicsThreadedTimerQueueCreate ( int okToShare, unsigned int threadPriority )
{
    try {
        epicsThreadedTimerQueue & queue = 
            epicsThreadedTimerQueue::allocate 
                ( okToShare ? true : false, threadPriority );
        return &queue;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsShareAPI epicsThreadedTimerQueueDelete ( epicsThreadedTimerQueueId pQueue )
{
    pQueue->release ();
}

extern "C" epicsTimerId epicsShareAPI epicsThreadedTimerQueueCreateTimer (
    epicsThreadedTimerQueueId pQueue, epicsTimerCallback pCallback, void *pArg )
{
    try {
        epicsTimerNotifyForC *pNotify = new epicsTimerNotifyForC ( pCallback, pArg );
        if ( ! pNotify ) {
            throw timer::noMemory ();
        }
        epicsTimer &tmr = pQueue->createTimer ( *pNotify );
        return &tmr;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void  epicsShareAPI epicsThreadedTimerQueueShow (
    epicsThreadedTimerQueueId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" void  epicsShareAPI epicsTimerDestroy ( epicsTimerId pTmr )
{
    delete pTmr;
}

extern "C" void  epicsShareAPI epicsTimerStartTime (
    epicsTimerId pTmr, const epicsTimeStamp *pTime )
{
    pTmr->start ( *pTime );
}

extern "C" void  epicsShareAPI epicsTimerStartDelay (
    epicsTimerId pTmr, double delaySeconds )
{
    pTmr->start ( delaySeconds );
}

extern "C" void  epicsShareAPI epicsTimerCancel ( epicsTimerId pTmr )
{
    pTmr->cancel ();
}

extern "C" double  epicsShareAPI epicsTimerGetExpireDelay ( epicsTimerId pTmr )
{
    return pTmr->getExpireDelay ();
}

extern "C" void  epicsShareAPI epicsTimerShow (
    epicsTimerId pTmr, unsigned int level )
{
    pTmr->show ( level );
}

