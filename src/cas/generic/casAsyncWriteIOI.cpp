
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
#include "casAsyncWriteIOI.h"
#include "casChannelI.h"

casAsyncWriteIOI::casAsyncWriteIOI (     
    casAsyncWriteIO & intf, const casCtx & ctx ) :
	casAsyncIOI ( ctx ),
	msg ( *ctx.getMsg() ), 
    asyncWriteIO ( intf ),
	chan ( *ctx.getChannel() ),
	completionStatus ( S_cas_internal )
{
    this->chan.installIO ( *this );
}

caStatus casAsyncWriteIOI::postIOCompletion ( caStatus completionStatusIn )
{
	this->completionStatus = completionStatusIn;
	return this->insertEventQueue ();
}

caStatus casAsyncWriteIOI::cbFuncAsyncIO (
    epicsGuard < casClientMutex > & guard )
{
    caStatus status;
    
    switch ( this->msg.m_cmmd ) {
    case CA_PROTO_WRITE:
        status = client.writeResponse ( guard, this->chan,
            this->msg, this->completionStatus );
        break;
        
    case CA_PROTO_WRITE_NOTIFY:
        status = client.writeNotifyResponse ( guard, this->chan,
            this->msg, this->completionStatus );
        break;
        
    default:
        errPrintf ( S_cas_invalidAsynchIO, __FILE__, __LINE__,
            " - client request type = %u", this->msg.m_cmmd );
		status = S_cas_invalidAsynchIO;
        break;
    }

    if ( status != S_cas_sendBlocked ) {
        this->chan.uninstallIO ( *this );
    }
    
    return status;
}


