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
 *
 */


#include "server.h"
#include "casChannelIIL.h" // casChannelI inline func
#include "casEventSysIL.h" // casEventSys inline func
#include "casMonEventIL.h" // casMonEvent inline func
#include "casCtxIL.h" // casCtx inline func


//
// casMonitor::casMonitor()
//
casMonitor::casMonitor(caResId clientIdIn, casChannelI &chan,
	unsigned long nElemIn, unsigned dbrTypeIn,
	const casEventMask &maskIn, osiMutex &mutexIn) :
	nElem(nElemIn),
	mutex(mutexIn),
	ciu(chan),
	mask(maskIn),
	clientId(clientIdIn),
	dbrType(dbrTypeIn),
	nPend(0u),
	ovf(FALSE),
	enabled(FALSE)
{
	//
	// If these are nill it is a programmer error
	//
	assert (&this->ciu);

	this->ciu.addMonitor(*this);

	this->enable();
}

//
// casMonitor::~casMonitor()
//
casMonitor::~casMonitor()
{
	casCoreClient &client = this->ciu.getClient();
	
	this->mutex.lock();
	
	this->disable();
	
	//
	// remove from the event system
	//
	if (this->ovf) {
		client.removeFromEventQueue (this->overFlowEvent);
	}

	this->ciu.deleteMonitor(*this);
	
	this->mutex.unlock();
}

//
// casMonitor::enable()
//
void casMonitor::enable()
{
	caStatus status;
	
	this->mutex.lock();
	if (!this->enabled && this->ciu.readAccess()) {
		this->enabled = TRUE;
		status = this->ciu.getPVI().registerEvent();
		if (status) {
			errMessage(status,
				"Server tool failed to register event\n");
		}
	}
	this->mutex.unlock();
}

//
// casMonitor::disable()
//
void casMonitor::disable()
{
	this->mutex.lock();
	if (this->enabled) {
		this->enabled = FALSE;
		this->ciu.getPVI().unregisterEvent();
	}
	this->mutex.unlock();
}

//
// casMonitor::push()
//
void casMonitor::push(gdd &newValue)
{
	casCoreClient	&client = this->ciu.getClient();
	casMonEvent 	*pLog;
	char			full;
	
	this->mutex.lock();
	
    client.getCAS().incrEventsPostedCounter ();

	//
	// get a new block if we havent exceeded quotas
	//
	full = (this->nPend>=individualEventEntries) 
		|| client.casEventSys::full();
	if (!full) {
		pLog = new casMonEvent(*this, newValue);
		if (pLog) {
			this->nPend++;
		}
	}
	else {
		pLog = NULL;
	}
	
	if (this->ovf) {
		if (pLog) {
			//
			// swap values
			// (ugly - but avoids purify ukn sym type problem)
			// (better to create a temp event object)
			//
			smartGDDPointer pValue = this->overFlowEvent.getValue();
			assert (pValue!=NULL);
			this->overFlowEvent = *pLog;
			pLog->assign(*this, pValue);
			client.insertEventQueue(*pLog, this->overFlowEvent);
		}
		else {
			//
			// replace the value with the current one
			//
			this->overFlowEvent.assign(*this, &newValue);
		}
		client.removeFromEventQueue(this->overFlowEvent);
		pLog = &this->overFlowEvent;
	}
	else if (!pLog) {
		//
		// no log block
		// => use the over flow block in the event structure
		//
		this->ovf = TRUE;
		this->overFlowEvent.assign(*this, &newValue);
		this->nPend++;
		pLog = &this->overFlowEvent;
	}
	
	client.addToEventQueue(*pLog);
	
	this->mutex.unlock();
}

//
// casMonitor::executeEvent()
//
caStatus casMonitor::executeEvent(casMonEvent *pEV)
{
	caStatus status;
	smartGDDPointer pVal;
	
	pVal = pEV->getValue ();
	assert (pVal!=NULL);
	
	this->mutex.lock();
	status = this->callBack (*pVal);
	this->mutex.unlock();
	
	//
	// if the event isnt accepted we will try
	// again later (and the event returns to the queue)
	//
	if (status) {
		return status;
	}
	
	//
	// decrement the count of the number of events pending
	//
	this->nPend--;
	
	//
	// delete event object if it isnt a cache entry
	// saved in the call back object
	//
	if (pEV == &this->overFlowEvent) {
		assert (this->ovf==TRUE);
		this->ovf = FALSE;
		pEV->clear();
	}
	else {
		delete pEV;
	}

    this->ciu.getClient().getCAS().incrEventsProcessedCounter ();
	
	return S_cas_success;
}

//
// casMonitor::show(unsigned level)
//
void casMonitor::show(unsigned level) const
{
        if (level>1u) {
                printf(
"\tmonitor type=%u count=%lu client id=%u enabled=%u OVF=%u nPend=%u\n",
                        dbrType, nElem, clientId, enabled, ovf, nPend);
		this->mask.show(level);
        }
}

