
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

#define epicsExportSharedSymbols
#include "casAsyncPVAttachIOI.h"

casAsyncPVAttachIOI::casAsyncPVAttachIOI (
        casAsyncPVAttachIO & intf, const casCtx & ctx ) :
	casAsyncIOI ( ctx ), msg ( *ctx.getMsg() ),
    asyncPVAttachIO ( intf ), retVal ( S_cas_badParameter )
{
    ctx.getClient()->installAsynchIO ( *this );
}

caStatus casAsyncPVAttachIOI::postIOCompletion ( const pvAttachReturn & retValIn )
{
	this->retVal = retValIn; 
	return this->insertEventQueue ();
}

caStatus casAsyncPVAttachIOI::cbFuncAsyncIO ( 
    epicsGuard < casClientMutex > & guard )
{
	caStatus 	status;

	switch ( this->msg.m_cmmd ) {
	case CA_PROTO_CREATE_CHAN:
		status = this->client.createChanResponse ( guard,
                        this->msg, this->retVal );
        if ( status == S_cas_sendBlocked ) {
            return status;
        }
		break;  

	default:
        errPrintf ( S_cas_invalidAsynchIO, __FILE__, __LINE__,
            " - client request type = %u", this->msg.m_cmmd );
		status = S_cas_invalidAsynchIO;
		break;
	}

    this->client.uninstallAsynchIO ( *this );

	return status;
}

