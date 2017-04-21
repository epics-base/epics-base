/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
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
		printf ( "\tthere are %d items in the event queue\n",
			this->eventLogQue.count() );
		printf ( "\tthere are %d items in the io queue\n",
			this->ioQue.count() );
		printf ( "Replace events flag = %d, dontProcessSubscr flag = %d\n",
			static_cast < int > ( this->replaceEvents ), 
            static_cast < int > ( this->dontProcessSubscr ) );
	}
}

casEventSys::~casEventSys()
{
	if ( this->pPurgeEvent != NULL ) {
		this->eventLogQue.remove ( *this->pPurgeEvent );
		delete this->pPurgeEvent;
	}

    // at this point:
    // o all channels have been deleted
    // o all IO has been deleted
    // o any subscription events remaining on the queue
    //   are pending destroy

    // verify above assertion is true
    casVerify ( this->eventLogQue.count() == 0 );
    casVerify ( this->ioQue.count() == 0 );

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

casProcCond casEventSys :: process (
    epicsGuard < casClientMutex > & casClientGuard )
{
	casProcCond cond = casProcOk;

    epicsGuard < evSysMutex > evGuard ( this->mutex );

    // we need two queues, one for io and one for subscriptions,
    // so that we dont hang up the server when in an IO postponed
    // state simultaneouly with a flow control active state
	while ( true ) {
		casEvent * pEvent = this->ioQue.get ();
		if ( pEvent == NULL ) {
			break;
		}

        caStatus status = pEvent->cbFunc ( 
                this->client, casClientGuard, evGuard );
		if ( status == S_cas_success ) {
            cond = casProcOk;
		}
		else if ( status == S_cas_sendBlocked ) {
			// not accepted so return to the head of the list
		    // (we will try again later)
	        this->ioQue.push ( *pEvent );
			cond = casProcOk;
			break;
		}
		else if ( status == S_cas_disconnect ) {
			cond = casProcDisconnect;
			break;
		}
		else {
			errMessage ( status, 
                "- unexpected error, processing io queue" );
			cond = casProcDisconnect;
			break;
		}
  	}

    if ( cond == casProcOk ) {
	    while ( ! this->dontProcessSubscr ) {
		    casEvent * pEvent = this->eventLogQue.get ();
		    if ( pEvent == NULL ) {
			    break;
		    }

            caStatus status = pEvent->cbFunc ( 
                    this->client, casClientGuard, evGuard );
		    if ( status == S_cas_success ) {
                cond = casProcOk;
		    }
		    else if ( status == S_cas_sendBlocked ) {
			    // not accepted so return to the head of the list
		        // (we will try again later)
	            this->eventLogQue.push ( *pEvent );
			    cond = casProcOk;
			    break;
		    }
		    else if ( status == S_cas_disconnect ) {
			    cond = casProcDisconnect;
			    break;
		    }
		    else {
			    errMessage ( status, 
                    "- unexpected error, processing event queue" );
			    cond = casProcDisconnect;
			    break;
		    }
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
		cond = casProcDisconnect;
	}

	return cond;
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
	this->dontProcessSubscr = false;

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
			    this->dontProcessSubscr = true;
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
	this->evSys.dontProcessSubscr = true;
	this->evSys.pPurgeEvent = NULL;
	delete this;
	return S_cas_success;
}

caStatus casEventSys::addToEventQueue ( casAsyncIOI & event, 
    bool & onTheQueue, bool & posted, bool & wakeupNeeded )
{
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
	    // dont allow them to post completion more than once
        if ( posted || onTheQueue ) {
            wakeupNeeded = false;
            return S_cas_redundantPost;
        }
        posted = true;
        onTheQueue = true;
        wakeupNeeded = 
            ( this->dontProcessSubscr || this->eventLogQue.count() == 0 ) &&
            this->ioQue.count() == 0;
	    this->ioQue.add ( event );
    }
    return S_cas_success;
}

void casEventSys::removeFromEventQueue ( casAsyncIOI &  io, bool & onTheIOQueue )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( onTheIOQueue ) {
        onTheIOQueue = false;
	    this->ioQue.remove ( io );
    }
}

bool casEventSys::addToEventQueue ( casChannelI & event, 
    bool & onTheIOQueue )
{
    bool wakeupNeeded = false;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
	    if ( ! onTheIOQueue ) {
		    onTheIOQueue = true;
            wakeupNeeded = 
                ( this->dontProcessSubscr || this->eventLogQue.count() == 0 ) &&
                this->ioQue.count() == 0;
	        this->ioQue.add ( event );
	    }
    }
    return wakeupNeeded;
}

void casEventSys::removeFromEventQueue ( class casChannelI & io, 
    bool & onTheIOQueue )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	if ( onTheIOQueue ) {
		onTheIOQueue = false;
	    this->ioQue.remove ( io );
	}
}

bool casEventSys::addToEventQueue ( channelDestroyEvent & event )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    bool wakeupRequired = 
                ( this->dontProcessSubscr || this->eventLogQue.count() == 0 ) &&
                this->ioQue.count() == 0;
	this->ioQue.add ( event );
    return wakeupRequired;
}

void casEventSys::setDestroyPending ()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	this->destroyPending = true;
}

inline bool casEventSys::full () const
{
	return this->replaceEvents || 
            this->eventLogQue.count() >= this->maxLogEntries;
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
            	
                signalNeeded |= 
                    !this->dontProcessSubscr && 
                    this->eventLogQue.count() == 0 &&
                    this->ioQue.count() == 0;

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
