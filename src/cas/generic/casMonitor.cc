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
#include "casChannelIIL.h" // casChannelI inline func
#include "casEventSysIL.h" // casEventSys inline func
#include "casMonEventIL.h" // casMonEvent inline func
#include "casCtxIL.h" // casCtx inline func


//
// casMonitor::casMonitor()
//
casMonitor::casMonitor ( 
        caResId clientIdIn, 
        casChannelI & chan,
	    unsigned long nElemIn, 
        unsigned dbrTypeIn,
	    const casEventMask & maskIn, 
        epicsMutex & mutexIn,
        casMonitorCallbackInterface & cb ) :
	nElem ( nElemIn ),
	mutex ( mutexIn ),
	ciu ( chan ),
    callBackIntf ( cb ),
	mask ( maskIn ),
	clientId ( clientIdIn ),
	dbrType ( static_cast <unsigned char> ( dbrTypeIn ) ),
	nPend ( 0u ),
	ovf ( false ),
	enabled ( false )
{
	assert ( &this->ciu );
	assert ( dbrTypeIn <= 0xff );
	this->enable();
}

//
// casMonitor::~casMonitor()
//
casMonitor::~casMonitor()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	this->disable();
	if ( this->ovf ) {
	    casCoreClient &client = this->ciu.getClient();
		client.removeFromEventQueue ( this->overFlowEvent );
	}
}

//
// casMonitor::enable()
//
void casMonitor::enable()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	if ( ! this->enabled && this->ciu.readAccess() ) {
		this->enabled = true;
		caStatus status = this->ciu.getPVI().registerEvent();
		if ( status ) {
			errMessage ( status,
				"Server tool failed to register event\n" );
		}
	}
}

//
// casMonitor::disable()
//
void casMonitor::disable()
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	if ( this->enabled ) {
		this->enabled = false;
		this->ciu.getPVI().unregisterEvent();
	}
}

//
// casMonitor::push()
//
void casMonitor::push ( const smartConstGDDPointer & pNewValue )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
	
	casCoreClient & client = this->ciu.getClient ();
    client.getCAS().incrEventsPostedCounter ();

	//
	// get a new block if we havent exceeded quotas
	//
	bool full = ( this->nPend >= individualEventEntries ) 
		|| client.eventSysIsFull ();
	casMonEvent * pLog;
	if ( ! full ) {
        try {
            // I should get rid of this try block by implementing a no 
            // throw version of the free list alloc
            pLog = & client.casMonEventFactory ( *this, pNewValue );
			this->nPend++; // X aCC 818
        }
        catch ( ... ) {
            pLog = 0;
        }
	}
	else {
		pLog = 0;
	}
	
	if ( this->ovf ) {
		if ( pLog ) {
			// swap values
			smartConstGDDPointer pOldValue = this->overFlowEvent.getValue ();
            if ( ! pOldValue ) {
                assert ( 0 ); // due to limitations in class smartConstGDDPointer
            }
            // copy old OVF value into the new entry which must remain
            // ordered in the queue where the OVF entry was before
			pLog->assign ( *this, pOldValue );
            // copy new value into OVF event entry which must be last
			this->overFlowEvent.assign ( *this, *pNewValue ); 
            // this inserts it out of order, but this is fixed below when the 
            // overflow event is removed from the queue 
			client.insertEventQueue ( *pLog, this->overFlowEvent );
		}
		else {
			// replace the old OVF value with the current one
			this->overFlowEvent.assign ( *this, pNewValue );
		}
        // remove OVF entry (with its new value) from the queue so
        // that it ends up properly ordered at the back of the
        // queue
		client.removeFromEventQueue ( this->overFlowEvent );
		pLog = & this->overFlowEvent;
	}
	else if ( ! pLog ) {
		//
		// no log block
		// => use the over flow block in the event structure
		//
		this->ovf = true;
		this->overFlowEvent.assign ( * this, pNewValue );
		this->nPend++; // X aCC 818
		pLog = &this->overFlowEvent;
	}
	
	client.addToEventQueue ( * pLog );
}

//
// casMonitor::executeEvent()
//
caStatus casMonitor::executeEvent ( casMonEvent & ev )
{
	smartConstGDDPointer pVal = ev.getValue ();
    if ( ! pVal ) {
        assert ( 0 );
    }
	
	caStatus status;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
	    status = 
            this->callBackIntf.
                casMonitorCallBack ( *this, *pVal );
    }
	
	//
	// if the event isnt accepted we will try
	// again later (and the event returns to the queue)
	//
	if ( status ) {
		return status;
	}
	
	//
	// decrement the count of the number of events pending
	//
	this->nPend--; // X aCC 818
	
	//
	// delete event object if it isnt a cache entry
	// saved in the call back object
	//
	if ( &ev == &this->overFlowEvent ) {
		assert ( this->ovf );
		this->ovf = false;
		ev.clear();
	}
	else {
        this->ciu.getClient().casMonEventDestroy ( ev );
	}

    this->ciu.getClient().getCAS().incrEventsProcessedCounter ();
	
	return S_cas_success;
}

//
// casMonitor::show(unsigned level)
//
void casMonitor::show ( unsigned level ) const
{
    if ( level > 1u ) {
        printf (
"\tmonitor type=%u count=%lu client id=%u enabled=%u OVF=%u nPend=%u\n",
                    dbrType, nElem, clientId, enabled, ovf, nPend );
		this->mask.show ( level );
    }
}

//
// casMonitor::resourceType()
//
casResType casMonitor::resourceType() const
{
	return casMonitorT;
}

void * casMonitor::operator new ( 
            size_t size, 
            tsFreeList < casMonitor, 1024 > & freeList ) 
            epicsThrows ( ( std::bad_alloc ) )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void casMonitor::operator delete ( void * pCadaver, 
                     tsFreeList < casMonitor, 1024 > & freeList ) epicsThrows(())
{
    freeList.release ( pCadaver );
}
#endif

void casMonitor::operator delete ( void * ) 
{
    errlogPrintf ( "casMonitor: compiler is confused "
        "about placement delete?\n" );
}
