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


#ifndef casChannelIIL_h
#define casChannelIIL_h

#include "casCoreClientIL.h"
#include "casEventSysIL.h"

//
// casChannelI::lock()
//
inline void casChannelI::lock() const
{
	this->client.lock();
}

//
// casChannelI::unlock()
//
inline void casChannelI::unlock() const
{
	this->client.unlock();
}

//
// casChannelI::postEvent()
//
inline void casChannelI::postEvent(const casEventMask &select, gdd &event)
{
	this->lock();
        tsDLIterBD<casMonitor> iter(this->monitorList.first());
        while ( iter!=tsDLIterBD<casMonitor>::eol() ) {
                iter->post(select, event);
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
// casChannelI::findMonitor
// (it is reasonable to do a linear search here because
// sane clients will require only one or two monitors
// per channel)
//
inline casMonitor *casChannelI::findMonitor(const caResId clientIdIn)
{
	this->lock();
	tsDLIterBD<casMonitor> iter(this->monitorList.first());
    while ( iter!=tsDLIterBD<casMonitor>::eol() ) {
		if ( clientIdIn == iter->getClientId()) {
			casMonitor *pMon = iter;
			return pMon;
		}
		++iter;
	}
	this->unlock();
	return NULL;
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
        this->pv.unregisterIO();
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
	if (!this->accessRightsEvPending) {
		this->accessRightsEvPending = TRUE;
		this->client.addToEventQueue(*this);
	}
}

//
// casChannelI::enumStringTable ()
//
inline const vector<string> casChannelI::enumStringTable () const
{
    return this->pv.enumStringTable ();
}

#endif // casChannelIIL_h

