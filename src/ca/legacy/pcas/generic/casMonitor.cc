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
#include "casMonitor.h"
#include "casChannelI.h"

casEvent::~casEvent () {}

casMonitor::casMonitor ( 
        caResId clientIdIn, 
        casChannelI & chan,
	    ca_uint32_t nElemIn, 
        unsigned dbrTypeIn,
	    const casEventMask & maskIn, 
        casMonitorCallbackInterface & cb ) :
    overFlowEvent ( *this ),
	nElem ( nElemIn ),
	pChannel ( & chan ),
    callBackIntf ( cb ),
	mask ( maskIn ),
	clientId ( clientIdIn ),
	dbrType ( static_cast <unsigned char> ( dbrTypeIn ) ),
	nPend ( 0u ),
	ovf ( false )
{
	assert ( dbrTypeIn <= 0xff );
}

casMonitor::~casMonitor()
{
}

caStatus casMonitor::response ( 
    epicsGuard < casClientMutex > & guard,
    casCoreClient & client, const gdd & value )
{
    if ( this->pChannel ) {
        // reconstruct request header
        caHdrLargeArray msg;
	    msg.m_cmmd = CA_PROTO_EVENT_ADD;
	    msg.m_postsize = 0u;
	    msg.m_dataType = this->dbrType;
	    msg.m_count = this->nElem;
	    msg.m_cid = this->pChannel->getSID();
	    msg.m_available = this->clientId; 
	    return client.monitorResponse ( 
            guard, *this->pChannel, msg, value, S_cas_success );
    }
    else {
        return S_cas_success;
    }
}

void casMonitor::installNewEventLog ( 
    tsDLList < casEvent > & eventLogQue, 
    casMonEvent * pLog, const gdd & event )
{
	if ( this->ovf ) {
		if ( pLog ) {
            pLog->assign ( event );
            this->overFlowEvent.swapValues ( *pLog );
			eventLogQue.insertAfter ( *pLog, this->overFlowEvent );
            assert ( this->nPend != UCHAR_MAX );
		    this->nPend++;
		}
		else {
			// replace the old OVF value with the current one
			this->overFlowEvent.assign ( event );
		}
        // remove OVF entry (with its new value) from the queue so
        // that it ends up properly ordered at the back of the
        // queue
        eventLogQue.remove ( this->overFlowEvent );
		pLog = & this->overFlowEvent;
	}
    else {
	    if ( pLog == 0 ) {
		    // use the over flow block in the event structure
		    this->ovf = true;
 		    pLog = & this->overFlowEvent;
        }
        pLog->assign ( event );
        assert ( this->nPend != UCHAR_MAX );
		this->nPend++;
    }
    eventLogQue.add ( *pLog );
}

caStatus casMonitor::executeEvent ( casCoreClient & client, 
    casMonEvent & ev, const gdd & value,
    epicsGuard < casClientMutex > & clientGuard,
    epicsGuard < evSysMutex > & evGuard )
{
    if ( this->pChannel ) {
        caStatus status = this->callBackIntf.casMonitorCallBack ( 
                clientGuard, *this, value );
	    if ( status != S_cas_success ) {
            return status;
        }
    }

    client.getCAS().incrEventsProcessedCounter ();
    assert ( this->nPend != 0u );
	this->nPend--;

	// delete event object if it isnt a cache entry
	// saved in the call back object
	if ( & ev == & this->overFlowEvent ) {
		assert ( this->ovf );
		this->ovf = false;
		this->overFlowEvent.clear ();
	}
	else {
        client.casMonEventDestroy ( ev, evGuard );
	}

    if ( ! this->pChannel && this->nPend == 0 ) {
        // we carefully avoid inverting the lock hierarchy here
        epicsGuardRelease < evSysMutex > unGuardEv ( evGuard );
        {
            epicsGuardRelease < casClientMutex > unGuardClient ( clientGuard );
            client.destroyMonitor ( *this );
        }
    }

    return S_cas_success;
}

void casMonitor::show ( unsigned level ) const
{
    if ( level > 1u ) {
        printf (
"\tmonitor type=%u count=%u client id=%u OVF=%u nPend=%u\n",
                    dbrType, nElem, clientId, ovf, nPend );
		this->mask.show ( level );
    }
}

void * casMonitor::operator new ( 
            size_t size, 
            tsFreeList < casMonitor, 1024 > & freeList ) 
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void casMonitor::operator delete ( void * pCadaver, 
                     tsFreeList < casMonitor, 1024 > & freeList ) 
{
    freeList.release ( pCadaver );
}
#endif

void casMonitor::operator delete ( void * ) 
{
    errlogPrintf ( "casMonitor: compiler is confused "
        "about placement delete?\n" );
}
