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
 *
 * History
 * $Log$
 * Revision 1.6  1997/06/30 22:54:27  jhill
 * use %p with pointers
 *
 * Revision 1.5  1997/04/10 19:34:09  jhill
 * API changes
 *
 * Revision 1.4  1996/11/02 00:54:12  jhill
 * many improvements
 *
 * Revision 1.3  1996/09/16 18:24:01  jhill
 * vxWorks port changes
 *
 * Revision 1.2  1996/07/24 22:00:49  jhill
 * added pushOnToEventQueue()
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
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
	printf ("casEventSys at %p\n", this);
	if (level>=1u) {
		printf ("\thas coreClient at %p\n", &this->coreClient);
		printf ("\tnumEventBlocks = %u, maxLogEntries = %u\n",
			this->numEventBlocks, this->maxLogEntries);	
		printf ("\tthere are %d events in the queue\n",
			this->eventLogQue.count());
		printf ("\tevents off = %d\n", this->eventsOff);
	}
}


//
// casEventSys::~casEventSys()
//
casEventSys::~casEventSys()
{
	casEvent 	*pE;
	
	this->mutex.osiLock();

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

	this->mutex.osiUnlock();
}


//
// casEventSys::installMonitor()
//
void casEventSys::installMonitor()
{
	this->mutex.osiLock();
	this->numEventBlocks++;
	this->maxLogEntries += averageEventEntries;
	this->mutex.osiUnlock();
}

//
// casEventSys::removeMonitor()
//
void casEventSys::removeMonitor() 
{       
	this->mutex.osiLock();
	assert (this->numEventBlocks>=1u);
	this->numEventBlocks--;
	this->maxLogEntries -= averageEventEntries;
	this->mutex.osiUnlock();
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

	this->mutex.osiLock();

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
			errMessage(status, "unexpect error processing event");
			cond = casProcDisconnect;
			break;
		}
  	}

	/*
	 * call flush function if they provided one 
	 */
	if (nAccepted > 0u) {
		this->coreClient.eventFlush();
	}

	this->mutex.osiUnlock();

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
	this->mutex.osiLock();

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

	this->mutex.osiUnlock();

    //
    // wakes up the event queue consumer
    //
    this->coreClient.eventSignal();
}

//
// casEventSys::eventsOff()
//
caStatus casEventSys::eventsOff()
{
	this->mutex.osiLock();

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

	this->mutex.osiUnlock();

	return S_cas_success;
}

//
// casEventPurgeEv::cbFunc()
// 
caStatus casEventPurgeEv::cbFunc (casEventSys &evSys)
{
	evSys.mutex.osiLock();
	evSys.dontProcess = TRUE;
	evSys.pPurgeEvent = NULL;
	evSys.mutex.osiUnlock();

	delete this;

	return S_cas_success;
}

