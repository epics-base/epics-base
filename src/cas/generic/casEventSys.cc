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

/*
 * ANSI C
 */
#include <string.h>

/*
 * EPICS
 */
#include "server.h"
#include "casEventSysIL.h" // casMonitor inline func

//
// casEventSys::show()
//
void casEventSys::show(unsigned level) const
{
	printf ( "casEventSys at %p\n", 
        static_cast <const void *> ( this ) );
	if (level>=1u) {
		printf ("\tnumEventBlocks = %u, maxLogEntries = %u\n",
			this->numEventBlocks, this->maxLogEntries);	
		printf ("\tthere are %d events in the queue\n",
			this->eventLogQue.count());
		printf ("Replace events flag = %d, dontProcess flag = %d\n",
			this->replaceEvents, this->dontProcess);
	}
}

//
// casEventSys::~casEventSys()
//
casEventSys::~casEventSys()
{
	casEvent 	*pE;
	
	this->mutex.lock();

	if (this->pPurgeEvent != NULL) {
		this->eventLogQue.remove(*this->pPurgeEvent);
		delete this->pPurgeEvent;
	}

	/*
	 * all active event blocks must be canceled first
	 */
	casVerify (this->numEventBlocks==0);

	while ( (pE = this->eventLogQue.get()) ) {
		delete pE;
	}

	this->mutex.unlock();
}

//
// casEventSys::installMonitor()
//
void casEventSys::installMonitor()
{
	this->mutex.lock();
	this->numEventBlocks++;
	this->maxLogEntries += averageEventEntries;
	this->mutex.unlock();
}

//
// casEventSys::removeMonitor()
//
void casEventSys::removeMonitor() 
{       
	this->mutex.lock();
	assert (this->numEventBlocks>=1u);
	this->numEventBlocks--;
	this->maxLogEntries -= averageEventEntries;
	this->mutex.unlock();
}

//
// casEventSys::process()
//
casProcCond casEventSys::process()
{
	casEvent	*pEvent;
	caStatus	status;
	casProcCond	cond = casProcOk;
	unsigned long	nAccepted = 0u;

	this->mutex.lock();

	while (!this->dontProcess) {

		pEvent = this->eventLogQue.get();
		if (pEvent==NULL) {
			break;
		}

		//
		// lock must remain on until the event queue
		// event is called
		//

		status = pEvent->cbFunc(*this);
		if (status==S_cas_success) {
			/*
			 * only remove it after it was accepted by the
			 * client
			 */
			nAccepted++;
		}
		else if (status==S_cas_sendBlocked) {
			/*
			 * not accepted so return to the head of the list
			 * (we will try again later)
			 */
			this->pushOnToEventQueue(*pEvent);
			cond = casProcOk;
			break;
		}
		else if (status==S_cas_disconnect) {
			cond = casProcDisconnect;
			break;
		}
		else {
			errMessage(status, "- unexpected error processing event");
			cond = casProcDisconnect;
			break;
		}
  	}

	/*
	 * call flush function if they provided one 
	 */
	if (nAccepted > 0u) {
		this->eventFlush ();
	}

	this->mutex.unlock();

	//
	// allows the derived class to be informed that it
	// needs to delete itself via the event system
	//
	// this gets the server out of nasty situations
	// where the client needs to be deleted but
	// the caller may be using the client's "this"
	// pointer.
	//
	if (this->destroyPending) {
		cond = casProcDisconnect;
	}

	return cond;
}

//
// casEventSys::eventsOn()
// 
void casEventSys::eventsOn()
{
	this->mutex.lock();

	//
	// allow multiple events for each monitor
	//
	this->replaceEvents = FALSE;

	//
	// allow the event queue to be processed
	//
	this->dontProcess = FALSE;

	//
	// remove purge event if it is still pending
	//
	if (this->pPurgeEvent != NULL) {
		this->eventLogQue.remove (*this->pPurgeEvent);
		delete this->pPurgeEvent;
		this->pPurgeEvent = NULL;
	}

	this->mutex.unlock();

    //
    // wakes up the event queue consumer
    //
    this->eventSignal ();
}

//
// casEventSys::eventsOff()
//
caStatus casEventSys::eventsOff()
{
	this->mutex.lock();

	//
	// new events will replace the last event on
	// the queue for a particular monitor
	//
	this->replaceEvents = TRUE;

	//
	// suppress the processing and sending of events
	// only after we have purged the event queue
	// for this particular client
	//
	if (this->pPurgeEvent==NULL) {
		this->pPurgeEvent = new casEventPurgeEv;
		if (this->pPurgeEvent==NULL) {
			//
			// if there is no room for the event then immediately
			// stop processing and sending events to the client
			// until we exit flow control
			//
			this->dontProcess = TRUE;
		}
		else {
			this->casEventSys::addToEventQueue(*this->pPurgeEvent);
		}
	}

	this->mutex.unlock();

	return S_cas_success;
}

// this is a pure virtual function, but we nevertheless need a  
// noop to be called if they post events when a channel is being 
// destroyed when we are in the casStrmClient destructor
void casEventSys::eventSignal()
{
}

// this is a pure virtual function, but we nevertheless need a  
// noop to be called if they call this when we are in the 
// casStrmClient destructor
caStatus casEventSys::disconnectChan (caResId id)
{
    return S_cas_success;
}

// this is a pure virtual function, but we nevertheless need a  
// noop to be called if they call this when we are in the 
// casStrmClient destructor
void casEventSys::eventFlush ()
{
}

// this is a pure virtual function, but we nevertheless need a  
// noop to be called if they call this when we are in the 
// casStrmClient destructor
casRes * casEventSys::lookupRes ( const caResId &, casResType )
{
    return 0;
}

//
// casEventPurgeEv::cbFunc()
// 
caStatus casEventPurgeEv::cbFunc (casEventSys &evSys)
{
	evSys.mutex.lock();
	evSys.dontProcess = TRUE;
	evSys.pPurgeEvent = NULL;
	evSys.mutex.unlock();

	delete this;

	return S_cas_success;
}

