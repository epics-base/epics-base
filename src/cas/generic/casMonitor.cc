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
	const casEventMask &maskIn, epicsMutex &mutexIn) :
	nElem(nElemIn),
	mutex(mutexIn),
	ciu(chan),
	mask(maskIn),
	clientId(clientIdIn),
	dbrType(static_cast <unsigned char> ( dbrTypeIn ) ),
	nPend(0u),
	ovf(false),
	enabled(false)
{
	//
	// If these are nill it is a programmer error
	//
	assert (&this->ciu);
	assert (dbrTypeIn<=0xff);

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
		this->enabled = true;
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
		this->enabled = false;
		this->ciu.getPVI().unregisterEvent();
	}
	this->mutex.unlock();
}

//
// casMonitor::push()
//
void casMonitor::push (const smartConstGDDPointer &pNewValue)
{
	casCoreClient	&client = this->ciu.getClient ();
	casMonEvent 	*pLog;
	char			full;
	
	this->mutex.lock();
	
    client.getCAS().incrEventsPostedCounter ();

	//
	// get a new block if we havent exceeded quotas
	//
	full = ( this->nPend >= individualEventEntries ) 
		|| client.casEventSys::full ();
	if (!full) {
		pLog = new casMonEvent (*this, pNewValue);
		if (pLog) {
			this->nPend++; // X aCC 818
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
			smartConstGDDPointer pValue = this->overFlowEvent.getValue ();
            if (!pValue) {
                assert (0);
            }
			this->overFlowEvent = *pLog;
			pLog->assign (*this, pValue);
			client.insertEventQueue (*pLog, this->overFlowEvent);
		}
		else {
			//
			// replace the value with the current one
			//
			this->overFlowEvent.assign (*this, pNewValue);
		}
		client.removeFromEventQueue (this->overFlowEvent);
		pLog = &this->overFlowEvent;
	}
	else if (!pLog) {
		//
		// no log block
		// => use the over flow block in the event structure
		//
		this->ovf = true;
		this->overFlowEvent.assign (*this, pNewValue);
		this->nPend++; // X aCC 818
		pLog = &this->overFlowEvent;
	}
	
	client.addToEventQueue (*pLog);
	
	this->mutex.unlock ();
}

//
// casMonitor::executeEvent()
//
caStatus casMonitor::executeEvent(casMonEvent *pEV)
{
	caStatus status;
	smartConstGDDPointer pVal;
	
	pVal = pEV->getValue ();
    if (!pVal) {
        assert (0);
    }
	
	this->mutex.lock ();
	status = this->callBack (*pVal);
	this->mutex.unlock ();
	
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
	this->nPend--; // X aCC 818
	
	//
	// delete event object if it isnt a cache entry
	// saved in the call back object
	//
	if (pEV == &this->overFlowEvent) {
		assert (this->ovf);
		this->ovf = false;
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

