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
#include "casAsyncIOIIL.h" // casAsyncIOI in line func
#include "casChannelIIL.h" // casChannelI in line func
#include "casCtxIL.h" // casCtxI in line func

//
// casAsyncReadIO::casAsyncReadIO()
//
casAsyncReadIO::casAsyncReadIO(const casCtx &ctx) :
	casAsyncIOI(*ctx.getClient()),
	msg(*ctx.getMsg()), 
	chan(*ctx.getChannel()), 
	pDD(NULL),
	completionStatus(S_cas_internal)
{
	assert (&this->chan);

	this->chan.installAsyncIO(*this);
}

//
// casAsyncReadIO::~casAsyncReadIO()
//
casAsyncReadIO::~casAsyncReadIO()
{
	this->lock();

	this->chan.removeAsyncIO(*this);

	this->unlock();
}

//
// casAsyncReadIO::postIOCompletion()
//
caStatus casAsyncReadIO::postIOCompletion(caStatus completionStatusIn,
				gdd &valueRead)
{
	this->lock();
	this->pDD = &valueRead;
	this->unlock();

	this->completionStatus = completionStatusIn;
	return this->postIOCompletionI();
}

//
// casAsyncReadIO::readOP()
//
epicsShareFunc bool casAsyncReadIO::readOP() const
{
	return true; // it is a read op
}

//
// casAsyncReadIO::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
epicsShareFunc caStatus casAsyncReadIO::cbFuncAsyncIO()
{
	caStatus 	status;

	switch (this->msg.m_cmmd) {
	case CA_PROTO_READ:
		status = client.readResponse(&this->chan, this->msg,
				*this->pDD, this->completionStatus);
		break;

	case CA_PROTO_READ_NOTIFY:
		status = client.readNotifyResponse(&this->chan, 
				this->msg, this->pDD, 
				this->completionStatus);
		break;

	case CA_PROTO_EVENT_ADD:
		status = client.monitorResponse(this->chan,
				this->msg, this->pDD,
				this->completionStatus);
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
// void casAsyncReadIO::destroy ()
//
void casAsyncReadIO::destroy ()
{
    delete this;
}