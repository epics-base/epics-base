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
 */

#include "server.h"
#include "casEventSysIL.h" // casEventSys inline func
#include "casAsyncIOIIL.h" // casAsyncIOI inline func
#include "casPVIIL.h" // casPVI inline func
#include "casCtxIL.h" // casCtx inline func

//
// casChannelI::casChannelI()
//
casChannelI::casChannelI (const casCtx &ctx) :
		client ( * (casStrmClient *) ctx.getClient ()), 
        pv (*ctx.getPV()),
		cid (ctx.getMsg()->m_cid),
		accessRightsEvPending (FALSE)
{
	assert (&this->client);
	assert (&this->pv);

	this->client.installChannel (*this);
}

//
// casChannelI::~casChannelI()
//
casChannelI::~casChannelI()
{	
	this->lock();
	
	//
	// cancel any pending asynchronous IO 
	//
	tsDLIterBD<casAsyncIOI> iterAIO(this->ioInProgList.first());
	while ( iterAIO!=tsDLIterBD<casAsyncIOI>::eol() ) {
		//
		// destructor removes from this list
		//
		tsDLIterBD<casAsyncIOI> tmpAIO = iterAIO;
		++tmpAIO;
		iterAIO->serverDestroy();
		iterAIO = tmpAIO;
	}
	
	//
	// cancel the monitors 
	//
	tsDLIterBD<casMonitor> iterMon (this->monitorList.first());
	while ( iterMon!=tsDLIterBD<casMonitor>::eol() ) {
		casMonitor *pMonitor;
		//
		// destructor removes from this list
		//
        tsDLIterBD<casMonitor> tmpMon = iterMon;
		pMonitor = tmpMon;
		++tmpMon;
		delete pMonitor;
		iterMon = tmpMon;
	}
	
	this->client.removeChannel(*this);
	
	//
	// force PV delete if this is the last channel attached
	//
	this->pv.deleteSignal();
	
	this->unlock();
}

//
// casChannelI::clearOutstandingReads()
//
void casChannelI::clearOutstandingReads()
{
	this->lock();

    //
    // cancel any pending asynchronous IO 
    //
	tsDLIterBD<casAsyncIOI> iterIO(this->ioInProgList.first());
	while (iterIO!=tsDLIterBD<casAsyncIOI>::eol() ) {
		//
		// destructor removes from this list
		//
		tsDLIterBD<casAsyncIOI> tmp = iterIO;
		++tmp;
		iterIO->serverDestroyIfReadOP();
		iterIO = tmp;
	}

	this->unlock();
}

//
// casChannelI::show()
//
void casChannelI::show(unsigned level) const
{
	this->lock();

	tsDLIterBD<casMonitor> iter(this->monitorList.first());
	if ( iter!=tsDLIterBD<casMonitor>::eol() ) {
		printf("List of CA events (monitors) for \"%s\".\n",
			this->pv.getName());
	}
	while ( iter!=tsDLIterBD<casMonitor>::eol() ) {
		iter->show(level);
		++iter;
	}

	this->show(level);

	this->unlock();
}

//
// casChannelI::cbFunc()
//
// access rights event call back
//
caStatus casChannelI::cbFunc(casEventSys &)
{
	caStatus stat;

	stat = this->client.accessRightsResponse(this);
	if (stat==S_cas_success) {
		this->accessRightsEvPending = FALSE;
	}
	return stat;
}

//
// casChannelI::resourceType()
//
casResType casChannelI::resourceType() const
{
	return casChanT;
}

//
// casChannelI::destroy()
//
// this noop version is safe to be called indirectly
// from casChannelI::~casChannelI
//
epicsShareFunc void casChannelI::destroy()
{
}

void casChannelI::destroyClientNotify ()
{
	casChanDelEv *pCDEV;
    caStatus status;

	pCDEV = new casChanDelEv (this->getCID());
	if (pCDEV) {
		this->client.casEventSys::addToEventQueue (*pCDEV);
	}
	else {	
		status = this->client.disconnectChan (this->getCID());
		if (status) {
			//
			// At this point there is no space in pool
			// for a tiny object and there is also
			// no outgoing buffer space in which to place
			// a message in which we inform the client
			// that his channel was deleted.
			//
			// => disconnect this client via the event
			// queue because deleting the client here
			// will result in bugs because no doubt this
			// could be called by a client member function.
			//
			this->client.setDestroyPending();
		}
	}
    this->destroy();
}


