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
 * Revision 1.6  1996/11/02 00:54:18  jhill
 * many improvements
 *
 * Revision 1.5  1996/09/16 18:24:03  jhill
 * vxWorks port changes
 *
 * Revision 1.4  1996/07/24 22:00:49  jhill
 * added pushOnToEventQueue()
 *
 * Revision 1.3  1996/07/01 19:56:11  jhill
 * one last update prior to first release
 *
 * Revision 1.2  1996/06/26 21:18:56  jhill
 * now matches gdd api revisions
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
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
		pModifiedValue(NULL),
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
 
        this->mutex.osiLock();

	this->disable();

        //
        // remove from the event system
        //
        if (this->ovf) {
                client.removeFromEventQueue (this->overFlowEvent);
        }
	if (this->pModifiedValue) {
		this->pModifiedValue->unreference();
		this->pModifiedValue = NULL;
	}
        this->ciu.deleteMonitor(*this);

        this->mutex.osiUnlock();
}

//
// casMonitor::enable()
//
void casMonitor::enable()
{
        caStatus status;
 
        this->mutex.osiLock();
        if (!this->enabled && this->ciu->readAccess()) {
		this->enabled = TRUE;
		status = this->ciu.getPVI().registerEvent();
		if (status) {
			errMessage(status,
				"Server tool failed to register event\n");
		}
        }
        this->mutex.osiUnlock();
}

//
// casMonitor::disable()
//
void casMonitor::disable()
{
        this->mutex.osiLock();
        if (this->enabled) {
		this->enabled = FALSE;
		this->ciu.getPVI().unregisterEvent();
        }
        this->mutex.osiUnlock();
}

//
// casMonitor::push()
//
void casMonitor::push(gdd &newValue)
{
        casCoreClient	&client = this->ciu.getClient();
        casMonEvent 	*pLog = NULL;
        char            full;
 
        this->mutex.osiLock();
 
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
 
        if (this->ovf) {
                if (pLog) {
                        int gddStatus;
                        //
                        // swap values
                        // (ugly - but avoids purify ukn sym type problem)
                        // (better to create a temp event object)
                        //
                        gdd *pValue = this->overFlowEvent.getValue();
                        assert(pValue);
                        gddStatus = pValue->reference();
                        assert(!gddStatus);
                        this->overFlowEvent = *pLog;
                        pLog->assign(*this, pValue);
                        gddStatus = pValue->unreference();
                        assert(!gddStatus);
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
                /*
                 * no log block
                 *
                 * => use the over flow block in the event structure
                 */
                this->ovf = TRUE;
                this->overFlowEvent.assign(*this, &newValue);
                this->nPend++;
                pLog = &this->overFlowEvent;
        }
 
        client.addToEventQueue(*pLog);
 
        this->mutex.osiUnlock();
}

//
// casMonitor::executeEvent()
//
caStatus casMonitor::executeEvent(casMonEvent *pEV)
{
        caStatus	status;
        gdd             *pVal;
 
        pVal = pEV->getValue ();
        assert (pVal);
 
	this->mutex.osiLock();
	if (this->ciu.getClient().getEventsOff()==aitFalse) {
		status = this->callBack (*pVal);
	}
	else {
		//
		// If flow control is on save the last update
		// (and send it later when flow control goes to
		// no flow control)
		//
		if (this->pModifiedValue) {
			this->pModifiedValue->unreference ();
		}
		pVal->reference ();
		this->pModifiedValue = pVal;
		status = S_cas_success;
	}
	this->mutex.osiUnlock();
 
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

//
// casMonitor::postIfModified()
// ( this shows up undefined if g++ compiled and it is inline)
//
void casMonitor::postIfModified()
{
        this->mutex.osiLock();
        if (this->pModifiedValue) {
                this->callBack (*this->pModifiedValue);
                this->pModifiedValue->unreference ();
                this->pModifiedValue = NULL;
        }
        this->mutex.osiUnlock();
}

