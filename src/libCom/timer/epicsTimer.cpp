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
#include "epicsGuard.h"
#include "timerPrivate.h"

epicsSingleton < tsFreeList < epicsTimerForC, 0x20 > > epicsTimerForC::pFreeList;
epicsSingleton < tsFreeList < epicsTimerQueuePassiveForC, 0x10 > > epicsTimerQueuePassiveForC::pFreeList;
epicsSingleton < tsFreeList < epicsTimerQueueActiveForC, 0x10 > > epicsTimerQueueActiveForC::pFreeList;

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
    delete this;
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

void epicsTimerQueueActiveForC::release ()
{
    pTimerQueueMgrEPICS->release ( *this );
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
        epicsTimerQueueActiveForC &tmr = 
            pTimerQueueMgrEPICS->allocate ( okToShare ? true : false, threadPriority );
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

