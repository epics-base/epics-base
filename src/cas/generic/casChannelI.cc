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
 * Revision 1.1.1.1  1996/06/20 00:28:14  jhill
 * ca server installation
 *
 *
 */

#include <server.h>
#include <casEventSysIL.h> // casEventSys inline func
#include <casAsyncIOIIL.h> // casAsyncIOI inline func
#include <casPVIIL.h> // casPVI inline func


//
// casChannelI::casChannelI()
//
casChannelI::casChannelI(const casCtx &ctx, casChannel &chanAdapter) :
		client(* (casStrmClient *) ctx.getClient()), 
		pv(*ctx.getPV()), 
		chan(chanAdapter),
		cid(ctx.getMsg()->m_cid)
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
        casAsyncIOI		*pIO;
	tsDLIter<casAsyncIOI>	iterIO(this->ioInProgList);
        casMonitor		*pMonitor;
	tsDLIter<casMonitor>	iterMon(this->monitorList);
	casChanDelEv		*pCDEV;
	caStatus		status;

	this->lock();

        //
        // cancel any pending asynchronous IO 
        //
	pIO = iterIO();
        while (pIO) {
		casAsyncIOI	*pNextIO;
		//
		// destructor removes from this list
		//
		pNextIO = iterIO();
		pIO->setServerDelete();
		(*pIO)->destroy();
                pIO = pNextIO;
        }

	//
	// cancel the monitors 
	//
	pMonitor = iterMon();
        while (pMonitor) {
        	casMonitor *pNextMon;
		//
		// destructor removes from this list
		//
		pNextMon = iterMon();
		delete pMonitor;
		pMonitor = pNextMon;
        }

	this->client.removeChannel(*this);

        //
        // force PV delete if this is the last channel attached
        //
        this->pv.deleteSignal();

	//
	// if its an old client and there is no memory
	// we disconnect them here (and delete the client) 
	// - therefore dont use member client after this point
	//
	if (!this->clientDestroyPending) {
		pCDEV = new casChanDelEv(this->getCID());
		if (pCDEV) {
			this->client.casEventSys::addToEventQueue(*pCDEV);
		}
		else {	
			status = this->client.disconnectChan (this->getCID());
			if (status) {
				errMessage(status, NULL);
				if (status == S_cas_disconnect) {
					this->client.destroy();
				}
			}
		}
	}

	this->unlock();
}


//
// casChannelI::postAllModifiedEvents()
//
void casChannelI::postAllModifiedEvents()
{
	casMonitor		*pMon;
	tsDLIter<casMonitor> 	iter(this->monitorList);

        this->lock();
        while ( (pMon=iter()) ) {
		pMon->postIfModified(); 
        }
        this->unlock();
}


//
// casChannelI::show()
//
void casChannelI::show(unsigned level)
{
	casMonitor	 	*pMon;
	tsDLIter<casMonitor>	iter(this->monitorList);

	this->lock();

	if ( (pMon = iter()) ) {
		printf("List of CA events (monitors) for \"%s\".\n",
			this->pv.resourceName());
	}

	while (pMon) {
		pMon->show(level);
		pMon = iter();
	}

	(*this)->show(level);

	this->unlock();
}

