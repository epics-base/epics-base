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
		errlogPrintf ("CAS: Connection Terminated\n");
    }

	this->lock();
	tsDLIterBD<casAsyncIOI> iterIO = this->ioInProgList.firstIter ();

	//
	// cancel any pending asynchronous IO
	//
	while ( iterIO.valid () ) {
		//
		// destructor removes from this list
		//
		tsDLIterBD <casAsyncIOI> tmpIO = iterIO;
		++tmpIO;
        iterIO->serverDestroy ();
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
    this->epicsMutex::show (level);
}

//
// one of these for each CA request type that has
// asynchronous completion
//
caStatus casCoreClient::asyncSearchResponse (
		const caNetAddr &, const caHdrLargeArray &, const pvExistReturn & )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::createChanResponse ( const caHdrLargeArray &, const pvAttachReturn & )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readResponse (casChannelI *, const caHdrLargeArray &, 
	const smartConstGDDPointer &, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readNotifyResponse (casChannelI *, const caHdrLargeArray &, 
	const smartConstGDDPointer &, const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeResponse (const caHdrLargeArray &, 
	const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeNotifyResponse (const caHdrLargeArray &, 
	const caStatus)
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::monitorResponse ( casChannelI &, const caHdrLargeArray &, 
	const smartConstGDDPointer &, const caStatus )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::accessRightsResponse (casChannelI *)
{
	return S_casApp_noSupport;
}

//
// casCoreClient::installChannel()
//
void casCoreClient::installChannel (casChannelI &)
{
	assert (0); // dont install channels on the wrong type of client
}
 
//
// casCoreClient::removeChannel()
//
void casCoreClient::removeChannel (casChannelI &)
{
	assert (0); // dont install channels on the wrong type of client
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
casRes *casCoreClient::lookupRes (const caResId &idIn, casResType type)
{
	casRes *pRes;
	pRes = this->ctx.getServer()->lookupRes(idIn, type);
	return pRes;
}
