/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <string.h>

#include "errlog.h"

#define epicsExportSharedSymbols
#include "caHdrLargeArray.h"
#include "casCoreClient.h"
#include "casAsyncIOI.h"
#include "casChannelI.h"
#include "channelDestroyEvent.h"

void casEventSys::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	printf ( "casEventSys at %p\n", 
        static_cast <const void *> ( this ) );
	if (level>=1u) {
		printf ( "\numSubscriptions = %u, maxLogEntries = %u\n",
			this->numSubscriptions, this->maxLogEntries );	
		printf ( "\tthere are %d events in the queue\n",
			this->eventLogQue.count() );
		printf ( "Replace events flag = %d, dontProcess flag = %d\n",
			static_cast < int > ( this->replaceEvents ), 
            static_cast < int > ( this->dontProcess ) );
	}
}

casEventSys::~casEventSys()
{
	if ( this->pPurgeEvent != NULL ) {
		this->eventLogQue.remove ( *this->pPurgeEvent );
		delete this->pPurgeEvent;
	}

    // at this point:
    // o all channels delete
    // o all IO deleted
    // o any subscription events remaining on the queue
    //   are pending destroy

    // verify above assertion is true
    casVerify ( this->eventLogQue.count() == 0 );

	// all active subscriptions should also have been 
    // uninstalled
	casVerify ( this->numSubscriptions == 0 );
    if ( this->numSubscriptions != 0 ) {
        printf ( "numSubscriptions=%u\n", this->numSubscriptions );
    }
}

void casEventSys::installMonitor ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	assert ( this->numSubscriptions < UINT_MAX );
	this->numSubscriptions++;
	this->maxLogEntries += averageEventEntries;
}

void casEventSys::removeMonitor () 
{       
    epicsGuard < epicsMutex > guard ( this->mutex );
	assert ( this->numSubscriptions >= 1u );
	this->numSubscriptions--;
	this->maxLogEntries -= averageEventEntries;
}

casEventSys::processStatus casEventSys::process (
    epicsGuard < casClientMutex > & casClientGuard )
{
    casEventSys::processStatus ps;
	ps.cond = casProcOk;
	ps.nAccepted = 0u;

    epicsGuard < evSysMutex > evGuard ( this->mutex );

	while ( ! this->dontProcess ) {
        casEvent * pEvent;

		pEvent = this->eventLogQue.get ();

		if ( pEvent == NULL ) {
			break;
		}

        caStatus status = pEvent->cbFunc ( 
                this->client, casClientGuard, evGuard );
		if ( status == S_cas_success ) {
			ps.nAccepted++;
		}
		else if ( status == S_cas_sendBlocked ) {
			// not accepted so return to the head of the list
		    // (we will try again later)
	        this->eventLogQue.push ( *pEvent );
			ps.cond = casProcOk;
			break;
		}
		else if ( status == S_cas_disconnect ) {
			ps.cond = casProcDisconnect;
			break;
		}
		else {
			errMessage ( status, 
                "- unexpected error processing event" );
			ps.cond = casProcDisconnect;
			break;
		}
  	}

	//
	// allows the derived class to be informed that it
	// needs to delete itself via the event system
	//
	// this gets the server out of nasty situations
	// where the client needs to be deleted but
	// the caller may be using the client's "this"
	// pointer.
	//
	if ( this->destroyPending ) {
		ps.cond = casProcDisconnect;
	}

	return ps;
}

void casEventSys::eventsOn ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );

	//
	// allow multiple events for each monitor
	//
	this->replaceEvents = false;

	//
	// allow the event queue to be processed
	//
	this->dontProcess = false;

	//
	// remove purge event if it is still pending
	//
	if ( this->pPurgeEvent != NULL ) {
		this->eventLogQue.remove ( *this->pPurgeEvent );
		delete this->pPurgeEvent;
		this->pPurgeEvent = NULL;
	}
}

bool casEventSys::eventsOff ()
{
    bool signalNeeded = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );

	    //
	    // new events will replace the last event on
	    // the queue for a particular monitor
	    //
	    this->replaceEvents = true;

	    //
	    // suppress the processing and sending of events
	    // only after we have purged the event queue
	    // for this particular client
	    //
	    if ( this->pPurgeEvent == NULL ) {
		    this->pPurgeEvent = new casEventPurgeEv ( *this );
		    if ( this->pPurgeEvent == NULL ) {
			    //
			    // if there is no room for the event then immediately
			    // stop processing and sending events to the client
			    // until we exit flow control
			    //
			    this->dontProcess = true;
		    }
		    else {
                if ( this->eventLogQue.count() == 0 ) {
                    signalNeeded = true;
                }
	            this->eventLogQue.add ( *this->pPurgeEvent );
		    }
	    }
    }
    return signalNeeded;
}

casEventPurgeEv::casEventPurgeEv ( casEventSys & evSysIn ) :
    evSys ( evSysIn )
{
}

caStatus casEventPurgeEv::cbFunc ( 
    casCoreClient &, 
    epicsGuard < casClientMutex > &,
    epicsGuard < evSysMutex > & )
{
	this->evSys.dontProcess = true;
	this->evSys.pPurgeEvent = NULL;
	delete this;
	return S_cas_success;
}

caStatus casEventSys::addToEventQueue ( casAsyncIOI & event, 
    bool & onTheQueue, bool & posted, bool & wakeupNeeded )
{
    wakeupNeeded = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
	    // dont allow them to post completion more than once
        if ( posted || onTheQueue ) {
            return S_cas_redundantPost;
        }
        posted = true;
        onTheQueue = true;
        wakeupNeeded = ! this->dontProcess && this->eventLogQue.count() == 0;
	    this->eventLogQue.add ( event );
    }
    return S_cas_success;
}

void casEventSys::removeFromEventQueue ( casAsyncIOI &  io, bool & onTheEventQueue )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( onTheEventQueue ) {
        onTheEventQueue = false;
	    this->eventLogQue.remove ( io );
    }
}

bool casEventSys::addToEventQueue ( casChannelI & event, 
    bool & inTheEventQueue )
{
    bool wakeupRequired = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
	    if ( ! inTheEventQueue ) {
		    inTheEventQueue = true;
            wakeupRequired = ! this->dontProcess && this->eventLogQue.count()==0;
	        this->eventLogQue.add ( event );
	    }
    }
    return wakeupRequired;
}

void casEventSys::removeFromEventQueue ( class casChannelI & io, 
    bool & inTheEventQueue )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	if ( inTheEventQueue ) {
		inTheEventQueue = false;
	    this->eventLogQue.remove ( io );
	}
}

bool casEventSys::addToEventQueue ( channelDestroyEvent & event )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    bool wakeupRequired = ! this->dontProcess && this->eventLogQue.count()==0;
	this->eventLogQue.add ( event );
    return wakeupRequired;
}

void casEventSys::setDestroyPending ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	this->destroyPending = true;
}

inline bool casEventSys::full () const // X aCC 361
{
	if ( this->replaceEvents || this->eventLogQue.count() >= this->maxLogEntries ) {
		return true;
	}
	else {
		return false;
	}
}

bool casEventSys::postEvent ( tsDLList < casMonitor > & monitorList, 
    const casEventMask & select, const gdd & event )
{
    bool signalNeeded = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        tsDLIter < casMonitor > iter = monitorList.firstIter ();
        while ( iter.valid () ) {
            if ( iter->selected ( select ) ) {
	            // get a new block if we havent exceeded quotas
	            bool full = ( iter->numEventsQueued() >= individualEventEntries ) 
                            || this->full ();
	            casMonEvent * pLog;
	            if ( ! full ) {
                    // should I get rid of this try block by implementing a no 
                    // throw version of the free list alloc? However, crude
                    // tests on windows with ms visual C++ dont appear to argue
                    // against the try block.
                    try {
                        pLog = new ( this->casMonEventFreeList ) 
                            casMonEvent ( *iter, event );
                    }
                    catch ( ... ) {
                        pLog = 0;
                    }
	            }
	            else {
		            pLog = 0;
	            }
            	
                if ( this->eventLogQue.count() == 0 ) {
                    signalNeeded = true;
                }
                iter->installNewEventLog ( 
                    this->eventLogQue, pLog, event );
            }
	        ++iter;
        }
    }
    return signalNeeded;
}

void casEventSys::casMonEventDestroy ( 
    casMonEvent & ev, epicsGuard < evSysMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    ev.~casMonEvent ();
    this->casMonEventFreeList.release ( & ev );
}

void casEventSys::prepareMonitorForDestroy ( casMonitor & mon )
{
    bool safeToDestroy = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        mon.markDestroyPending ();
        // if events reference it on the queue then it gets 
        // deleted when it reaches the top of the queue
        if ( mon.numEventsQueued () == 0 ) {
            safeToDestroy = true;
        }
    }
    if ( safeToDestroy ) {
        this->client.destroyMonitor ( mon );
    }
}
