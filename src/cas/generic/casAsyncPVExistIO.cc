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
#include "casChannelIIL.h"	// casChannelI in line func
#include "casCtxIL.h"		// casCtx in line func
#include "casCoreClientIL.h"	// casCoreClient in line func

//
// casAsyncPVExistIO::casAsyncPVExistIO()
//
casAsyncPVExistIO::casAsyncPVExistIO (const casCtx &ctx) :
	casAsyncIOI ( ctx ),
	msg ( *ctx.getMsg () ),
	retVal (pverDoesNotExistHere),
	dgOutAddr ( ctx.getClient ()->fetchLastRecvAddr () ),
    protocolRevision ( ctx.getClient ()->protocolRevision () ),
    sequenceNumber ( ctx.getClient ()->datagramSequenceNumber () )
{
	this->client.installAsyncIO (*this);
}

//
// casAsyncPVExistIO::~casAsyncPVExistIO ()
//
casAsyncPVExistIO::~casAsyncPVExistIO ()
{
	this->client.removeAsyncIO (*this);
}

//
// casAsyncPVExistIO::postIOCompletion ()
//
caStatus casAsyncPVExistIO::postIOCompletion (const pvExistReturn &retValIn)
{
	this->retVal = retValIn; 
	return this->postIOCompletionI ();
}

//
// casAsyncPVExistIO::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
epicsShareFunc caStatus casAsyncPVExistIO::cbFuncAsyncIO()
{
    caStatus 	status;
    
    if (this->msg.m_cmmd==CA_PROTO_SEARCH) {
        //
        // pass output DG address parameters
        //
        status = this->client.asyncSearchResponse (
            this->dgOutAddr, this->msg, this->retVal,
            this->protocolRevision, this->sequenceNumber );
    }
    else {
        errPrintf (S_cas_invalidAsynchIO, __FILE__, __LINE__,
            " - client request type = %u", this->msg.m_cmmd);
		status = S_cas_invalidAsynchIO;
    }
    
    return status;
}

//
// void casAsyncPVExistIO::destroy ()
//
void casAsyncPVExistIO::destroy ()
{
    delete this;
}

