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
#include "casChannelIIL.h" // casChannelI in line func
#include "casCtxIL.h" // casCtx in line func

//
// casAsyncWriteIO::casAsyncWriteIO()
//
casAsyncWriteIO::casAsyncWriteIO(const casCtx &ctx) :
	casAsyncIOI(*ctx.getClient()),
	msg(*ctx.getMsg()), 
	chan(*ctx.getChannel()),
	completionStatus(S_cas_internal)
{
	assert (&this->chan);
	this->chan.installAsyncIO(*this);
}

//
// casAsyncWriteIO::~casAsyncWriteIO()
//
casAsyncWriteIO::~casAsyncWriteIO()
{
    epicsGuard < casCoreClient > guard ( this->client );
	this->chan.removeAsyncIO(*this);
}

//
// casAsyncWriteIO::postIOCompletion()
//
caStatus casAsyncWriteIO::postIOCompletion(caStatus completionStatusIn)
{
	this->completionStatus = completionStatusIn;
	return this->postIOCompletionI();
}

//
// casAsyncWriteIO::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
epicsShareFunc caStatus casAsyncWriteIO::cbFuncAsyncIO()
{
    caStatus 	status;
    
    switch (this->msg.m_cmmd) {
    case CA_PROTO_WRITE:
        status = client.writeResponse(this->msg,
            this->completionStatus);
        break;
        
    case CA_PROTO_WRITE_NOTIFY:
        status = client.writeNotifyResponse(
            this->msg, this->completionStatus);
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
// void casAsyncWriteIO::destroy ()
//
void casAsyncWriteIO::destroy ()
{
    delete this;
}
