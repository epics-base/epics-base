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
 * Revision 1.2  1996/09/16 18:24:02  jhill
 * vxWorks port changes
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casEventSysIL_h
#define casEventSysIL_h

//
// required for casCoreClient::lookupRes()
//
#include "casCoreClientIL.h"

//
// casEventSys::casEventSys ()
//
inline casEventSys::casEventSys (casCoreClient &coreClientIn) :
	coreClient(coreClientIn),
	numEventBlocks(0u),
	maxLogEntries(individualEventEntries),
	eventsOff(aitFalse)
{
}

//
// casEventSys::init()
//
inline caStatus casEventSys::init()
{
	if (mutex.init()) {
		return S_cas_noMemory;
	}
	return S_cas_success;
}

//
// casEventSys::addToEventQueue()
//
inline void casEventSys::addToEventQueue(casEvent &event)
{
        this->mutex.osiLock();
        this->eventLogQue.add(event);
        this->mutex.osiUnlock();
        //
        // wakes up the event queue consumer
        //
        this->coreClient.eventSignal();
}

//
// casEventSys::getCoreClient
//
inline casCoreClient &casEventSys::getCoreClient()
{
	return this->coreClient;
}

//
// casEventSys::getEventsOff ()
//
inline aitBool casEventSys::getEventsOff () const
{
	return this->eventsOff?aitTrue:aitFalse;
}

//
// casEventSys::setEventsOn()
//
void casEventSys::setEventsOn()
{
	this->eventsOff = aitFalse;
}

//
// casEventSys::setEventsOff()
//
void casEventSys::setEventsOff()
{
	this->eventsOff = aitTrue;
}

//
// casEventSys::insertEventQueue()
//
inline void casEventSys::insertEventQueue(casEvent &insert, casEvent &prevEvent)
{
        this->mutex.osiLock();
        this->eventLogQue.insertAfter(insert, prevEvent);
        this->mutex.osiUnlock();
}
 
//
// casEventSys::pushOnToEventQueue()
//
inline void casEventSys::pushOnToEventQueue(casEvent &event)
{
        this->mutex.osiLock();
        this->eventLogQue.push(event);
        this->mutex.osiUnlock();
}
 
//
// casEventSys::removeFromEventQueue()
//
inline void casEventSys::removeFromEventQueue(casEvent &event)
{
        this->mutex.osiLock();
        this->eventLogQue.remove(event);
        this->mutex.osiUnlock();
}
 
//
// casEventSys::full()
//
inline aitBool casEventSys::full()
{
        if (this->eventLogQue.count()>=this->maxLogEntries) {
                return aitTrue;
        }
        else {
                return aitFalse;
        }
}

//
// casMonitor::resIdToMon()
//
inline casMonitor *casEventSys::resIdToMon(const caResId id)
{
        casRes *pRes = this->coreClient.lookupRes(id, casClientMonT);
 
        //
        // safe to cast because we have checked the type code above
        // (and we know that casMonitor derived from casRes)
        //
        return (casMonitor *) pRes;
}

#endif // casEventSysIL_h

