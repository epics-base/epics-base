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
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casChannelIIL_h
#define casChannelIIL_h

//
// casChannelI::lock()
//
inline void casChannelI::lock()
{
	this->client.lock();
}

//
// casChannelI::unlock()
//
inline void casChannelI::unlock()
{
	this->client.unlock();
}

//
// casChannelI::postEvent()
//
inline void casChannelI::postEvent(const casEventMask &select, gdd &event)
{
        casMonitor              *pMon;
        tsDLIter<casMonitor>    iter(this->monitorList);
 
	this->lock();
        while ( (pMon = iter()) ) {
                pMon->post(select, event);
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
	assert(&mon == (casMonitor *)pRes);
	this->unlock();
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
	tsDLIter<casMonitor>    iter(this->monitorList);
	casMonitor              *pMon;

	while ( (pMon = iter()) ) {
		if ( clientIdIn == pMon->getClientId()) {
			return pMon;
		}
	}
	return NULL;
}

//
// casChannelI::clientDestroy()
//
inline void casChannelI::clientDestroy() 
{
	this->clientDestroyPending=TRUE;
	(*this)->destroy();
}

#include <casPVIIL.h>

//
// functions that use casPVIIL.h below here 
//

//
// casChannelI::installAsyncIO()
//
inline void casChannelI::installAsyncIO(casAsyncIOI &io)
{
        this->lock();
        this->pv.registerIO();
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
	return this->uintId::getId();
}

#endif // casChannelIIL_h

