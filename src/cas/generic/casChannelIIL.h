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


#ifndef casChannelIIL_h
#define casChannelIIL_h

#include "casCoreClientIL.h"
#include "casEventSysIL.h"

//
// casChannelI::lock()
//
inline void casChannelI::lock() const
{
	this->pClient->lock();
}

//
// casChannelI::unlock()
//
inline void casChannelI::unlock() const
{
	this->pClient->unlock();
}

//
// casChannelI::postEvent()
//
inline void casChannelI::postEvent (const casEventMask &select, const gdd &event)
{
	this->lock();

    tsDLIter<casMonitor> iter = this->monitorList.firstIter ();
    while ( iter.valid () ) {
        iter->post (select, event);
	    ++iter;
    }

	this->unlock();
}


//
// casChannelI::deleteMonitor()
//
inline void casChannelI::deleteMonitor(casMonitor &mon)
{
	casRes *pRes;
	this->lock();
	this->getClient().casEventSys::removeMonitor();
	this->monitorList.remove(mon);
	pRes = this->getClient().getCAS().removeItem(mon);
	this->unlock();
	assert(&mon == (casMonitor *)pRes);
}

//
// casChannelI::addMonitor()
//
inline void casChannelI::addMonitor(casMonitor &mon)
{
	this->lock();
	this->monitorList.add(mon);
	this->getClient().getCAS().installItem(mon);
	this->getClient().casEventSys::installMonitor();
	this->unlock();
}

//
// casChannelI::destroyNoClientNotify()
//
inline void casChannelI::destroyNoClientNotify() 
{
	this->destroy ();
}

#include "casPVIIL.h"

//
// functions that use casPVIIL.h below here 
//

//
// casChannelI::installAsyncIO()
//
inline void casChannelI::installAsyncIO(casAsyncIOI &io)
{
        this->lock();
        this->ioInProgList.add(io);
        this->unlock();
}

//
// casChannelI::removeAsyncIO()
//
inline void casChannelI::removeAsyncIO(casAsyncIOI &io)
{
        this->lock();
        this->ioInProgList.remove(io);
        this->pPV->unregisterIO();
        this->unlock();
}

//
// casChannelI::getSID()
// fetch the unsigned integer server id for this PV
//
inline const caResId casChannelI::getSID()
{
    return this->casRes::getId();
}

//
// casChannelI::postAccessRightsEvent()
//
inline void casChannelI::postAccessRightsEvent()
{
	if ( ! this->accessRightsEvPending ) {
		this->accessRightsEvPending = true;
		this->pClient->addToEventQueue ( *this );
	}
}

//
// casChannelI::enumStringTable ()
//
inline const gddEnumStringTable & casChannelI::enumStringTable () const
{
    return this->pPV->enumStringTable ();
}

#endif // casChannelIIL_h

