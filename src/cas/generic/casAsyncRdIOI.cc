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
 * Revision 1.3  1996/09/04 20:13:16  jhill
 * initialize new member - asyncIO
 *
 * Revision 1.2  1996/06/26 21:18:50  jhill
 * now matches gdd api revisions
 *
 * Revision 1.1.1.1  1996/06/20 00:28:14  jhill
 * ca server installation
 *
 *
 */


#include <server.h>
#include <casAsyncIOIIL.h> // casAsyncIOI in line func
#include <casChannelIIL.h> // casChannelI in line func
#include <casCtxIL.h> // casCtxI in line func

//
// casAsyncRdIOI::casAsyncRdIOI()
//
casAsyncRdIOI::casAsyncRdIOI(const casCtx &ctx, casAsyncReadIO &ioIn) :
	casAsyncIOI(*ctx.getClient(), ioIn),
	msg(*ctx.getMsg()), 
	chan(*ctx.getChannel()), 
	pDD(NULL),
	completionStatus(S_cas_internal)
{
	assert (&this->msg);
	assert (&this->chan);

	if (this->msg.m_cmmd != CA_PROTO_READ &&
		this->msg.m_cmmd != CA_PROTO_READ_NOTIFY) {
		this->reportInvalidAsynchIO(this->msg.m_cmmd);
	}

	this->chan.installAsyncIO(*this);
}

//
// casAsyncRdIOI::~casAsyncRdIOI()
//
casAsyncRdIOI::~casAsyncRdIOI()
{
	int gddStatus;
	
	this->lock();

	this->chan.removeAsyncIO(*this);

	if (this->pDD) {
		gddStatus = this->pDD->unreference();
		assert(!gddStatus);
	}

	this->unlock();
}

//
// casAsyncRdIOI::postIOCompletion()
//
caStatus casAsyncRdIOI::postIOCompletion(caStatus completionStatusIn,
				gdd &valueRead)
{
	int gddStatus;

	this->lock();
	this->pDD = &valueRead;
	gddStatus = this->pDD->reference();
	this->unlock();

	assert(!gddStatus);

	this->completionStatus = completionStatusIn;
	return this->postIOCompletionI();
}

//
// casAsyncRdIOI::readOP()
//
int casAsyncRdIOI::readOP()
{
	return TRUE; // it is a read op
}


//
// casAsyncRdIOI::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
caStatus casAsyncRdIOI::cbFuncAsyncIO()
{
	caStatus 	status;

	if (this->msg.m_cmmd == CA_PROTO_READ) {
		status = client.readResponse(&this->chan, this->msg,
				this->pDD, this->completionStatus);
	}
	else if (this->msg.m_cmmd == CA_PROTO_READ_NOTIFY) {
		status = client.readNotifyResponse(&this->chan, 
				this->msg, this->pDD, 
				this->completionStatus);
	}
	else {
		status = S_cas_internal;
	}

	return status;
}

