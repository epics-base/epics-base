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


#ifndef caServerIIL_h
#define caServerIIL_h

#ifdef epicsExportSharedSymbols
#define caServerIIL_h_epicsExportSharedSymbols
#undef epicsExportSharedSymbols
#endif

#include "epicsGuard.h"

#ifdef caServerIIL_h_epicsExportSharedSymbols
#define epicsExportSharedSymbols
#endif

//
// caServerI::getAdapter()
//
inline caServer *caServerI::getAdapter()
{
	return &this->adapter;
}

//
// call virtual function in the interface class
//
inline caServer * caServerI::operator -> ()
{
	return this->getAdapter();
}

//
// caServerI::installItem()
//
inline void caServerI::installItem(casRes &res)
{
    this->chronIntIdResTable<casRes>::add(res);
}

//
// caServerI::removeItem()
//
inline casRes *caServerI::removeItem(casRes &res)
{
	return this->chronIntIdResTable<casRes>::remove(res);
}

//
// caServerI::setDebugLevel()
//
inline void caServerI::setDebugLevel(unsigned debugLevelIn)
{
	this->debugLevel = debugLevelIn;
}

//
// casEventMask caServerI::valueEventMask()
//
inline casEventMask caServerI::valueEventMask() const
{
    return this->valueEvent;
}

//
// caServerI::logEventMask()
//
inline casEventMask caServerI::logEventMask() const
{
    return this->logEvent;
}

//
// caServerI::alarmEventMask()
//
inline casEventMask caServerI::alarmEventMask() const
{
    return this->alarmEvent;
}

//
// caServerI::subscriptionEventsProcessedCounter () const
//
inline unsigned caServerI::subscriptionEventsProcessed () const
{
    return this->nEventsProcessed;
}

//
// caServerI::incrEventsProcessedCounter ()
//
inline void caServerI::incrEventsProcessedCounter ()
{
    this->nEventsProcessed++;
}

//
// caServerI::subscriptionEventsPosted () const
//
inline unsigned caServerI::subscriptionEventsPosted () const
{
    return this->nEventsPosted;
}

//
// caServerI::incEventsPostedCounter ()
//
inline void caServerI::incrEventsPostedCounter ()
{
    this->nEventsPosted++;
}

inline void caServerI::lock () const
{
    this->mutex.lock ();
}

inline void caServerI::unlock () const
{
    this->mutex.unlock ();
}

inline casMonEvent & caServerI::casMonEventFactory ( casMonitor & monitor, 
            const gdd & pNewValue )
{
    return * new ( this->casMonEventFreeList ) casMonEvent ( monitor, pNewValue );
}

inline void caServerI::casMonEventDestroy ( casMonEvent & monEvent )
{
    monEvent.~casMonEvent ();
    this->casMonEventFreeList.release ( & monEvent );
}

inline casMonitor & caServerI::casMonitorFactory ( 
    casChannelI & chan, caResId clientId, 
    const unsigned long count, const unsigned type, 
    const casEventMask & mask, 
    casMonitorCallbackInterface & cb )
{
    casMonitor * pMon = 
        new ( this->casMonitorFreeList ) casMonitor 
            ( clientId, chan, count, type, mask, cb );
	this->installItem ( *pMon );
    return *pMon;
}

inline void caServerI::casMonitorDestroy ( casMonitor & cm )
{
	casRes * pRes = this->removeItem ( cm );
	assert ( & cm == ( casMonitor * ) pRes );
    cm.~casMonitor ();
    this->casMonitorFreeList.release ( & cm );
}

#endif // caServerIIL_h

