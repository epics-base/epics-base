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
 * Revision 1.5  1998/04/14 00:50:00  jhill
 * cosmetic
 *
 * Revision 1.4  1997/04/10 19:34:00  jhill
 * API changes
 *
 * Revision 1.3  1996/11/02 00:54:02  jhill
 * many improvements
 *
 * Revision 1.2  1996/09/04 20:18:03  jhill
 * init new chan member
 *
 * Revision 1.1.1.1  1996/06/20 00:28:14  jhill
 * ca server installation
 *
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
casChannelI::casChannelI(const casCtx &ctx, casChannel &chanAdapter) :
		client(* (casStrmClient *) ctx.getClient()), 
		pv(*ctx.getPV()), 
		chan(chanAdapter),
		cid(ctx.getMsg()->m_cid),
		clientDestroyPending(FALSE),
		accessRightsEvPending(FALSE)
{
	assert(&this->client);
	assert(&this->pv);
	assert(&this->chan);

	this->client.installChannel(*this);
}


//
// casChannelI::~casChannelI()
//
casChannelI::~casChannelI()
{
	casChanDelEv		*pCDEV;
	caStatus		status;

	this->lock();

        //
        // cancel any pending asynchronous IO 
        //
	tsDLIterBD<casAsyncIOI> iterAIO(this->ioInProgList.first());
	const tsDLIterBD<casAsyncIOI> eolAIO;
	tsDLIterBD<casAsyncIOI> tmpAIO;
        while ( iterAIO!=eolAIO ) {
		//
		// destructor removes from this list
		//
		tmpAIO = iterAIO;
		++tmpAIO;
		iterAIO->destroy();
		iterAIO = tmpAIO;
        }

	//
	// cancel the monitors 
	//
	tsDLIterBD<casMonitor> iterMon(this->monitorList.first());
	const tsDLIterBD<casMonitor> eolMon;
	tsDLIterBD<casMonitor> tmpMon;
        while ( iterMon!=eolMon ) {
        	casMonitor *pMonitor;
		//
		// destructor removes from this list
		//
		pMonitor = tmpMon = iterMon;
		++tmpMon;
		delete pMonitor;
		iterMon = tmpMon;
        }

	this->client.removeChannel(*this);

        //
        // force PV delete if this is the last channel attached
        //
        this->pv.deleteSignal();

	//
	// If we are not in the process of deleting the client
	// then inform the client that we have deleted its 
	// channel
	//
	if (!this->clientDestroyPending) {
		pCDEV = new casChanDelEv(this->getCID());
		if (pCDEV) {
			this->client.casEventSys::addToEventQueue(*pCDEV);
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
	}

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
	tsDLIterBD<casAsyncIOI> eolIO;
	tsDLIterBD<casAsyncIOI> tmp;
	while (iterIO!=eolIO) {
		//
		// destructor removes from this list
		//
		tmp = iterIO;
		++tmp;
		iterIO->destroyIfReadOP();
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
	tsDLIterBD<casMonitor> eol;
	if ( iter!=eol ) {
		printf("List of CA events (monitors) for \"%s\".\n",
			this->pv->getName());
	}
	while ( iter!=eol ) {
		iter->show(level);
		++iter;
	}

	(*this)->show(level);

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
// casChannelI::destroy()
//
// call the destroy in the server tool
//
void casChannelI::destroy()
{
        this->chan.destroy();
}

//
// casChannelI::resourceType()
//
casResType casChannelI::resourceType() const
{
	return casChanT;
}

