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
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#include <server.h>
#include <caServerIIL.h>

//
// static declartions for class casCoreClient
//
int casCoreClient::msgHandlersInit;
pAsyncIoCallBack casCoreClient::asyncIOJumpTable[CA_PROTO_LAST_CMMD+1u];
 

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
	casCoreClient::loadProtoJumpTable();
}

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
 

//
// casCoreClient::~casCoreClient()
//
casCoreClient::~casCoreClient()
{
	tsDLIter<casAsyncIOI>   iterIO(this->ioInProgList);
	casAsyncIOI		*pCurIO;

        if (this->ctx.getServer()->getDebugLevel()>0u) {
                ca_printf ("CAS: Connection Terminated\n");
        }

        //
        // cancel any pending asynchronous IO
        //
        pCurIO = iterIO();
        while (pCurIO) {
                casAsyncIOI     *pNextIO;
                //
                // destructor removes from this list
                //
                pNextIO = iterIO();
                delete pCurIO;
                pCurIO = pNextIO;
        }
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

void casCoreClient::show (unsigned level)
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
caStatus casCoreClient::searchResponse(casChannelI *, const caHdr &, 
		gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::createChanResponse(casChannelI *, const caHdr &, 
	gdd *, const caStatus)
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
	gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeNotifyResponse(casChannelI *, const caHdr &, 
	gdd *, const caStatus)
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
// casCoreClient::lookupRes()
// (this shows up undefined if it is inline and compiled by g++?)
//
casRes *casCoreClient::lookupRes(const caResId &idIn, casResType type)
{
        return this->ctx.getServer()->lookupRes(idIn, type);
}

//
// casCoreClient::asyncIOCompletion()
// (this shows up undefined if it is inline and compiled by g++?)
//
caStatus casCoreClient::asyncIOCompletion(casChannelI *pChan,
        const caHdr &msg, gdd *pDesc, caStatus completionStatus)
{
        pAsyncIoCallBack        pCB;
 
        pCB = casCoreClient::asyncIOJumpTable[msg.m_cmmd];
        if (pCB==NULL) {
                return S_casApp_noSupport;
        }
 
        return (this->*pCB)(pChan, msg, pDesc, completionStatus);
}

