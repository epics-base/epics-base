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
#include "timerPrivate.h"

struct epicsTimerForC : public epicsTimerNotify, public timer {
public:
    epicsTimerForC ( timerQueue &, epicsTimerCallback, void *pPrivateIn );
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~epicsTimerForC ();
private:
    epicsTimerCallback pCallBack;
    void * pPrivate;
    expireStatus expire ();
    static tsFreeList < epicsTimerForC > freeList;
};

struct epicsTimerQueuePassiveForC : public epicsTimerQueueNotify, public timerQueuePassive {
public:
    epicsTimerQueuePassiveForC ( epicsTimerQueueRescheduleCallback pCallback, void *pPrivate );
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~epicsTimerQueuePassiveForC ();
private:
    epicsTimerQueueRescheduleCallback pCallback;
    void *pPrivate;
    void reschedule ();
    static tsFreeList < epicsTimerQueuePassiveForC > freeList;
};

void epicsTimerNotify::show ( unsigned /* level */ ) const {}

tsFreeList < epicsTimerForC > epicsTimerForC::freeList;

epicsTimerForC::epicsTimerForC ( timerQueue &queue, epicsTimerCallback pCBIn, void *pPrivateIn ) :
    timer ( *this, queue ), pCallBack ( pCBIn ), pPrivate ( pPrivateIn )
{
}

epicsTimerForC::~epicsTimerForC () 
{
}

inline void epicsTimerForC::destroy ()
{
    delete this;
}

inline void * epicsTimerForC::operator new ( size_t size )
{ 
    return epicsTimerForC::freeList.allocate ( size );
}

inline void epicsTimerForC::operator delete ( void *pCadaver, size_t size )
{ 
    epicsTimerForC::freeList.release ( pCadaver, size );
}

epicsTimerNotify::expireStatus epicsTimerForC::expire ()
{
    ( *this->pCallBack ) ( this->pPrivate );
    return noRestart;
}

tsFreeList < epicsTimerQueueForC > epicsTimerQueueForC::freeList;

epicsTimerQueueForC::epicsTimerQueueForC ( bool okToShare, unsigned priority ) :
    timerQueueActive ( okToShare, priority )
{
}

epicsTimerQueueForC::~epicsTimerQueueForC ()
{
}

void epicsTimerQueueForC::release ()
{
    queueMgr.release ( *this );
}

tsFreeList < epicsTimerQueuePassiveForC > epicsTimerQueuePassiveForC::freeList;

inline void * epicsTimerQueuePassiveForC::operator new ( size_t size )
{ 
    return epicsTimerQueuePassiveForC::freeList.allocate ( size );
}

inline void epicsTimerQueuePassiveForC::operator delete ( void *pCadaver, size_t size )
{ 
    epicsTimerQueuePassiveForC::freeList.release ( pCadaver, size );
}

epicsTimerQueuePassiveForC::epicsTimerQueuePassiveForC 
    ( epicsTimerQueueRescheduleCallback pCallbackIn, void * pPrivateIn ) :
        timerQueuePassive ( * static_cast < epicsTimerQueueNotify * > ( this ) ), 
            pCallback ( pCallbackIn ), pPrivate ( pPrivateIn ) 
{
}

epicsTimerQueuePassiveForC::~epicsTimerQueuePassiveForC () 
{
}

void epicsTimerQueuePassiveForC::reschedule ()
{
    (*this->pCallback) ( this->pPrivate );
}

inline void epicsTimerQueuePassiveForC::destroy ()
{
    delete this;
}

extern "C" epicsTimerQueuePassiveId epicsShareAPI
    epicsTimerQueuePassiveCreate ( epicsTimerQueueRescheduleCallback pCallbackIn, void *pPrivateIn )
{
    try {
        return new epicsTimerQueuePassiveForC ( pCallbackIn, pPrivateIn );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsShareAPI 
    epicsTimerQueuePassiveDestroy ( epicsTimerQueuePassiveId pQueue )
{
    pQueue->destroy ();
}

extern "C" void epicsShareAPI 
    epicsTimerQueuePassiveProcess ( epicsTimerQueuePassiveId pQueue )
{
    pQueue->process ();
}

extern "C" double epicsShareAPI 
    epicsTimerQueuePassiveGetDelayToNextExpire (
        epicsTimerQueuePassiveId pQueue )
{
    return pQueue->getNextExpireDelay ();
}

extern "C" epicsTimerId epicsShareAPI epicsTimerQueuePassiveCreateTimer (
    epicsTimerQueuePassiveId pQueue, epicsTimerCallback pCallback, void *pArg )
{
    try {
        return new epicsTimerForC ( pQueue->getTimerQueue (), pCallback, pArg );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void  epicsShareAPI epicsTimerQueuePassiveShow (
    epicsTimerQueuePassiveId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" epicsTimerQueueId epicsShareAPI
    epicsTimerQueueCreate ( int okToShare, unsigned int threadPriority )
{
    try {
        epicsTimerQueueForC &tmr = queueMgr.allocate ( okToShare ? true : false, threadPriority );
        return &tmr;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsShareAPI epicsTimerQueueDelete ( epicsTimerQueueId pQueue )
{
    pQueue->release ();
}

extern "C" epicsTimerId epicsShareAPI epicsTimerQueueCreateTimer (
    epicsTimerQueueId pQueue, epicsTimerCallback pCallback, void *pArg )
{
    try {
        return new epicsTimerForC ( pQueue->getTimerQueue (), pCallback, pArg );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void  epicsShareAPI epicsTimerQueueShow (
    epicsTimerQueueId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" void  epicsShareAPI epicsTimerDestroy ( epicsTimerId pTmr )
{
    pTmr->destroy ();
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
    pTmr->timer::show ( level );
}

