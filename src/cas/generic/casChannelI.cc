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
    tsDLIterBD<casAsyncIOI> iterAIO = this->ioInProgList.firstIter ();
    while ( iterAIO.valid () ) {
        //
        // destructor removes from this list
        //
        tsDLIterBD <casAsyncIOI> tmpAIO = iterAIO;
        ++tmpAIO;
        iterAIO->serverDestroy ();
        iterAIO = tmpAIO;
    }
    
    //
    // cancel the monitors 
    //
    tsDLIterBD <casMonitor> iterMon = this->monitorList.firstIter ();
    while ( iterMon.valid () ) {
        //
        // destructor removes from this list
        //
        tsDLIterBD <casMonitor> tmpMon = iterMon;
        ++tmpMon;
        iterMon->destroy ();
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
	tsDLIterBD <casAsyncIOI> iterIO = this->ioInProgList.firstIter ();
	while ( iterIO.valid () ) {
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
void casChannelI::show ( unsigned level ) const
{
	this->lock ();

	tsDLIterConstBD <casMonitor> iter = this->monitorList.firstIter ();
	if ( iter.valid () ) {
		printf("List of CA events (monitors) for \"%s\".\n",
			this->pv.getName());
	}
	while ( iter.valid () ) {
		iter->show ( level );
		++iter;
	}

	this->show ( level );

	this->unlock ();
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

//
// casChannelI::findMonitor
// (it is reasonable to do a linear search here because
// sane clients will require only one or two monitors
// per channel)
//
tsDLIterBD <casMonitor> casChannelI::findMonitor (const caResId clientIdIn)
{
	this->lock ();
	tsDLIterBD <casMonitor> iter = this->monitorList.firstIter ();
    while ( iter.valid () ) {
		if ( clientIdIn == iter->getClientId () ) {
			break;
		}
		iter++;
	}
	this->unlock ();
	return iter;
}

