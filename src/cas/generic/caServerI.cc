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

#define caServerGlobal
#include "server.h"
#include "casCtxIL.h" // casCtx in line func

//
// the maximum beacon period if EPICS_CA_BEACON_PERIOD isnt available
//
static const double CAServerMaxBeaconPeriod = 15.0; // seconds

//
// the initial beacon period
//
static const double CAServerMinBeaconPeriod = 1.0e-3; // seconds

//
// caServerI::show()
//
void caServerI::show (unsigned level) const
{
    int			bytes_reserved;
    
    printf( "Channel Access Server Status V%d.%d\n",
        CA_PROTOCOL_VERSION, CA_MINOR_VERSION);
    
    this->osiMutex::show(level);
    
    this->osiLock();
    tsDLIterBD<casStrmClient> iterCl(this->clientList.first());
    while ( iterCl!=tsDLIterBD<casStrmClient>::eol() ) {
        iterCl->show(level);
        ++iterCl;
    }
    this->dgClient.show(level);
    
    tsDLIterBD<casIntfOS> iterIF(this->intfList.first());
    while ( iterIF!=tsDLIterBD<casIntfOS>::eol() ) {
        iterIF->show(level);
        ++iterIF;
    }
    
    this->osiUnlock();
    
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
        this->osiLock();
        this->chronIntIdResTable<casRes>::show(level);
        this->osiUnlock();
    }
    
    // @@@@@@ caPrintAddrList(&destAddr);
    
    return;
}

//
// caServerI::caServerI()
//
caServerI::caServerI (caServer &tool, unsigned nPV) :
    caServerOS (*this),
    casEventRegistry (* (osiMutex *) this),
    dgClient (*this),
    chronIntIdResTable<casRes>(nPV*2u),
    //
    // Set up periodic beacon interval
    // (exponential back off to a plateau
    // from this intial period)
    //
    beaconPeriod (CAServerMinBeaconPeriod),
    adapter (tool),
    debugLevel (0u),
    haveBeenInitialized (FALSE)
{
	caStatus status;
	double maxPeriod;

	assert (&adapter != NULL);
	
	if (this->osiMutex::init ()) {
		errMessage (S_cas_noMemory, "CA server mutex init");
		return;
	}
	
	status = caServerIO::init (*this);
	if (status) {
		errMessage (status, "CA server IO internals init");
		return;
	}
	
	if (this->intfList.count()==0u) {
		errMessage (S_cas_noInterface, "CA server internals init");
		return;
	}
	
	status = caServerOS::init();
	if (status) {
		errMessage (status, "CA server OS dependent init");
		return;
	}
	
	status = this->dgClient.init();
	if (status) {
		errMessage (status, "CA server DG init");
		return;
	}
	
	status = envGetDoubleConfigParam (&EPICS_CA_BEACON_PERIOD, &maxPeriod);
	if (status || maxPeriod<=0.0) {
		this->maxBeaconInterval = CAServerMaxBeaconPeriod;
		ca_printf (
			"EPICS \"%s\" float fetch failed\n", EPICS_CA_BEACON_PERIOD.name);
		ca_printf (
			"Setting \"%s\" = %f\n", EPICS_CA_BEACON_PERIOD.name, 
			this->maxBeaconInterval);
	}
	else {
		this->maxBeaconInterval = maxPeriod;
	}

	this->haveBeenInitialized = TRUE;
	return;
}

/*
 * caServerI::~caServerI()
 */
caServerI::~caServerI()
{

	this->osiLock();

	//
	// delete all clients
	//
	tsDLIterBD<casStrmClient> iter(this->clientList.first());
    while ( iter!=tsDLIterBD<casStrmClient>::eol() ) {
		tsDLIterBD<casStrmClient> tmp = iter;
		++tmp;
		//
		// destructor takes client out of list
		//
		casStrmClient *pC = iter;
		delete pC;
		iter = tmp;
	}

	casIntfOS *pIF;
	while ( (pIF = this->intfList.get()) ) {
		delete pIF;
	}

	this->osiUnlock();
}

//
// caServerI::installClient()
//
void caServerI::installClient(casStrmClient *pClient)
{
	this->osiLock();
	this->clientList.add(*pClient);
	this->osiUnlock();
}

//
// caServerI::removeClient()
//
void caServerI::removeClient(casStrmClient *pClient)
{
	this->osiLock();
	this->clientList.remove(*pClient);
	this->osiUnlock();
}

//
// caServerI::connectCB()
//
void caServerI::connectCB(casIntfOS &intf)
{
        casStreamOS *pNewClient;
        caStatus status;
 
	pNewClient = intf.newStreamClient(*this);
	if (!pNewClient) {
                errMessage(S_cas_noMemory, NULL);
                return;
	}

        status = pNewClient->init();
        if (status) {
                errMessage(status, NULL);
                delete pNewClient;
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
void casVerifyFunc(const char *pFile, unsigned line, const char *pExp)
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
void serverToolDebugFunc(const char *pFile, unsigned line, const char *pComment)
{
       fprintf(stderr,
"Bad server tool response detected at line %u in \"%s\" because \"%s\"\n",
                line, pFile, pComment);
}

//
// caServerI::addAddr()
//
caStatus caServerI::addAddr(const caNetAddr &addr, int autoBeaconAddr,
			int addConfigBeaconAddr)
{
        caStatus stat;
        casIntfOS *pIntf;
 
        pIntf = new casIntfOS(*this);
        if (pIntf) {
                stat = pIntf->init(addr, this->dgClient, 
				autoBeaconAddr, addConfigBeaconAddr);
                if (stat==S_cas_success) {
			this->osiLock();
                        this->intfList.add(*pIntf);
			this->osiUnlock();
                }
                else {
                        errMessage(stat, NULL); 
                        delete pIntf;
                }
        }
        else {
                stat = S_cas_noMemory;
        }
        return stat;
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
	this->osiLock();
	tsDLIterBD<casIntfOS> iter(this->intfList.first());
	while ( iter ) {
		iter->requestBeacon();
		iter++;
	}
	this->osiUnlock();
 
	//
	// double the period between beacons (but dont exceed max)
	//
	this->advanceBeaconPeriod();
}

//
// casEventRegistry::~casEventRegistry()
//
// (must not be inline because it is virtual)
//
casEventRegistry::~casEventRegistry()
{
	this->destroyAllEntries();
}
