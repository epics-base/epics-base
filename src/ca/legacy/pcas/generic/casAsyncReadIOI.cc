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

#include "errlog.h"

#define epicsExportSharedSymbols
#include "casAsyncReadIOI.h"
#include "casChannelI.h"

casAsyncReadIOI::casAsyncReadIOI ( 
    casAsyncReadIO & intf, const casCtx & ctx ) :
	casAsyncIOI ( ctx ), msg ( *ctx.getMsg() ), 
    asyncReadIO ( intf ), chan ( *ctx.getChannel () ), 
    pDD ( NULL ), completionStatus ( S_cas_internal )
{
    this->chan.installIO ( *this );
}

casAsyncReadIOI::~casAsyncReadIOI ()
{
    this->asyncReadIO.serverInitiatedDestroy ();
}

caStatus casAsyncReadIOI::postIOCompletion ( 
    caStatus completionStatusIn, const gdd & valueRead )
{
	this->pDD = & valueRead;
	this->completionStatus = completionStatusIn;
	return this->insertEventQueue ();
}

bool casAsyncReadIOI::oneShotReadOP () const
{
	return true; // it is a read op
}

caStatus casAsyncReadIOI::cbFuncAsyncIO (
    epicsGuard < casClientMutex > & guard )
{
	caStatus status;

    // uninstall the io early on to prevent a channel delete from
    // destroying this object twice
    this->chan.uninstallIO ( *this );

	switch ( this->msg.m_cmmd ) {
	case CA_PROTO_READ:
		status = client.readResponse ( 
            guard, & this->chan, this->msg,
			* this->pDD, this->completionStatus );
		break;

	case CA_PROTO_READ_NOTIFY:
		status = client.readNotifyResponse ( 
            guard, & this->chan, this->msg, * this->pDD, 
			this->completionStatus );
        break;

	case CA_PROTO_EVENT_ADD:
		status = client.monitorResponse ( 
            guard, this->chan, this->msg, * this->pDD,
			this->completionStatus );
		break;

	case CA_PROTO_CREATE_CHAN:
        // we end up here if the channel claim protocol response is delayed
        // by an asynchronous enum string table fetch response
        status = client.enumPostponedCreateChanResponse ( 
                    guard, this->chan, this->msg );
        if ( status == S_cas_success ) {
            if ( this->completionStatus == S_cas_success && this->pDD.valid() ) {
                this->chan.getPVI().updateEnumStringTableAsyncCompletion ( *this->pDD );
            }
            else {
                errMessage ( this->completionStatus, 
                    "unable to read application type \"enums\" string"
                    " conversion table for enumerated PV" );
            }
        }
        break;

	default:
        errPrintf ( S_cas_invalidAsynchIO, __FILE__, __LINE__,
            " - client request type = %u", this->msg.m_cmmd );
		status = S_cas_invalidAsynchIO;
		break;
	}

    if ( status == S_cas_sendBlocked ) {
        this->chan.installIO ( *this );
    }

	return status;
}


