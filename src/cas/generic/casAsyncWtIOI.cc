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
 * Revision 1.1  1996/11/02 01:01:04  jhill
 * installed
 *
 *
 */


#include <server.h>
#include <casAsyncIOIIL.h> // casAsyncIOI in line func
#include <casChannelIIL.h> // casChannelI in line func
#include <casCtxIL.h> // casCtx in line func

//
// casAsyncWtIOI::casAsyncWtIOI()
//
casAsyncWtIOI::casAsyncWtIOI(const casCtx &ctx, casAsyncWriteIO &ioIn) :
	casAsyncIOI(*ctx.getClient(), ioIn),
	msg(*ctx.getMsg()), 
	chan(*ctx.getChannel()),
	completionStatus(S_cas_internal)
{
	assert (&this->msg);
	assert (&this->chan);

	this->chan.installAsyncIO(*this);
}

//
// casAsyncWtIOI::~casAsyncWtIOI()
//
casAsyncWtIOI::~casAsyncWtIOI()
{
	this->lock();
	this->chan.removeAsyncIO(*this);
	this->unlock();
}

//
// casAsyncWtIOI::postIOCompletion()
//
caStatus casAsyncWtIOI::postIOCompletion(caStatus completionStatusIn)
{
	this->completionStatus = completionStatusIn;
	return this->postIOCompletionI();
}


//
// casAsyncWtIOI::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
caStatus casAsyncWtIOI::cbFuncAsyncIO()
{
	caStatus 	status;

        switch (this->msg.m_cmmd) {
        case CA_PROTO_WRITE:
		status = client.writeResponse(&this->chan, this->msg,
				this->completionStatus);
                break;
 
        case CA_PROTO_WRITE_NOTIFY:
		status = client.writeNotifyResponse(&this->chan, 
				this->msg, this->completionStatus);
                break;
 
        default:
                this->reportInvalidAsynchIO(this->msg.m_cmmd);
                status = S_cas_internal;
                break;
        }

	return status;
}

