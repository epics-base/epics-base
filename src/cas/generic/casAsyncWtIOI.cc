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
 * Revision 1.5  1998/10/28 23:51:00  jhill
 * server nolonger throws exception when a poorly formed get/put call back
 * request arrives. Instead a get/put call back response is sent which includes
 * unsuccessful status
 *
 * Revision 1.4  1997/08/05 00:47:02  jhill
 * fixed warnings
 *
 * Revision 1.3  1997/04/10 19:33:57  jhill
 * API changes
 *
 * Revision 1.2  1996/11/06 22:15:56  jhill
 * allow monitor init read to using rd async io
 *
 * Revision 1.1  1996/11/02 01:01:04  jhill
 * installed
 *
 *
 */


#include "server.h"
#include "casAsyncIOIIL.h" // casAsyncIOI in line func
#include "casChannelIIL.h" // casChannelI in line func
#include "casCtxIL.h" // casCtx in line func

//
// casAsyncWtIOI::casAsyncWtIOI()
//
epicsShareFunc casAsyncWtIOI::casAsyncWtIOI(const casCtx &ctx, casAsyncWriteIO &ioIn) :
	casAsyncIOI(*ctx.getClient(), ioIn),
	msg(*ctx.getMsg()), 
	chan(*ctx.getChannel()),
	completionStatus(S_cas_internal)
{
	assert (&this->msg);
	assert (&this->chan);

	this->chan.installAsyncIO(*this);
}

//
// casAsyncWtIOI::~casAsyncWtIOI()
//
casAsyncWtIOI::~casAsyncWtIOI()
{
	this->lock();
	this->chan.removeAsyncIO(*this);
	this->unlock();
}

//
// casAsyncWtIOI::postIOCompletion()
//
epicsShareFunc caStatus casAsyncWtIOI::postIOCompletion(caStatus completionStatusIn)
{
	this->completionStatus = completionStatusIn;
	return this->postIOCompletionI();
}


//
// casAsyncWtIOI::cbFuncAsyncIO()
// (called when IO completion event reaches top of event queue)
//
caStatus casAsyncWtIOI::cbFuncAsyncIO()
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
			this->reportInvalidAsynchIO(this->msg.m_cmmd);
			status = S_cas_internal;
			break;
        }

	return status;
}

