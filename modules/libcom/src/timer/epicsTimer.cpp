/*************************************************************************\
* Copyright (c) 2015 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <string>
#include <stdexcept>

#include "epicsMath.h"
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

template class tsFreeList < epicsTimerForC, 0x20 >;

epicsTimer::~epicsTimer () {}

epicsTimerQueueNotify::~epicsTimerQueueNotify () {}

epicsTimerNotify::~epicsTimerNotify  () {}

void epicsTimerNotify::show ( unsigned /* level */ ) const {}

epicsTimerForC::epicsTimerForC ( timerQueue &queue, epicsTimerCallback pCBIn, void *pPrivateIn ) :
    timer ( queue ), pCallBack ( pCBIn ), pPrivate ( pPrivateIn )
{
}

epicsTimerForC::~epicsTimerForC ()
{
}

void epicsTimerForC::destroy ()
{
    timerQueue & queueTmp = this->queue;
    this->~epicsTimerForC ();
    queueTmp.timerForCFreeList.release ( this );
}

epicsTimerNotify::expireStatus epicsTimerForC::expire ( const epicsTime & )
{
    ( *this->pCallBack ) ( this->pPrivate );
    return noRestart;
}

epicsTimerQueueActiveForC ::
    epicsTimerQueueActiveForC ( RefMgr & refMgr,
        bool okToShare, unsigned priority ) :
    timerQueueActive ( refMgr, okToShare, priority )
{
    timerQueueActive::start();
}

epicsTimerQueueActiveForC::~epicsTimerQueueActiveForC ()
{
}

void epicsTimerQueueActiveForC::release ()
{
    _refMgr->release ( *this );
}

epicsTimerQueuePassiveForC::epicsTimerQueuePassiveForC (
    epicsTimerQueueNotifyReschedule pRescheduleCallbackIn,
    epicsTimerQueueNotifyQuantum pSleepQuantumCallbackIn,
    void * pPrivateIn ) :
        timerQueuePassive ( * static_cast < epicsTimerQueueNotify * > ( this ) ),
        pRescheduleCallback ( pRescheduleCallbackIn ),
        pSleepQuantumCallback ( pSleepQuantumCallbackIn ),
        pPrivate ( pPrivateIn )
{
}

epicsTimerQueuePassiveForC::~epicsTimerQueuePassiveForC ()
{
}

void epicsTimerQueuePassiveForC::reschedule ()
{
    (*this->pRescheduleCallback) ( this->pPrivate );
}

double epicsTimerQueuePassiveForC::quantum ()
{
    return (*this->pSleepQuantumCallback) ( this->pPrivate );
}

void epicsTimerQueuePassiveForC::destroy ()
{
    delete this;
}

LIBCOM_API epicsTimerNotify::expireStatus::expireStatus ( restart_t restart ) : 
    delay ( - DBL_MAX )
{
    if ( restart != noRestart ) {
        throw std::logic_error
            ( "timer restart was requested without specifying a delay?" );
    }
}

LIBCOM_API epicsTimerNotify::expireStatus::expireStatus 
    ( restart_t restartIn, const double & expireDelaySec ) :
    delay ( expireDelaySec )
{
    if ( restartIn != epicsTimerNotify::restart ) {
        throw std::logic_error
            ( "no timer restart was requested, but a delay was specified?" );
    }
    if ( this->delay < 0.0 || !finite(this->delay) ) {
        throw std::logic_error
            ( "timer restart was requested, but a negative delay was specified?" );
    }
}

LIBCOM_API bool epicsTimerNotify::expireStatus::restart () const
{
    return this->delay >= 0.0 && finite(this->delay);
}

LIBCOM_API double epicsTimerNotify::expireStatus::expirationDelay () const
{
    if ( this->delay < 0.0 || !finite(this->delay) ) {
        throw std::logic_error
            ( "no timer restart was requested, but you are asking for a restart delay?" );
    }
    return this->delay;
}

extern "C" epicsTimerQueuePassiveId epicsStdCall
    epicsTimerQueuePassiveCreate (
        epicsTimerQueueNotifyReschedule pRescheduleCallbackIn,
        epicsTimerQueueNotifyQuantum pSleepQuantumCallbackIn,
        void * pPrivateIn )
{
    try {
        return new epicsTimerQueuePassiveForC (
            pRescheduleCallbackIn,
            pSleepQuantumCallbackIn,
            pPrivateIn );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsStdCall 
    epicsTimerQueuePassiveDestroy ( epicsTimerQueuePassiveId pQueue )
{
    pQueue->destroy ();
}

extern "C" double epicsStdCall 
    epicsTimerQueuePassiveProcess ( epicsTimerQueuePassiveId pQueue )
{
    try {
        return pQueue->process ( epicsTime::getCurrent() );
    }
    catch ( ... ) {
        return 1.0;
    }
}

extern "C" epicsTimerId epicsStdCall epicsTimerQueuePassiveCreateTimer (
    epicsTimerQueuePassiveId pQueue, epicsTimerCallback pCallback, void *pArg )
{
    try {
        return  & pQueue->createTimerForC ( pCallback, pArg );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" LIBCOM_API void epicsStdCall epicsTimerQueuePassiveDestroyTimer ( 
    epicsTimerQueuePassiveId /* pQueue */, epicsTimerId pTmr )
{
    pTmr->destroy ();
}

extern "C" void  epicsStdCall epicsTimerQueuePassiveShow (
    epicsTimerQueuePassiveId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" epicsTimerQueueId epicsStdCall
    epicsTimerQueueAllocate ( int okToShare, unsigned int threadPriority )
{
    try {
        epicsSingleton < timerQueueActiveMgr > :: reference ref =
            timerQueueMgrEPICS.getReference ();
        epicsTimerQueueActiveForC & tmr =
            ref->allocate ( ref, okToShare != 0, threadPriority );
        return &tmr;
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void epicsStdCall epicsTimerQueueRelease ( epicsTimerQueueId pQueue )
{
    pQueue->release ();
}

extern "C" epicsTimerId epicsStdCall epicsTimerQueueCreateTimer (
    epicsTimerQueueId pQueue, epicsTimerCallback pCallback, void *pArg )
{
    try {
        return & pQueue->createTimerForC ( pCallback, pArg );
    }
    catch ( ... ) {
        return 0;
    }
}

extern "C" void  epicsStdCall epicsTimerQueueShow (
    epicsTimerQueueId pQueue, unsigned int level )
{
    pQueue->show ( level );
}

extern "C" void epicsStdCall epicsTimerQueueDestroyTimer ( 
    epicsTimerQueueId /* pQueue */, epicsTimerId pTmr )
{
    pTmr->destroy ();
}

extern "C" void  epicsStdCall epicsTimerStartTime (
    epicsTimerId pTmr, const epicsTimeStamp *pTime )
{
    pTmr->start ( *pTmr, *pTime );
}

extern "C" void  epicsStdCall epicsTimerStartDelay (
    epicsTimerId pTmr, double delaySeconds )
{
    pTmr->start ( *pTmr, delaySeconds );
}

extern "C" void  epicsStdCall epicsTimerCancel ( epicsTimerId pTmr )
{
    pTmr->cancel ();
}

extern "C" double  epicsStdCall epicsTimerGetExpireDelay ( epicsTimerId pTmr )
{
    return pTmr->getExpireDelay ();
}

extern "C" void  epicsStdCall epicsTimerShow (
    epicsTimerId pTmr, unsigned int level )
{
    pTmr->timer::show ( level );
}

