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


#ifndef casCoreClientIL_h
#define casCoreClientIL_h

#include "caServerIIL.h" // caServerI in line func 
#include "casCtxIL.h" // casEventSys in line func 


inline void casCoreClient::lock ()
{
    this->mutex.lock ();
}

inline void casCoreClient::unlock ()
{
    this->mutex.unlock ();
}

//
// casCoreClient::getCAS()
//
inline caServerI &casCoreClient::getCAS() const
{
	return *this->ctx.getServer();
}

//
// casCoreClient::installAsyncIO()
//
inline void casCoreClient::installAsyncIO(casAsyncIOI &ioIn)
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	this->ioInProgList.add ( ioIn );
}

//
// casCoreClient::removeAsyncIO()
//
inline void casCoreClient::removeAsyncIO(casAsyncIOI &ioIn)
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	this->ioInProgList.remove ( ioIn );
	this->ctx.getServer()->ioBlockedList::signal ();
}

inline bool casCoreClient::okToStartAsynchIO ()
{
    if ( ! this->asyncIOFlag ) {
        this->asyncIOFlag = true;
        return true;
    }
    return false;
}

inline casMonEvent & casCoreClient::casMonEventFactory ( casMonitor & monitor, 
        const smartConstGDDPointer & pNewValue )
{
    return this->ctx.getServer()->casMonEventFactory ( monitor, pNewValue );
}

inline void casCoreClient::casMonEventDestroy ( casMonEvent & event )
{
    this->ctx.getServer()->casMonEventDestroy ( event );
}

inline casEventSys::processStatus casCoreClient::eventSysProcess ()
{
	return this->eventSys.process ();
}

inline void casCoreClient::addToEventQueue ( casEvent & ev )
{
	this->eventSys.addToEventQueue ( ev );
}

inline void casCoreClient::insertEventQueue ( casEvent & insert, casEvent & prevEvent )
{
	this->eventSys.insertEventQueue ( insert, prevEvent );
}

inline void casCoreClient::removeFromEventQueue ( casEvent &  ev )
{
    this->eventSys.removeFromEventQueue ( ev );
}

inline void casCoreClient::enableEvents ()
{
    this->eventSys.eventsOn ();
    this->eventSignal (); // wake up the event queue consumer
}

inline void casCoreClient::disableEvents ()
{
    this->eventSys.eventsOff ();
}

inline void casCoreClient::setDestroyPending ()
{
    this->eventSys.setDestroyPending ();
}

inline bool casCoreClient::eventSysIsFull ()
{
    return this->eventSys.full ();
}

inline casMonitor * casCoreClient::lookupMonitor ( const caResId & idIn )
{
    return this->ctx.getServer()->lookupMonitor ( idIn );
}

#endif // casCoreClientIL_h

