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
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <stdexcept>

#define epicsExportSharedSymbols
#include "epicsTimer.h"
#include "epicsGuard.h"
#include "timerPrivate.h"

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif

epicsTimer::~epicsTimer () {}

epicsTimerQueueNotify::~epicsTimerQueueNotify () {}

epicsTimerNotify::~epicsTimerNotify  () {}

void epicsTimerNotify::show ( unsigned /* level */ ) const {}

epicsTimerForC::epicsTimerForC ( timerQueue &queue, epicsTimerCallback pCBIn, void *pPrivateIn ) epics_throws (()) :
    timer ( queue ), pCallBack ( pCBIn ), pPrivate ( pPrivateIn )
{
}

epicsTimerForC::~epicsTimerForC ()
{
}

void epicsTimerForC::destroy ()
{
    timerQueue & queueTmp ( this->queue );
    this->~epicsTimerForC ();
    epicsTimerForC::operator delete ( this, queueTmp.timerForCFreeList );
}

epicsTimerNotify::expireStatus epicsTimerForC::expire ( const epicsTime & )
{
    ( *this->pCallBack ) ( this->pPrivate );
    return noRestart;
}

epicsTimerQueueActiveForC::epicsTimerQueueActiveForC ( bool okToShare, unsigned priority ) :
    timerQueueActive ( okToShare, priority )
{
}

epicsTimerQueueActiveForC::~epicsTimerQueueActiveForC ()
{
}

void epicsTimerQueueActiveForC::release () epics_throws (())
{
    epicsSingleton < timerQueueActiveMgr >::reference pMgr = 
        timerQueueMgrEPICS;
    pMgr->release ( *this );
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

void epicsTimerQueuePassiveForC::destroy ()
{
    delete this;
}

epicsShareFunc epicsTimerNotify::expireStatus::expireStatus ( restart_t restart ) : 
    delay ( - DBL_MAX )
{
    if ( restart != noRestart ) {
        throw std::logic_error 
            ( "timer restart was requested without specifying a delay?" );
    }
}

epicsShareFunc epicsTimerNotify::expireStatus::expireStatus 
    ( restart_t restartIn, const double & expireDelaySec ) :
    delay ( expireDelaySec )
{
    if ( restartIn != epicsTimerNotify::restart ) {
        throw std::logic_error 
            ( "no timer restart was requested, but a delay was specified?" );
    }
    if ( this->delay < 0.0 ) {
        throw std::logic_error 
            ( "timer restart was requested, but a negative delay was specified?" );
    }
}

epicsShareFunc bool epicsTimerNotify::expireStatus::restart () const
{
    return this->delay >= 0.0;
}

epicsShareFunc double epicsTimerNotify::expireStatus::expirationDelay () const
{
    if ( this->delay < 0.0 ) {
        throw std::logic_error 
            ( "no timer restart was requested, but you are asking for a restart delay?" );
    }
    return this->delay;
}

extern "C" epicsTimerQueuePassiveId epicsShareAPI
    epicsTimerQueuePassiveCreate ( epicsTimerQueueRescheduleCallback pCallbackIn, void *pPrivateIn )
{
    try {
        return  new epicsTimerQueuePassiveForC ( pCallbackIn, pPrivateIn );
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

extern "C" double epicsShareAPI 
    epicsTimerQueuePassiveProcess ( epicsTimerQueuePassiveId pQueue )
{
    return pQueue->process ( epicsTime::getCurrent() );
}

extern "C" epicsTimerId epicsShareAPI epicsTimerQueuePassiveCreateTimer (
    epicsTimerQueuePassiveId pQueue, epicsTimerCallback pCallback, void *pArg )
{
    try {
        return  & pQueue->createTimerForC ( pCallback, pArg );
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
    epicsTimerQueueAllocate ( int okToShare, unsigned int threadPriority )
{
    try {
        epicsSingleton < timerQueueActiveMgr >::reference ref = timerQueueMgrEPICS;
        epicsTimerQueueActiveForC & tmr = 
            ref->allocate ( okToShare ? true : false, threadPriority );
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
        return & pQueue->createTimerForC ( pCallback, pArg );
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
    pTmr->start ( *pTmr, *pTime );
}

extern "C" void  epicsShareAPI epicsTimerStartDelay (
    epicsTimerId pTmr, double delaySeconds )
{
    pTmr->start ( *pTmr, delaySeconds );
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

