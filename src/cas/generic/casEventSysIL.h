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


#ifndef casEventSysIL_h
#define casEventSysIL_h

//
// casEventSys::casEventSys ()
//
inline casEventSys::casEventSys () :
	pPurgeEvent (NULL),
	numEventBlocks (0u),
	maxLogEntries (individualEventEntries),
	destroyPending (false),
	replaceEvents (false), 
	dontProcess (false) 
{
}

//
// casEventSys::addToEventQueue()
//
inline void casEventSys::addToEventQueue(casEvent &event)
{
	this->mutex.lock();
	this->eventLogQue.add(event);
	this->mutex.unlock();
	//
	// wake up the event queue consumer only if
	// we are not supressing events to a client that
	// is in flow control
	//
	if (!this->dontProcess) {
		this->eventSignal();
	}
}

//
// casEventSys::setDestroyPending()
//
inline void casEventSys::setDestroyPending()
{
	this->destroyPending = true;
    //
    // wakes up the event queue consumer
    //
    this->eventSignal();
}

//
// casEventSys::insertEventQueue()
//
inline void casEventSys::insertEventQueue(casEvent &insert, casEvent &prevEvent)
{
	this->mutex.lock();
	this->eventLogQue.insertAfter(insert, prevEvent);
	this->mutex.unlock();
}
 
//
// casEventSys::pushOnToEventQueue()
//
inline void casEventSys::pushOnToEventQueue (casEvent &event)
{
	this->mutex.lock ();
	this->eventLogQue.push (event);
	this->mutex.unlock ();
}
 
//
// casEventSys::removeFromEventQueue()
//
inline void casEventSys::removeFromEventQueue(casEvent &event)
{
	this->mutex.lock();
	this->eventLogQue.remove(event);
	this->mutex.unlock();
}
 
//
// casEventSys::full()
//
inline bool casEventSys::full() // X aCC 361
{
	if (this->replaceEvents || this->eventLogQue.count()>=this->maxLogEntries) {
		return true;
	}
	else {
		return false;
	}
}

//
// casMonitor::resIdToMon()
//
inline casMonitor *casEventSys::resIdToMon(const caResId id)
{
	casRes *pRes = this->lookupRes (id, casClientMonT);
	
	//
	// safe to cast because we have checked the type code above
	// (and we know that casMonitor derived from casRes)
	//
	return (casMonitor *) pRes;
}

#endif // casEventSysIL_h

