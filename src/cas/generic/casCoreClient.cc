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
#include "caServerIIL.h"	// caServerI in line func
#include "casAsyncIOIIL.h"	// casAsyncIOI in line func
#include "casEventSysIL.h"	// casEventSys in line func
#include "casCtxIL.h"		// casCtx in line func
#include "inBufIL.h"		// inBuf in line func
#include "outBufIL.h"		// outBuf in line func

//
// casCoreClient::casCoreClient()
//
casCoreClient::casCoreClient (caServerI &serverInternal)
{
	assert (&serverInternal);
	ctx.setServer (&serverInternal);
	ctx.setClient (this);
}
 
//
// casCoreClient::~casCoreClient()
//
casCoreClient::~casCoreClient()
{
    if (this->ctx.getServer()->getDebugLevel()>0u) {
		ca_printf ("CAS: Connection Terminated\n");
    }

	this->lock();
	tsDLIterBD<casAsyncIOI>   iterIO(this->ioInProgList.first());

	//
	// cancel any pending asynchronous IO
	//
	while (iterIO!=tsDLIterBD<casAsyncIOI>::eol()) {
		//
		// destructor removes from this list
		//
		tsDLIterBD<casAsyncIOI> tmpIO = iterIO;
		++tmpIO;
        iterIO->serverDestroy();
		iterIO = tmpIO;
	}

	this->unlock();
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
caStatus casCoreClient::asyncSearchResponse (
		const caNetAddr &, const caHdr &, const pvExistReturn &)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::createChanResponse(const caHdr &, const pvAttachReturn &)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readResponse(casChannelI *, const caHdr &, 
	const gdd &, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readNotifyResponse(casChannelI *, const caHdr &, 
	const gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeResponse(const caHdr &, 
	const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeNotifyResponse(const caHdr &, 
	const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::monitorResponse(casChannelI &, const caHdr &, 
	const gdd *, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::accessRightsResponse(casChannelI *)
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
// casCoreClient::fetchLastRecvAddr ()
//
caNetAddr casCoreClient::fetchLastRecvAddr () const
{
	return caNetAddr(); // sets addr type to UDF
}

//
// casCoreClient::lookupRes()
//
casRes *casCoreClient::lookupRes(const caResId &idIn, casResType type)
{
	casRes *pRes;
	pRes = this->ctx.getServer()->lookupRes(idIn, type);
	return pRes;
}
