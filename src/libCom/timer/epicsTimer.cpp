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

class epicsTimerQueueNotifyForC : public epicsTimerQueueNotify {
public:
    epicsTimerQueueNotifyForC ( epicsTimerQueueReschedualCallback pCallback, void *pPrivate );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~epicsTimerQueueNotifyForC ();
private:
    epicsTimerQueueReschedualCallback pCallback;
    void *pPrivate;
    void reschedule ();
    static tsFreeList < epicsTimerQueueNotifyForC > freeList;
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

tsFreeList < epicsTimerQueueNotifyForC > epicsTimerQueueNotifyForC::freeList;

inline void * epicsTimerQueueNotifyForC::operator new ( size_t size )
{ 
    return epicsTimerQueueNotifyForC::freeList.allocate ( size );
}

inline void epicsTimerQueueNotifyForC::operator delete ( void *pCadaver, size_t size )
{ 
    epicsTimerQueueNotifyForC::freeList.release ( pCadaver, size );
}


epicsTimerQueueNotifyForC::epicsTimerQueueNotifyForC ( epicsTimerQueueReschedualCallback pCallbackIn, void *pPrivateIn ) :
    pCallback ( pCallbackIn ), pPrivate ( pPrivateIn ) {}

epicsTimerQueueNotifyForC::~epicsTimerQueueNotifyForC () {}

void epicsTimerQueueNotifyForC::reschedule ()
{
    (*this->pCallback) ( this->pPrivate );
}

extern "C" epicsTimerQueueNonThreadedId epicsShareAPI
    epicsTimerQueueNonThreadedCreate ( epicsTimerQueueReschedualCallback pCallbackIn, void *pPrivateIn )
{
    try {
        epicsTimerQueueNotifyForC *pNotify = new epicsTimerQueueNotifyForC ( pCallbackIn, pPrivateIn );
        if ( ! pNotify ) {
            throw timer::noMemory ();
        }
        epicsTimerQueueNonThreaded &queue = 
            epicsTimerQueueNonThreaded::create ( *pNotify );
        return &queue;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsShareAPI 
    epicsTimerQueueNonThreadedDestroy ( epicsTimerQueueNonThreadedId pQueue )
{
    delete pQueue;
}

extern "C" void epicsShareAPI 
    epicsTimerQueueNonThreadedProcess ( epicsTimerQueueNonThreadedId pQueue )
{
    pQueue->process ();
}

extern "C" double epicsShareAPI 
    epicsTimerQueueNonThreadedGetDelayToNextExpire (
        epicsTimerQueueNonThreadedId pQueue )
{
    return pQueue->getNextExpireDelay ();
}

extern "C" epicsTimerId epicsShareAPI epicsTimerQueueNonThreadedCreateTimer (
    epicsTimerQueueNonThreadedId pQueue,
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

extern "C" void  epicsShareAPI epicsTimerQueueNonThreadedShow (
    epicsTimerQueueNonThreadedId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" epicsTimerQueueThreadedId epicsShareAPI
    epicsTimerQueueThreadedCreate ( int okToShare, unsigned int threadPriority )
{
    try {
        epicsTimerQueueThreaded & queue = 
            epicsTimerQueueThreaded::allocate 
                ( okToShare ? true : false, threadPriority );
        return &queue;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsShareAPI epicsTimerQueueThreadedDelete ( epicsTimerQueueThreadedId pQueue )
{
    pQueue->release ();
}

extern "C" epicsTimerId epicsShareAPI epicsTimerQueueThreadedCreateTimer (
    epicsTimerQueueThreadedId pQueue, epicsTimerCallback pCallback, void *pArg )
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

extern "C" void  epicsShareAPI epicsTimerQueueThreadedShow (
    epicsTimerQueueThreadedId pQueue, unsigned int level )
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

