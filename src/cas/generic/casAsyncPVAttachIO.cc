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
// deprecated
//
casAsyncPVCreateIO::casAsyncPVCreateIO(const casCtx &ctx) : 
		casAsyncPVAttachIO (ctx) 
{
}

//
// deprecated
//
epicsShareFunc casAsyncPVCreateIO::~casAsyncPVCreateIO()
{
}


