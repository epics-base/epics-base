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
#include "casCoreClient.h"
#include "casAsyncPVExistIOI.h"
#include "casAsyncPVAttachIOI.h"

casCoreClient::casCoreClient ( caServerI & serverInternal ) :
    eventSys ( *this )
{
	assert ( & serverInternal );
	ctx.setServer ( & serverInternal );
	ctx.setClient ( this );
}
 
casCoreClient::~casCoreClient()
{
    // only used by io that does not have a channel
	while ( casAsyncIOI * pIO = this->ioList.get() ) {
        pIO->removeFromEventQueue ();
		delete pIO;
	}
    if ( this->ctx.getServer()->getDebugLevel() > 0u ) {
		errlogPrintf ( "CAS: Connection Terminated\n" );
    }
}

caStatus casCoreClient::disconnectChan ( caResId )
{
	printf ("Disconnect Chan issued for inappropriate client type?\n");
	return S_cas_success;
}

void casCoreClient::show ( unsigned level ) const
{
	printf ( "Core client\n" );
    epicsGuard < epicsMutex > guard ( this->mutex );
	this->eventSys.show ( level );
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
caStatus casCoreClient::readResponse ( casChannelI *, const caHdrLargeArray &, 
	    const gdd &, const caStatus )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::readNotifyResponse ( casChannelI *, const caHdrLargeArray &, 
	    const gdd &, const caStatus )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeResponse ( casChannelI &, 
        const caHdrLargeArray &, const caStatus )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::writeNotifyResponse ( casChannelI &, 
        const caHdrLargeArray &, const caStatus )
{
	return S_casApp_noSupport;
}
caStatus casCoreClient::monitorResponse ( casChannelI &, const caHdrLargeArray &, 
	const gdd &, const caStatus )
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
    const caStatus )
{
	return S_casApp_noSupport;
}

caNetAddr casCoreClient::fetchLastRecvAddr () const
{
	return caNetAddr(); // sets addr type to UDF
}

ca_uint32_t casCoreClient::datagramSequenceNumber () const
{
	return 0;
}

ca_uint16_t casCoreClient::protocolRevision() const
{
    return 0;
}

// this is a pure virtual function, but we nevertheless need a  
// noop to be called if they post events when a channel is being 
// destroyed when we are in the casStrmClient destructor
void casCoreClient::eventSignal()
{
}

caStatus casCoreClient::casMonitorCallBack ( casMonitor &,
    const gdd & )
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
            chan, clientId, count, type, mask, *this );
    this->eventSys.installMonitor ();
    return mon;
}

void casCoreClient::destroyMonitor ( casMonitor & mon )
{
    this->eventSys.removeMonitor ();
    assert ( mon.numEventsQueued() == 0 );
    this->ctx.getServer()->casMonitorDestroy ( mon );
}

void casCoreClient::installAsynchIO ( casAsyncPVAttachIOI & io )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->ioList.add ( io );
}

void casCoreClient::uninstallAsynchIO ( casAsyncPVAttachIOI & io )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->ioList.remove ( io );
}

void casCoreClient::installAsynchIO ( casAsyncPVExistIOI & io )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->ioList.add ( io );
}

void casCoreClient::uninstallAsynchIO ( casAsyncPVExistIOI & io )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->ioList.remove ( io );
}
