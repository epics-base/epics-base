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
#include <server.h>
#include <casEventSysIL.h> // casMonitor inline func

//
// casEventSys::show()
//
void casEventSys::show(unsigned level) const
{
	printf ("casEventSys at %x\n", (unsigned) this);
	if (level>=1u) {
		printf ("\thas coreClient at %x\n", (unsigned) &this->coreClient);
		printf ("\tnumEventBlocks = %d, maxLogEntries = %d\n",
			this->numEventBlocks, this->maxLogEntries);	
		printf ("\tthere are %d events in the queue\n",
			this->eventLogQue.count());
		printf ("\tevents off =  %d\n", this->eventsOff);
	}
}


//
// casEventSys::~casEventSys()
//
casEventSys::~casEventSys()
{
	casEvent 	*pE;
	
	/*
	 * all active event blocks must be canceled first
	 */
	assert (this->numEventBlocks==0);

	this->mutex.osiLock();

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

	while ( (pEvent = this->eventLogQue.get()) ) {

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
			errMessage(status, 
				"unexpect error processing event");
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

	return cond;
}

