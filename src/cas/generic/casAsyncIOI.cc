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
 * Revision 1.2  1996/06/26 21:18:50  jhill
 * now matches gdd api revisions
 *
 * Revision 1.1.1.1  1996/06/20 00:28:14  jhill
 * ca server installation
 *
 *
 */


#include <server.h>
#include <casAsyncIOIIL.h> // casAsyncIOI inline func
#include <casChannelIIL.h> // casChannelI inline func
#include <casEventSysIL.h> // casEventSys inline func

//
// casAsyncIOI::casAsyncIOI()
//
casAsyncIOI::casAsyncIOI(const casCtx &ctx, casAsyncIO &ioIn, gdd *pDD) :
	msg(*ctx.getMsg()), 
	client(*ctx.getClient()), 
	asyncIO(ioIn),
	pChan(ctx.getChannel()), 
	pDesc(pDD),
	completionStatus(S_cas_internal),
	inTheEventQueue(FALSE),
	posted(FALSE),
	ioComplete(FALSE),
	serverDelete(FALSE)
{
	assert (&this->client);
	assert (&this->msg);
	assert (&this->asyncIO);

	if (this->pChan) {
		this->pChan->installAsyncIO(*this);
	}
	else {
		this->client.installAsyncIO(*this);
	}

	if (this->pDesc) {
		int gddStatus;

		gddStatus = this->pDesc->reference();
		assert(!gddStatus);
	}
}

//
// casAsyncIOI::~casAsyncIOI()
//
// ways this gets destroyed:
// 1) io completes, this is pulled off the queue, and result
// 	is sent to the client
// 2) client, channel, or PV is deleted
// 3) server tool deletes the casAsyncIO obj
//
// Case 1)  => normal completion
//
// Case 2)
// If the server deletes the channel or the PV then the
// client will get a disconnect msg for the channel 
// involved and this will cause the io call back
// to be called with a disconnect error code.
// Therefore we dont need to force an IO canceled 
// response here.
//
// Case 3)
// If for any reason the server tool needs to cancel an IO
// operation then it should post io completion with status
// S_casApp_canceledAsyncIO. Deleting the asyncronous io
// object prior to its being allowed to forward an IO termination 
// message to the client will result in NO IO CALL BACK TO THE
// CLIENT PROGRAM (in this situation a warning message will be printed by 
// the server lib).
//
casAsyncIOI::~casAsyncIOI()
{
	
	this->lock();

	if (!this->serverDelete) {
		fprintf(stderr, 
	"WARNING: An async IO operation was deleted prematurely\n");
		fprintf(stderr, 
	"WARNING: by the server tool. This results in no IO cancel\n");
		fprintf(stderr, 
	"WARNING: message being sent to the client. PLease cancel\n");
		fprintf(stderr, 
	"WARNING: IO by posting S_casApp_canceledAsyncIO instead of\n");
		fprintf(stderr, 
	"WARNING: by deleting the async IO object.\n");
	}

	if (this->pChan) {
		this->pChan->removeAsyncIO(*this);
	}
	else {
		this->client.removeAsyncIO(*this);
	}

	//
	// pulls itself out of the event queue
	// if it is installed there
	//
	if (this->inTheEventQueue) {
		this->client.casEventSys::removeFromEventQueue(*this);
	}

	if (this->pDesc) {
		int	gddStatus;

		gddStatus = this->pDesc->unreference();
		assert (gddStatus==0);
		this->pDesc = NULL;
	}

	this->unlock();
}


//
// casAsyncIOI::cbFunc()
// (called when IO completion event reaches top of event queue)
//
caStatus casAsyncIOI::cbFunc(class casEventSys &)
{
	caStatus 		status;

	this->lock();

	this->inTheEventQueue = FALSE;

	status = this->client.asyncIOCompletion(this->pChan,
			this->msg, this->pDesc, this->completionStatus);

        if (status == S_cas_sendBlocked) {
		//
		// causes this op to be pushed back on the queue 
		//
		this->inTheEventQueue = TRUE;
		this->unlock();
                return status;
        }
        else if (status != S_cas_success) {
                errMessage (status, "Asynch IO completion failed");
        }

	//
	// dont use "this" after this delete
	//
	this->ioComplete = TRUE;

	this->unlock();

	this->setServerDelete();
	(*this)->destroy();

	return S_cas_success;
}


//
// casAsyncIOI::postIOCompletion()
//
caStatus casAsyncIOI::postIOCompletion(caStatus completionStatusIn, gdd *pValue)
{
	this->lock();

	//
	// verify that they dont post completion more than once
	//
	if (this->posted) {
		this->unlock();
		return S_cas_redundantPost;
	}

	//
	// dont call the server tool's cancel() when this object deletes 
	//
	this->posted = TRUE;
	this->completionStatus = completionStatusIn;

	if (pValue) {
		int gddStatus;

		gddStatus = pValue->reference();
		assert(!gddStatus);

		if (this->pDesc) {
			gddStatus = this->pDesc->unreference();
			assert (gddStatus==0);
		}

		this->pDesc = pValue;
	}

	//
	// No changes after the IO completes
	//
	if (this->pDesc) {
		pDesc->markConstant();
	}

	//
	// place this event in the event queue
	// (this also signals the event consumer)
	//
	this->inTheEventQueue = TRUE;
	this->client.casEventSys::addToEventQueue(*this);

	this->unlock();

	return S_cas_success;
}

//
// casAsyncIOI::getCAS()
// (not inline because this is used by the interface class)
//
caServer *casAsyncIOI::getCAS()
{
        return this->client.getCAS().getAdapter();
}

