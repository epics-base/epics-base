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

#include <stdexcept>

#include "epicsGuard.h"
#include "epicsVersion.h"

#include "addrList.h"

#define caServerGlobal
#include "server.h"
#include "casCtxIL.h" // casCtx in line func
#include "beaconTimer.h"
#include "beaconAnomalyGovernor.h"

static const char * pVersionCAS = 
    "@(#) " EPICS_VERSION_STRING ", CA Portable Server Library " 
    "$Date$";

//
// caServerI::caServerI()
//
caServerI::caServerI (caServer &tool) :
    adapter (tool),
    beaconTmr ( * new beaconTimer ( *this ) ),
    beaconAnomalyGov ( * new beaconAnomalyGovernor ( *this ) ),
    debugLevel (0u),
    nEventsProcessed (0u),
    nEventsPosted (0u)
{
	assert ( & adapter != NULL );

    // create predefined event types
    this->valueEvent = registerEvent ("value");
	this->logEvent = registerEvent ("log");
	this->alarmEvent = registerEvent ("alarm");

    this->locateInterfaces ();

	if (this->intfList.count()==0u) {
		errMessage (S_cas_noInterface, 
            "- CA server internals init unable to continue");
        throw S_cas_noInterface;
	}

	return;
}

/*
 * caServerI::~caServerI()
 */
caServerI::~caServerI()
{
    epicsGuard < epicsMutex > locker ( this->mutex );

    delete & this->beaconAnomalyGov;
    delete & this->beaconTmr;

	// delete all clients
	tsDLIter <casStrmClient> iter = this->clientList.firstIter ();
    while ( iter.valid () ) {
		tsDLIter <casStrmClient> tmp = iter;
		++tmp;
		//
		// destructor takes client out of list
		//
		iter->destroy ();
		iter = tmp;
	}

	casIntfOS *pIF;
	while ( ( pIF = this->intfList.get() ) ) {
		delete pIF;
	}
}

//
// caServerI::installClient()
//
void caServerI::installClient(casStrmClient *pClient)
{
    epicsGuard < epicsMutex > locker ( this->mutex );
	this->clientList.add(*pClient);
}

//
// caServerI::removeClient()
//
void caServerI::removeClient (casStrmClient *pClient)
{
    epicsGuard < epicsMutex > locker ( this->mutex );
	this->clientList.remove (*pClient);
}

//
// caServerI::connectCB()
//
void caServerI::connectCB ( casIntfOS & intf )
{
    casStreamOS * pClient = intf.newStreamClient ( *this, this->clientBufMemMgr );
    if ( pClient ) {
        pClient->sendVersion ();
        pClient->flush ();
    }
}

//
// casVerifyFunc()
//
void casVerifyFunc (const char *pFile, unsigned line, const char *pExp)
{
    fprintf(stderr, "the expression \"%s\" didnt evaluate to boolean true \n",
        pExp);
    fprintf(stderr,
        "and therefore internal problems are suspected at line %u in \"%s\"\n",
        line, pFile);
    fprintf(stderr,
        "Please forward above text to johill@lanl.gov - thanks\n");
}

//
// serverToolDebugFunc()
// 
void serverToolDebugFunc (const char *pFile, unsigned line, const char *pComment)
{
    fprintf (stderr,
"Bad server tool response detected at line %u in \"%s\" because \"%s\"\n",
                line, pFile, pComment);
}

//
// caServerI::attachInterface()
//
caStatus caServerI::attachInterface ( const caNetAddr & addrIn, 
        bool autoBeaconAddr, bool addConfigBeaconAddr)
{    
    casIntfOS * pIntf = new casIntfOS ( *this, this->clientBufMemMgr, 
        addrIn, autoBeaconAddr, addConfigBeaconAddr );
    
    {
        epicsGuard < epicsMutex > locker ( this->mutex );
        this->intfList.add ( *pIntf );
    }

    return S_cas_success;
}

//
// caServerI::sendBeacon()
// send a beacon over each configured address
//
void caServerI::sendBeacon ( ca_uint32_t beaconNo )
{
    epicsGuard < epicsMutex > locker ( this->mutex );
	tsDLIter <casIntfOS> iter = this->intfList.firstIter ();
	while ( iter.valid () ) {
		iter->sendBeacon ( beaconNo );
		iter++;
	}
}

void caServerI::generateBeaconAnomaly ()
{
    this->beaconAnomalyGov.start ();
}

//
// caServerI::lookupMonitor ()
//
casMonitor * caServerI::lookupMonitor ( const caResId &idIn )
{
    chronIntId tmpId ( idIn );

	epicsGuard < epicsMutex > locker ( this->mutex );
    casRes * pRes = this->chronIntIdResTable<casRes>::lookup ( tmpId );
	if ( pRes ) {
		if ( pRes->resourceType() == casMonitorT ) {
            // ok to avoid overhead of dynamic cast here because
            // type code was checked above
            return static_cast < casMonitor * > ( pRes );
		}
	}
	return 0;
}

//
// caServerI::lookupChannel ()
//
casChannelI * caServerI::lookupChannel ( const caResId &idIn )
{
    chronIntId tmpId ( idIn );

	epicsGuard < epicsMutex > locker ( this->mutex );
    casRes * pRes = this->chronIntIdResTable<casRes>::lookup ( tmpId );
	if ( pRes ) {
		if ( pRes->resourceType() == casChanT ) {
            // ok to avoid overhead of dynamic cast here because
            // type code was checked above
            return static_cast < casChannelI * > ( pRes );
		}
	}
	return 0;
}

//
// caServerI::show()
//
void caServerI::show (unsigned level) const
{
    int bytes_reserved;
    
    printf ( "Channel Access Server V%s\n",
        CA_VERSION_STRING ( CA_MINOR_PROTOCOL_REVISION ) );
    printf ( "\trevision %s\n", pVersionCAS );

    this->mutex.show(level);
    
    {
        epicsGuard < epicsMutex > locker ( this->mutex );
        tsDLIterConst<casStrmClient> iterCl = this->clientList.firstIter ();
        while ( iterCl.valid () ) {
            iterCl->show (level);
            ++iterCl;
        }
    
        tsDLIterConst<casIntfOS> iterIF = this->intfList.firstIter ();
        while ( iterIF.valid () ) {
            iterIF->casIntfOS::show ( level );
            ++iterIF;
        }
    }
    
    bytes_reserved = 0u;
#if 0
    bytes_reserved += sizeof(casClient) *
        ellCount(&this->freeClientQ);
    bytes_reserved += sizeof(casChannel) *
        ellCount(&this->freeChanQ);
    bytes_reserved += sizeof(casEventBlock) *
        ellCount(&this->freeEventQ);
    bytes_reserved += sizeof(casAsyncIIO) *
        ellCount(&this->freePendingIO);
#endif
    if (level>=1) {
        printf(
            "There are currently %d bytes on the server's free list\n",
            bytes_reserved);
#if 0
        printf(
            "%d client(s), %d channel(s), %d event(s) (monitors), and %d IO blocks\n",
            ellCount(&this->freeClientQ),
            ellCount(&this->freeChanQ),
            ellCount(&this->freeEventQ),
            ellCount(&this->freePendingIO));
#endif
        printf( 
            "The server's integer resource id conversion table:\n");
        {
            epicsGuard < epicsMutex > locker ( this->mutex );
            this->chronIntIdResTable<casRes>::show(level);
        }
    }
        
    return;
}

//
// casEventRegistry::~casEventRegistry()
//
// (must not be inline because it is virtual)
//
casEventRegistry::~casEventRegistry()
{
    this->traverse ( &casEventMaskEntry::destroy );
}

