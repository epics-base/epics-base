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
#include "casEventSysIL.h" // casEventSys inline func
#include "casPVIIL.h" // casPVI inline func
#include "casCtxIL.h" // casCtx inline func

//
// casChannelI::casChannelI()
//
casChannelI::casChannelI ( const casCtx &  ctx ) :
		pClient ( ctx.getClient() ), pPV ( ctx.getPV() ), 
        cid ( ctx.getMsg()->m_cid ), accessRightsEvPending ( false )
{
}

//
// casChannelI::~casChannelI()
//
casChannelI::~casChannelI()
{	    
    //
    // cancel any pending asynchronous IO 
    //
    tsDLIter<casAsyncIOI> iterAIO = this->ioInProgList.firstIter ();
    while ( iterAIO.valid () ) {
        //
        // destructor removes from this list
        //
        tsDLIter <casAsyncIOI> tmpAIO = iterAIO;
        ++tmpAIO;
        iterAIO->serverDestroy ();
        iterAIO = tmpAIO;
    }
    
    //
    // cancel the monitors 
    //
    while ( casMonitor * pMon = this->monitorList.get () ) {
	    this->getClient().destroyMonitor ( *pMon );
    }
    
    this->pClient->removeChannel ( *this );
    
    //
    // force PV delete if this is the last channel attached
    //
    this->pPV->deleteSignal ();
}

//
// casChannelI::clearOutstandingReads()
//
void casChannelI::clearOutstandingReads()
{
    //
    // cancel any pending asynchronous IO 
    //
	tsDLIter <casAsyncIOI> iterIO = this->ioInProgList.firstIter ();
	while ( iterIO.valid () ) {
		//
		// destructor removes from this list
		//
		tsDLIter<casAsyncIOI> tmp = iterIO;
		++tmp;
		iterIO->serverDestroyIfReadOP();
		iterIO = tmp;
	}
}

//
// casChannelI::show()
//
void casChannelI::show ( unsigned level ) const
{
    printf ( "casChannelI: client id %u PV %s\n", 
        this->cid, this->pPV->getName() );
    if ( this->monitorList.count() ) {
		printf ( "List of subscriptions attached\n" );
        // use the client's lock to protect the list
        this->pClient->showMonitorsInList ( 
            this->monitorList, level );
    }
}

//
// casChannelI::cbFunc()
//
// access rights event call back
//
caStatus casChannelI::cbFunc ( casCoreClient & )
{
	caStatus stat;

	stat = this->pClient->accessRightsResponse ( this );
	if ( stat == S_cas_success ) {
		this->accessRightsEvPending = false;
	}
	return stat;
}

void casChannelI::eventSysDestroyNotify ( casCoreClient & ) 
{
	delete this;
}

//
// casChannelI::resourceType()
//
casResType casChannelI::resourceType() const
{
	return casChanT;
}

//
// casChannelI::destroy()
//
// this noop version is safe to be called indirectly
// from casChannelI::~casChannelI
//
void casChannelI::destroy()
{
}

void casChannelI::destroyClientNotify ()
{
	casChanDelEv *pCDEV;
    caStatus status;

	pCDEV = new casChanDelEv ( this->getCID() );
	if ( pCDEV ) {
		this->pClient->addToEventQueue ( *pCDEV );
	}
	else {	
		status = this->pClient->disconnectChan ( this->getCID () );
		if ( status ) {
			//
			// At this point there is no space in pool
			// for a tiny object and there is also
			// no outgoing buffer space in which to place
			// a message in which we inform the client
			// that his channel was deleted.
			//
			// => disconnect this client via the event
			// queue because deleting the client here
			// will result in bugs because no doubt this
			// could be called by a client member function.
			//
			this->pClient->setDestroyPending ();
		}
	}
    this->destroy ();
}

bool casChannelI::unistallMonitor ( ca_uint32_t clientIdIn )
{
	//
	// (it is reasonable to do a linear search here because
	// sane clients will require only one or two monitors 
	// per channel)
	//
	tsDLIter <casMonitor> iter = this->monitorList.firstIter ();
    while ( iter.valid () ) {
		if ( clientIdIn == iter->getClientId () ) {
	        this->monitorList.remove ( *iter.pointer() );
	        this->getClient().destroyMonitor ( *iter.pointer() );
            return true;
		}
		iter++;
	}
    return false;
}

//
// casChannelI::installMonitor ()
//
void casChannelI::installMonitor ( 
    caResId clientId, 
    const unsigned long count, 
    const unsigned type, 
    const casEventMask & mask )
{
    casMonitor & mon = this->pClient->monitorFactory (
        *this, clientId, count, type, mask );
	this->monitorList.add ( mon );
}

