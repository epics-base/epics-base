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
 * Revision 1.3  1996/09/16 18:23:59  jhill
 * vxWorks port changes
 *
 * Revision 1.2  1996/08/13 22:53:14  jhill
 * changes for MVC++
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#include <server.h>
#include <caServerIIL.h>	// caServerI in line func
#include <casAsyncIOIIL.h>	// casAsyncIOI in line func
#include <casEventSysIL.h>	// casEventSys in line func
#include <casCtxIL.h>		// casCtx in line func
#include <inBufIL.h>		// inBuf in line func
#include <outBufIL.h>		// outBuf in line func

//
// casCoreClient::init()
//
caStatus casCoreClient::init()
{
        if (this->osiMutex::init()) {
                return S_cas_noMemory;
        }
        return this->casEventSys::init();
}

//
// static declartions for class casCoreClient
//
//int casCoreClient::msgHandlersInit;
//pAsyncIoCallBack casCoreClient::asyncIOJumpTable[CA_PROTO_LAST_CMMD+1u];
 

//
// casCoreClient::casCoreClient()
//
casCoreClient::casCoreClient(caServerI &serverInternal) : casEventSys(*this)
{
	assert(&serverInternal);
	ctx.setServer(&serverInternal);
	ctx.setClient(this);

	//
	// static member init
	//
	//casCoreClient::loadProtoJumpTable();
}

#if 0
//
// casCoreClient::loadProtoJumpTable()
//
void casCoreClient::loadProtoJumpTable()
{
        //
        // Load the static protocol handler tables
        //
        if (casCoreClient::msgHandlersInit) {
                return;
        }
 
        //
        // Asynch Response Protocol Jump Table
        //
        casCoreClient::asyncIOJumpTable[CA_PROTO_SEARCH] =
                        casCoreClient::searchResponse;
        casCoreClient::asyncIOJumpTable[CA_PROTO_CLAIM_CIU] =
                        casCoreClient::createChanResponse;
        casCoreClient::asyncIOJumpTable[CA_PROTO_READ] =
                        casCoreClient::readResponse;
        casCoreClient::asyncIOJumpTable[CA_PROTO_READ_NOTIFY] =
                        casCoreClient::readNotifyResponse;
        casCoreClient::asyncIOJumpTable[CA_PROTO_WRITE] =
                        casCoreClient::writeResponse;
        casCoreClient::asyncIOJumpTable[CA_PROTO_WRITE_NOTIFY] =
                        casCoreClient::writeNotifyResponse;
        casCoreClient::asyncIOJumpTable[CA_PROTO_EVENT_ADD] =
                        casCoreClient::monitorResponse;

        casCoreClient::msgHandlersInit = TRUE;
}
#endif
 

//
// casCoreClient::~casCoreClient()
//
casCoreClient::~casCoreClient()
{
	casAsyncIOI		*pCurIO;

        if (this->ctx.getServer()->getDebugLevel()>0u) {
                ca_printf ("CAS: Connection Terminated\n");
        }

	this->osiLock();
	tsDLFwdIter<casAsyncIOI>   iterIO(this->ioInProgList);

        //
        // cancel any pending asynchronous IO
        //
        pCurIO = iterIO.next();
        while (pCurIO) {
                casAsyncIOI     *pNextIO;
                //
                // destructor removes from this list
                //
                pNextIO = iterIO.next();
		pCurIO->destroy();
                pCurIO = pNextIO;
        }

	this->osiUnlock();
}

//
// casCoreClient::destroy()
//
void casCoreClient::destroy()
{
	delete this;
}

//
// casCoreClient::disconnectChan()
//
caStatus casCoreClient::disconnectChan(caResId)
{
	printf ("Disconnect Chan issued for inappropriate client type?\n");
	return S_cas_success;
}

void casCoreClient::show (unsigned level) const
{
	printf ("Core client\n");
	this->casEventSys::show (level);
	printf ("\t%d io ops in progess\n", this->ioInProgList.count());
	this->ctx.show (level);
}

//
// one of these for each CA request type that has
// asynchronous completion
//
caStatus casCoreClient::asyncSearchResponse(casDGIntfIO &,
		const caAddr &, const caHdr &, const pvExistReturn &)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::createChanResponse(const caHdr &, const pvExistReturn &)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readResponse(casChannelI *, const caHdr &, 
	gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readNotifyResponse(casChannelI *, const caHdr &, 
	gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeResponse(casChannelI *, const caHdr &, 
	const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeNotifyResponse(casChannelI *, const caHdr &, 
	const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::monitorResponse(casChannelI *, const caHdr &, 
	gdd *, const caStatus)
{
	return S_casApp_noSupport;
}

//
// casCoreClient::installChannel()
//
void casCoreClient::installChannel(casChannelI &)
{
	assert(0); // dont install channels on the wrong type of client
}
 
//
// casCoreClient::removeChannel()
//
void casCoreClient::removeChannel(casChannelI &)
{
	assert(0); // dont install channels on the wrong type of client
}

//
// casCoreClient::fetchRespAddr()
//
caAddr casCoreClient::fetchRespAddr()
{
	caAddr	addr;
	memset (&addr, '\0', sizeof(addr));
	return addr;
}

//
// casCoreClient::fetchOutIntf()
//
casDGIntfIO* casCoreClient::fetchOutIntf()
{
	return NULL;
}

