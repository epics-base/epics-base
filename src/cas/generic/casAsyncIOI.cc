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
#include "caServerIIL.h" // caServerI in line func
#include "casCoreClientIL.h" // casCoreClient in line func
#include "casEventSysIL.h" // casEventSys in line func
#include "casAsyncIOIIL.h" // casAsyncIOI in line func
#include "casCtxIL.h" // casCtx in line func

//
// casAsyncIOI::casAsyncIOI()
//
casAsyncIOI::casAsyncIOI (casCoreClient &clientIn) :
	client (clientIn),
	inTheEventQueue (FALSE),
	posted (FALSE),
	ioComplete (FALSE),
	serverDelete (FALSE)
{
	//
	// catch situation where they create more than one
	// async IO object per request
	//
	if (client.asyncIOFlag) {
		errMessage(S_cas_badParameter, 
			"- duplicate async IO creation");
		this->duplicate = TRUE;
	}
	else {
		client.asyncIOFlag = TRUE;
		this->duplicate = FALSE;
	}
}

//
// casAsyncIOI::~casAsyncIOI()
//
// ways this gets destroyed:
// 1) io completes, this is pulled off the queue, and result
// 	is sent to the client
// 2) client, channel, or PV is deleted
// 3) server tool deletes the casAsyncXxxxIO obj
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
	"WARNING: message being sent to the client. Please cancel\n");
		fprintf(stderr, 
	"WARNING: IO by posting S_casApp_canceledAsyncIO instead of\n");
		fprintf(stderr, 
	"WARNING: by deleting the async IO object.\n");
	}

	//
	// pulls itself out of the event queue
	// if it is installed there
	//
	if (this->inTheEventQueue) {
		this->client.casEventSys::removeFromEventQueue(*this);
	}

	this->unlock();
}

//
// casAsyncIOI::cbFunc()
// (called when IO completion event reaches top of event queue)
//
caStatus casAsyncIOI::cbFunc(class casEventSys &)
{
	casCoreClient	&theClient = this->client;
	caStatus 	status;

	//
	// Use the client's lock here (which is the same as the
	// asynch IO's lock) here because we need to leave the lock
	// applied around the destroy() call here.
	//
	theClient.lock();

	this->inTheEventQueue = FALSE;

	status = this->cbFuncAsyncIO();

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

	this->ioComplete = TRUE;

	//
	// dont use "this" after potentially destroying the
	// object here
	//
	this->serverDestroy();

	theClient.unlock();

	return S_cas_success;
}

//
// casAsyncIOI::postIOCompletionI()
//
caStatus casAsyncIOI::postIOCompletionI()
{
	//
	// detect the case where the server called destroy(), 
	// the server tool postponed deletion of the object, 
	// and then it called postIOCompletion() on this object 
	// when it was currently not in use by the server.
	//
	if (this->serverDelete) {
		return S_cas_redundantPost;
	}

	this->lock();

	if (this->duplicate) {
		errMessage(S_cas_badParameter, 
			"- duplicate async IO");
		//
		// dont use "this" after potentially destroying the
		// object here
		//
		this->unlock();
		this->serverDestroy();
		return S_cas_redundantPost;
	}

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
caServer *casAsyncIOI::getCAS() const
{
	return this->client.getCAS().getAdapter();
}

//
// casAsyncIOI::readOP()
//
epicsShareFunc bool casAsyncIOI::readOP() const
{
	//
	// not a read op
	//
	return false;
}

//
// casAsyncIOI::serverDestroyIfReadOP()
//
void casAsyncIOI::serverDestroyIfReadOP()
{
    casCoreClient &clientCopy = this->client;
    
    //
    // client lock used because this object's
    // lock may be destroyed
    //
    clientCopy.lock();
    
    if (this->readOP()) {
        this->serverDestroy();
    }
    
    //
    // NO REF TO THIS OBJECT BELOW HERE
    // BECAUSE OF THE DELETE ABOVE
    //
    
    clientCopy.unlock();
}

//
// void casAsyncIOI::serverDestroy ()
//
void casAsyncIOI::serverDestroy ()
{
	this->serverDelete = TRUE;
	this->destroy();
}

//
// void casAsyncIOI::destroy ()
//
void casAsyncIOI::destroy ()
{
    delete this;
}
