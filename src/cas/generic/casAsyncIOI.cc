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
#include "caServerIIL.h" // caServerI in line func
#include "casCoreClientIL.h" // casCoreClient in line func
#include "casEventSysIL.h" // casEventSys in line func
#include "casCtxIL.h" // casCtx in line func

//
// casAsyncIOI::casAsyncIOI()
//
casAsyncIOI::casAsyncIOI ( const casCtx & ctx ) :
	client ( *ctx.getClient() ), inTheEventQueue ( false ),
	posted ( false ), ioComplete ( false ), 
    serverDelete ( false ), duplicate ( false )
{
	//
	// catch situation where they create more than one
	// async IO object per request
	//
	if ( ! client.okToStartAsynchIO () ) {
		errMessage ( S_cas_badParameter, 
			"- duplicate async IO creation" );
		this->duplicate = true;
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
	if ( ! this->serverDelete ) {
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
	// if it is installed there. 
	//
	this->client.removeFromEventQueue ( *this );
}

//
// casAsyncIOI::cbFunc()
// o called when IO completion event reaches top of event queue
// o clients lock is applied when calling this
//
caStatus casAsyncIOI::cbFunc ( casCoreClient & )
{
	this->inTheEventQueue = false;

	caStatus status = this->cbFuncAsyncIO();

	if (status == S_cas_sendBlocked) {
		//
		// causes this op to be pushed back on the queue 
		//
		this->inTheEventQueue = true;
		return status;
	}
	else if (status != S_cas_success) {
		errMessage (status, "Asynch IO completion failed");
	}

	this->ioComplete = true;

	//
	// dont use "this" after potentially destroying the
	// object here
	//
	this->serverDestroy();

	return S_cas_success;
}

void casAsyncIOI::eventSysDestroyNotify ( casCoreClient & )
{
	this->serverDestroy();
}

//
// casAsyncIOI::postIOCompletionI ()
//
caStatus casAsyncIOI::postIOCompletionI ()
{
	//
	// detect the case where the server called destroy(), 
	// the server tool postponed deletion of the object, 
	// and then it called postIOCompletion() on this object 
	// when it was currently not in use by the server.
	//
	if ( this->serverDelete ) {
		return S_cas_redundantPost;
	}

    // this is only touched in the constructor so its 
    // ok not to not take a lock here
	if ( this->duplicate ) {
		errMessage ( S_cas_badParameter, 
			"- duplicate async IO");
		//
		// dont use "this" after potentially destroying the
		// object here
		//
		this->serverDestroy();
		return S_cas_redundantPost;
	}

	//
	// place this event in the event queue
	// o this also signals the event consumer
    // o clients lock protects list and flag
	//
	return this->client.addToEventQueue ( *this, this->inTheEventQueue, this->posted );

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
bool casAsyncIOI::readOP() const
{
	//
	// not a read op
	//
	return false;
}

//
// casAsyncIOI::serverDestroyIfReadOP()
// o clients lock is held when this is called
//
void casAsyncIOI::serverDestroyIfReadOP()
{    
    if ( this->readOP() ) {
        this->serverDestroy();
    }
    
    //
    // NO REF TO THIS OBJECT BELOW HERE
    // BECAUSE OF THE DELETE ABOVE
    //
}

//
// void casAsyncIOI::serverDestroy ()
//
void casAsyncIOI::serverDestroy ()
{
	this->serverDelete = true;
	this->destroy();
}

//
// void casAsyncIOI::destroy ()
//
void casAsyncIOI::destroy ()
{
    delete this;
}
