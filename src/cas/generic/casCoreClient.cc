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
#include "caServerIIL.h"	
#include "casEventSysIL.h"	
#include "casCtxIL.h"		
#include "inBufIL.h"		
#include "outBufIL.h"		
#include "casCoreClientIL.h" 

//
// casCoreClient::casCoreClient()
//
casCoreClient::casCoreClient ( caServerI & serverInternal ) :
    eventSys ( *this )
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

    epicsGuard < epicsMutex > guard ( this->mutex );

	tsDLIter<casAsyncIOI> iterIO = this->ioInProgList.firstIter ();

	//
	// cancel any pending asynchronous IO
	//
	while ( iterIO.valid () ) {
		//
		// destructor removes from this list
		//
		tsDLIter <casAsyncIOI> tmpIO = iterIO;
		++tmpIO;
        iterIO->serverDestroy ();
		iterIO = tmpIO;
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

void casCoreClient::show ( unsigned level ) const
{
	printf ( "Core client\n" );
	this->eventSys.show ( level );
	printf ( "\t%d io ops in progess\n", this->ioInProgList.count() );
	this->ctx.show ( level );
    this->mutex.show ( level );
}

//
// one of these for each CA request type that has
// asynchronous completion
//
caStatus casCoreClient::asyncSearchResponse (
		const caNetAddr &, const caHdrLargeArray &, const pvExistReturn &,
        ca_uint16_t, ca_uint32_t )
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
caStatus casCoreClient::accessRightsResponse ( casChannelI * )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::enumPostponedCreateChanResponse ( casChannelI &, 
    const caHdrLargeArray &, unsigned )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::channelCreateFailedResp ( const caHdrLargeArray &, 
    caStatus )
{
	return S_casApp_noSupport;
}

//
// casCoreClient::installChannel()
//
void casCoreClient::installChannel ( casChannelI & )
{
	assert (0); // dont install channels on the wrong type of client
}
 
//
// casCoreClient::removeChannel()
//
void casCoreClient::removeChannel ( casChannelI & )
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
// casCoreClient::datagramSequenceNumber ()
//
ca_uint32_t casCoreClient::datagramSequenceNumber () const
{
	return 0;
}

//
// casCoreClient::protocolRevision ()
//
ca_uint16_t casCoreClient::protocolRevision() const
{
    return 0;
}

//
// casCoreClient::lookupRes()
//
casRes * casCoreClient::lookupRes (const caResId &idIn, casResType type)
{
	return this->ctx.getServer()->lookupRes(idIn, type);
}

// this is a pure virtual function, but we nevertheless need a  
// noop to be called if they post events when a channel is being 
// destroyed when we are in the casStrmClient destructor
void casCoreClient::eventSignal()
{
}

caStatus casCoreClient::casMonitorCallBack ( casMonitor &,
    const smartConstGDDPointer & )
{
    return S_cas_internal;
}

casMonitor & casCoreClient::monitorFactory ( 
    casChannelI & chan,
    caResId clientId, 
    const unsigned long count, 
    const unsigned type, 
    const casEventMask & mask )
{
    casMonitor & mon = this->ctx.getServer()->casMonitorFactory ( 
            chan, clientId, count, 
            type, mask, 
            this->mutex,
            *this );
	this->installMonitor ();
    return mon;
}

void casCoreClient::destroyMonitor ( casMonitor & mon )
{
    this->removeMonitor ();
    this->ctx.getServer()->casMonitorDestroy ( mon );
}




