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
 */

#define CAS_VERSION_GLOBAL

#include "addrList.h"

#define caServerGlobal
#include "server.h"
#include "casCtxIL.h" // casCtx in line func

#if defined ( _MSC_VER )
#   pragma warning ( push )
#   pragma warning ( disable: 4660 )
#endif

template class tsSLNode < casPVI >;
template class tsSLNode < casRes >;
template class resTable < casRes, chronIntId >;
template class resTable < casEventMaskEntry, stringId >;
template class chronIntIdResTable < casRes >;

#if defined ( _MSC_VER )
#   pragma warning ( pop )
#endif

//
// the maximum beacon period if EPICS_CA_BEACON_PERIOD isnt available
//
static const double CAServerMaxBeaconPeriod = 15.0; // seconds

//
// the initial beacon period
//
static const double CAServerMinBeaconPeriod = 1.0e-3; // seconds

//
// caServerI::caServerI()
//
caServerI::caServerI (caServer &tool) :
    //
    // Set up periodic beacon interval
    // (exponential back off to a plateau
    // from this intial period)
    //
    beaconPeriod (CAServerMinBeaconPeriod),
    adapter (tool),
    debugLevel (0u),
    nEventsProcessed (0u),
    nEventsPosted (0u)
{
	caStatus status;
	double maxPeriod;

	assert (&adapter != NULL);

    //
    // create predefined event types
    //
    this->valueEvent = registerEvent ("value");
	this->logEvent = registerEvent ("log");
	this->alarmEvent = registerEvent ("alarm");
	
	status = envGetDoubleConfigParam (&EPICS_CA_BEACON_PERIOD, &maxPeriod);
	if (status || maxPeriod<=0.0) {
		this->maxBeaconInterval = CAServerMaxBeaconPeriod;
		errlogPrintf (
			"EPICS \"%s\" float fetch failed\n", EPICS_CA_BEACON_PERIOD.name);
		errlogPrintf (
			"Setting \"%s\" = %f\n", EPICS_CA_BEACON_PERIOD.name, 
			this->maxBeaconInterval);
	}
	else {
		this->maxBeaconInterval = maxPeriod;
	}

    this->locateInterfaces ();

	if (this->intfList.count()==0u) {
		errMessage (S_cas_noInterface, "CA server internals init");
		return;
	}

	return;
}

/*
 * caServerI::~caServerI()
 */
caServerI::~caServerI()
{
    epicsAutoMutex locker ( this->mutex );

	//
	// delete all clients
	//
	tsDLIterBD <casStrmClient> iter = this->clientList.firstIter ();
    while ( iter.valid () ) {
		tsDLIterBD <casStrmClient> tmp = iter;
		++tmp;
		//
		// destructor takes client out of list
		//
		iter->destroy ();
		iter = tmp;
	}

	casIntfOS *pIF;
	while ( (pIF = this->intfList.get()) ) {
		delete pIF;
	}
}

//
// caServerI::installClient()
//
void caServerI::installClient(casStrmClient *pClient)
{
    epicsAutoMutex locker ( this->mutex );
	this->clientList.add(*pClient);
}

//
// caServerI::removeClient()
//
void caServerI::removeClient (casStrmClient *pClient)
{
    epicsAutoMutex locker ( this->mutex );
	this->clientList.remove (*pClient);
}

//
// caServerI::connectCB()
//
void caServerI::connectCB ( casIntfOS & intf )
{
    casStreamOS * pClient = intf.newStreamClient ( *this );
    if ( pClient ) {
        pClient->sendVersion ();
        pClient->flush ();
    }
}

//
// caServerI::advanceBeaconPeriod()
//
// compute delay to the next beacon
//
void caServerI::advanceBeaconPeriod()
{
	//
	// return if we are already at the plateau
	//
	if (this->beaconPeriod >= this->maxBeaconInterval) {
		return;
	}

	this->beaconPeriod += this->beaconPeriod;

	if (this->beaconPeriod >= this->maxBeaconInterval) {
		this->beaconPeriod = this->maxBeaconInterval;
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
caStatus caServerI::attachInterface (const caNetAddr &addr, bool autoBeaconAddr,
                            bool addConfigBeaconAddr)
{
    casIntfOS *pIntf;
    
    pIntf = new casIntfOS (*this, addr, autoBeaconAddr, addConfigBeaconAddr);
    if (pIntf==NULL) {
        return S_cas_noMemory;
    }
    
    {
        epicsAutoMutex locker ( this->mutex );
        this->intfList.add (*pIntf);
    }

    return S_cas_success;
}


//
// caServerI::sendBeacon()
// (implemented here because this has knowledge of the protocol)
//
void caServerI::sendBeacon()
{
	//
	// send a broadcast beacon over each configured
	// interface unless EPICS_CA_AUTO_ADDR_LIST specifies
	// otherwise. Also send a beacon to all configured
	// addresses.
	// 
    {
        epicsAutoMutex locker ( this->mutex );
	    tsDLIterBD <casIntfOS> iter = this->intfList.firstIter ();
	    while ( iter.valid () ) {
		    iter->sendBeacon ();
		    iter++;
	    }
    }
 
	//
	// double the period between beacons (but dont exceed max)
	//
	this->advanceBeaconPeriod();
}

//
// caServerI::getBeaconPeriod()
//
double caServerI::getBeaconPeriod() const 
{ 
    return this->beaconPeriod; 
}

//
// caServerI::show()
//
void caServerI::show (unsigned level) const
{
    int bytes_reserved;
    
    printf( "Channel Access Server Status V%s\n",
        CA_VERSION_STRING ( CA_MINOR_PROTOCOL_REVISION ) );
    
    this->mutex.show(level);
    
    {
        epicsAutoMutex locker ( this->mutex );
        tsDLIterConstBD<casStrmClient> iterCl = this->clientList.firstIter ();
        while ( iterCl.valid () ) {
            iterCl->show (level);
            ++iterCl;
        }
    
        tsDLIterConstBD<casIntfOS> iterIF = this->intfList.firstIter ();
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
            epicsAutoMutex locker ( this->mutex );
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

