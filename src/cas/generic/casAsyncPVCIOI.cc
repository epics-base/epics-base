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
 * Revision 1.1  1997/04/10 19:38:14  jhill
 * installed
 *
 * Revision 1.2  1996/11/06 22:15:53  jhill
 * allow monitor init read to using rd async io
 *
 * Revision 1.1  1996/11/02 01:01:02  jhill
 * installed
 *
 *
 *
 */


#include "server.h"
#include "casAsyncIOIIL.h" 	// casAsyncIOI in line func
#include "casChannelIIL.h"	// casChannelI in line func
#include "casCtxIL.h"		// casCtx in line func
#include "casCoreClientIL.h"	// casCoreClient in line func

//
// casAsyncPVCIOI::casAsyncPVCIOI()
//
casAsyncPVCIOI::casAsyncPVCIOI(const casCtx &ctx, 
			casAsyncPVCreateIO &ioIn) :
	casAsyncIOI(*ctx.getClient(), ioIn),
	msg(*ctx.getMsg()),
	retVal(S_cas_badParameter)
{
	assert (&this->msg);
	this->client.installAsyncIO(*this);
}

//
// casAsyncPVCIOI::~casAsyncPVCIOI()
//
casAsyncPVCIOI::~casAsyncPVCIOI()
{
	this->client.removeAsyncIO(*this);
}

//
// casAsyncPVCIOI::postIOCompletion()
//
caStatus casAsyncPVCIOI::postIOCompletion(const pvCreateReturn &retValIn)
{
	this->retVal = retValIn; 
	return this->postIOCompletionI();
}


//
// casAsyncPVCIOI::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
caStatus casAsyncPVCIOI::cbFuncAsyncIO()
{
	caStatus 	status;

        switch (this->msg.m_cmmd) {
        case CA_PROTO_CLAIM_CIU:
		status = this->client.createChanResponse(this->msg, this->retVal);
                break;
 
        default:
                this->reportInvalidAsynchIO(this->msg.m_cmmd);
                status = S_cas_internal;
                break;
        }

	return status;
}

