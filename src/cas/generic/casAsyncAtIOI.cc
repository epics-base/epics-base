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
 *
 */

#include "server.h"
#include "casAsyncIOIIL.h" 	// casAsyncIOI in line func
#include "casChannelIIL.h"	// casChannelI in line func
#include "casCtxIL.h"		// casCtx in line func
#include "casCoreClientIL.h"	// casCoreClient in line func

//
// casAsyncAtIOI::casAsyncAtIOI()
//
epicsShareFunc casAsyncAtIOI::casAsyncAtIOI (const casCtx &ctx, 
			casAsyncPVAttachIO &ioIn) :
	casAsyncIOI (*ctx.getClient(), ioIn),
	msg (*ctx.getMsg()),
	retVal (S_cas_badParameter)
{
	assert (&this->msg);
	this->client.installAsyncIO (*this);
}

//
// casAsyncAtIOI::~casAsyncAtIOI()
//
casAsyncAtIOI::~casAsyncAtIOI()
{
	this->client.removeAsyncIO (*this);
}

//
// casAsyncAtIOI::postIOCompletion()
//
epicsShareFunc caStatus casAsyncAtIOI::postIOCompletion(const pvAttachReturn &retValIn)
{
	this->retVal = retValIn; 
	return this->postIOCompletionI ();
}


//
// casAsyncAtIOI::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
caStatus casAsyncAtIOI::cbFuncAsyncIO()
{
	caStatus 	status;

	switch (this->msg.m_cmmd) {
	case CA_PROTO_CLAIM_CIU:
		status = this->client.createChanResponse (this->msg, this->retVal);
		break;

	default:
		this->reportInvalidAsynchIO (this->msg.m_cmmd);
		status = S_cas_internal;
		break;
	}

	return status;
}

