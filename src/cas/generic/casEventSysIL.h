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
inline casEventSys::casEventSys ( casCoreClient & clientIn ) :
    client ( clientIn ),
	pPurgeEvent ( NULL ),
	numEventBlocks ( 0u ),
	maxLogEntries ( individualEventEntries ),
	destroyPending ( false ),
	replaceEvents ( false ), 
	dontProcess ( false ) 
{
}

//
// casEventSys::addToEventQueue()
//
inline void casEventSys::addToEventQueue ( casEvent & event )
{
	this->eventLogQue.add ( event );

	//
	// wake up the event queue consumer only if
	// we are not supressing events to a client that
	// is in flow control
	//
	if ( ! this->dontProcess ) {
		this->client.eventSignal ();
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
    this->client.eventSignal ();
}

//
// casEventSys::insertEventQueue()
//
inline void casEventSys::insertEventQueue( casEvent & insert, casEvent & prevEvent )
{
	this->eventLogQue.insertAfter ( insert, prevEvent );
}
 
//
// casEventSys::pushOnToEventQueue()
//
inline void casEventSys::pushOnToEventQueue ( casEvent & event )
{
	this->eventLogQue.push ( event );
}
 
//
// casEventSys::removeFromEventQueue()
//
inline void casEventSys::removeFromEventQueue ( casEvent & event )
{
	this->eventLogQue.remove ( event );
}
 
//
// casEventSys::full()
//
inline bool casEventSys::full() // X aCC 361
{
	if ( this->replaceEvents || this->eventLogQue.count() >= this->maxLogEntries ) {
		return true;
	}
	else {
		return false;
	}
}

#endif // casEventSysIL_h

