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

#include <typeinfo>

#define epicsExportSharedSymbols
#include "epicsTimer.h"
#if 0
#include "tsFreeList.h"
#include "tsDLList.h"
#include "epicsMutex.h"
#include "locationException.h"
#include "errlog.h"
#endif

epicsTimerQueueNonThreaded::~epicsTimerQueueNonThreaded () {}
void epicsTimerNotify::show ( unsigned /* level */ ) const {}

#if 0
class epicsTimerForC : public epicsTimer {
    epicsShareFunc virtual void expire ();
    epicsShareFunc virtual void destroy ();
    epicsShareFunc virtual bool again () const;
    epicsShareFunc virtual double delay () const;
    epicsShareFunc virtual void show ( unsigned level ) const;
    const epicsTimerJumpTable &jt;
    void * pPrivate;
public:
    epicsTimerForC ( const epicsTimerJumpTable &jtIn, epicsTimerQueue &queue, void *pPrivateIn );
};

epicsTimerForC::epicsTimerForC ( const epicsTimerJumpTable &jtIn, epicsTimerQueue &queue, void *pPrivateIn ) :
    epicsTimer ( queue ), jt ( jtIn ), pPrivate ( pPrivateIn ) {}

void epicsTimerForC::expire ()
{
    ( *this->jt.expire ) ( this->pPrivate );
}

void epicsTimerForC::destroy ()
{
    ( *this->jt.destroy ) ( this->pPrivate );
}

bool epicsTimerForC::again () const
{
    return ( *this->jt.again ) ( this->pPrivate ) ? true : false ;
}

double epicsTimerForC::delay () const 
{
    return ( *this->jt.delay ) ( this->pPrivate );
}

void epicsTimerForC::show ( unsigned level ) const 
{
    ( *this->jt.show ) ( this->pPrivate, level );
}

extern "C" epicsShareFunc epicsTimerId epicsShareAPI epicsTimerCreate (
    const epicsTimerJumpTable *pjtIn, epicsTimerQueueId queueIdIn, void *pPrivateIn )
{
    assert ( pjtIn );
    assert ( queueIdIn );
    return ( epicsTimerId ) new epicsTimerForC 
        ( *pjtIn, *static_cast < epicsTimerQueue * > ( queueIdIn ), pPrivateIn );
}

extern "C" epicsShareFunc void epicsShareAPI epicsTimerArm  ( epicsTimerId tmrIdIn, double delay )
{
    epicsTimerForC *pTmr = static_cast < epicsTimerForC * > ( tmrIdIn );
    assert ( pTmr );
    pTmr->reschedule ( delay );
}

extern "C" epicsShareFunc void epicsShareAPI epicsTimerCancel ( epicsTimerId tmrIdIn )
{
    epicsTimerForC *pTmr = static_cast < epicsTimerForC * > ( tmrIdIn );
    assert ( pTmr );
    pTmr->cancel ();
}

extern "C" epicsShareFunc double epicsShareAPI epicsTimerTimeRemaining ( epicsTimerId idIn )
{
    epicsTimerForC *pTmr = static_cast < epicsTimerForC * > ( idIn );
    assert ( pTmr );
    return pTmr->timeRemaining ();
}

extern "C" epicsShareFunc epicsTimeStamp epicsShareAPI epicsTimerExpirationDate ( epicsTimerId idIn )
{
    epicsTimerForC *pTmr = static_cast < epicsTimerForC * > ( idIn );
    assert ( pTmr );
    return pTmr->expirationDate ();
}

#endif