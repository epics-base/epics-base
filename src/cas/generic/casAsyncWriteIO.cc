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
#include "casChannelIIL.h" // casChannelI in line func
#include "casAsyncIOIIL.h" // casAsyncIOI in line func
#include "casCtxIL.h" // casCtx in line func

//
// casAsyncWriteIO::casAsyncWriteIO()
//
casAsyncWriteIO::casAsyncWriteIO(const casCtx &ctx) :
	casAsyncIOI(*ctx.getClient()),
	msg(*ctx.getMsg()), 
	chan(*ctx.getChannel()),
	completionStatus(S_cas_internal)
{
	assert (&this->chan);
	this->chan.installAsyncIO(*this);
}

//
// casAsyncWriteIO::~casAsyncWriteIO()
//
casAsyncWriteIO::~casAsyncWriteIO()
{
	this->lock();
	this->chan.removeAsyncIO(*this);
	this->unlock();
}

//
// casAsyncWriteIO::postIOCompletion()
//
caStatus casAsyncWriteIO::postIOCompletion(caStatus completionStatusIn)
{
	this->completionStatus = completionStatusIn;
	return this->postIOCompletionI();
}

//
// casAsyncWriteIO::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
epicsShareFunc caStatus casAsyncWriteIO::cbFuncAsyncIO()
{
    caStatus 	status;
    
    switch (this->msg.m_cmmd) {
    case CA_PROTO_WRITE:
        status = client.writeResponse(this->msg,
            this->completionStatus);
        break;
        
    case CA_PROTO_WRITE_NOTIFY:
        status = client.writeNotifyResponse(
            this->msg, this->completionStatus);
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
// void casAsyncWriteIO::destroy ()
//
void casAsyncWriteIO::destroy ()
{
    delete this;
}
