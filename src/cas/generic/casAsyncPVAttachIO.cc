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
#include "casAsyncIOIIL.h" 	// casAsyncIOI in line func
#include "casChannelIIL.h"	// casChannelI in line func
#include "casCtxIL.h"		// casCtx in line func
#include "casCoreClientIL.h"	// casCoreClient in line func

//
// casAsyncPVAttachIO::casAsyncPVAttachIO()
//
casAsyncPVAttachIO::casAsyncPVAttachIO (const casCtx &ctx) :
	casAsyncIOI (*ctx.getClient()),
	msg (*ctx.getMsg()),
	retVal (S_cas_badParameter)
{
	this->client.installAsyncIO (*this);
}

//
// casAsyncPVAttachIO::~casAsyncPVAttachIO()
//
casAsyncPVAttachIO::~casAsyncPVAttachIO()
{
	this->client.removeAsyncIO (*this);
}

//
// casAsyncPVAttachIO::postIOCompletion()
//
caStatus casAsyncPVAttachIO::postIOCompletion(const pvAttachReturn &retValIn)
{
	this->retVal = retValIn; 
	return this->postIOCompletionI ();
}

//
// casAsyncPVAttachIO::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
epicsShareFunc caStatus casAsyncPVAttachIO::cbFuncAsyncIO()
{
	caStatus 	status;

	switch (this->msg.m_cmmd) {
	case CA_PROTO_CLAIM_CIU:
		status = this->client.createChanResponse (this->msg, this->retVal);
		break;

	default:
        errPrintf (S_cas_invalidAsynchIO, __FILE__, __LINE__,
            " - client request type = %u", this->msg.m_cmmd);
		status = S_cas_invalidAsynchIO;
		break;
	}

	return status;
}

//
// void casAsyncPVAttachIO::destroy ()
//
void casAsyncPVAttachIO::destroy ()
{
    delete this;
}

//
// depricated
//
casAsyncPVCreateIO::casAsyncPVCreateIO(const casCtx &ctx) : 
		casAsyncPVAttachIO (ctx) 
{
}

//
// depricated
//
epicsShareFunc casAsyncPVCreateIO::~casAsyncPVCreateIO()
{
}


