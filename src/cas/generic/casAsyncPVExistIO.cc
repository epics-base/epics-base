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
// casAsyncPVExistIO::casAsyncPVExistIO()
//
casAsyncPVExistIO::casAsyncPVExistIO (const casCtx &ctx) :
	casAsyncIOI ( *ctx.getClient () ),
	msg ( *ctx.getMsg () ),
	retVal (pverDoesNotExistHere),
	dgOutAddr ( ctx.getClient ()->fetchLastRecvAddr () )
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
            this->dgOutAddr, this->msg, this->retVal);
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

