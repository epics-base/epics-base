/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#include <string>
#include <stdexcept>

#include "errlog.h"

#define epicsExportSharedSymbols
#include "casAsyncIOI.h"

casAsyncIOI::casAsyncIOI ( const casCtx & ctx ) :
	client ( *ctx.getClient() ), inTheEventQueue ( false ),
	posted ( false ), ioComplete ( false )
{
	//
	// catch situation where they create more than one
	// async IO object per request
	//
	if ( ! client.okToStartAsynchIO () ) {
        throw std::logic_error ( 
            "server tool attempted to "
            "start duplicate asynchronous IO" );
	}
}

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
	//
	// pulls itself out of the event queue
	// if it is installed there. 
	//
	this->client.removeFromEventQueue ( *this, this->inTheEventQueue );
}

//
// o called when IO completion event reaches top of event queue
// o clients lock is applied when calling this
//
caStatus casAsyncIOI::cbFunc ( 
    casCoreClient &, 
    epicsGuard < casClientMutex > & clientGuard,
    epicsGuard < evSysMutex > & )
{
    caStatus status = S_cas_success;
    {
	    this->inTheEventQueue = false;

	    status = this->cbFuncAsyncIO ( clientGuard );
	    if ( status == S_cas_sendBlocked ) {
		    //
		    // causes this op to be pushed back on the queue 
		    //
		    this->inTheEventQueue = true;
		    return status;
	    }
	    else if ( status != S_cas_success ) {
		    errMessage ( status, "Asynch IO completion failed" );
	    }

	    this->ioComplete = true;
    }

	// dont use "this" after destroying the object here
	delete this;

	return S_cas_success;
}

caStatus casAsyncIOI::insertEventQueue ()
{
	//
	// place this event in the event queue
	// o this also signals the event consumer
    // o clients lock protects list and flag
	//
	return this->client.addToEventQueue ( *this, this->inTheEventQueue, this->posted );
}

caServer *casAsyncIOI::getCAS() const
{
	return this->client.getCAS().getAdapter();
}

bool casAsyncIOI::oneShotReadOP() const
{
	return false;
}
