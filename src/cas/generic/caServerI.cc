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
 * History
 * $Log$
 * Revision 1.7  1997/04/10 19:33:53  jhill
 * API changes
 *
 * Revision 1.6  1996/11/02 00:53:54  jhill
 * many improvements
 *
 * Revision 1.5  1996/09/16 18:23:56  jhill
 * vxWorks port changes
 *
 * Revision 1.4  1996/09/04 20:12:04  jhill
 * added arg to serverToolDebugFunc()
 *
 * Revision 1.3  1996/08/13 22:56:12  jhill
 * added init for mutex class
 *
 * Revision 1.2  1996/08/05 19:25:17  jhill
 * removed unused code
 *
 * Revision 1.1.1.1  1996/06/20 00:28:15  jhill
 * ca server installation
 *
 *
 */

#define CAS_VERSION_GLOBAL

#define caServerGlobal
#include "server.h"
#include "casCtxIL.h" // casCtx in line func

static const osiTime CAServerMaxBeaconPeriod (5.0 /* sec */);
static const osiTime CAServerMinBeaconPeriod (1.0e-3 /* sec */);


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
	const tsDLIterBD<casStrmClient> eolSC;
	tsDLIterBD<casStrmClient> iterCl(this->clientList.first());
        while ( iterCl!=eolSC ) {
                iterCl->show(level);
		++iterCl;
        }
        this->dgClient.show(level);

	const tsDLIterBD<casIntfOS> eolIOS;
	tsDLIterBD<casIntfOS> iterIF(this->intfList.first());
        while ( iterIF!=eolIOS ) {
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
                this->uintResTable<casRes>::show(level);
                this->osiUnlock();
        }

        // @@@@@@ caPrintAddrList(&destAddr);

        return;
}


//
// caServerI::caServerI()
//
caServerI::caServerI (caServer &tool, unsigned nPV) :
	caServerOS(*this),
	casEventRegistry(* (osiMutex *) this),
	dgClient(*this),
        //
        // Set up periodic beacon interval
        // (exponential back off to a plateau
        // from this intial period)
        //
	beaconPeriod(CAServerMinBeaconPeriod),
	adapter(tool),
	debugLevel(0u),
	pvCountEstimate(nPV<100u?100u:nPV), 
	haveBeenInitialized(FALSE)
{
	caStatus	status;

	assert(&adapter);
	//ctx.setServer(this);

	status = this->init();
	if (status) {
		errMessage(status, "CA server internals init");
	}
}


//
// caServerI::init()
//
caStatus caServerI::init()
{
	int		status;
	int		resLibStatus;

        if (this->osiMutex::init()) {
                return S_cas_noMemory;
        }

        status = casEventRegistry::init();
        if (status) {
                return status;
        }

        status = caServerIO::init(*this);
        if (status) {
                return status;
        }

	if (this->intfList.count()==0u) {
		return S_cas_noInterface;
	}

        status = caServerOS::init();
        if (status) {
                return status;
        }

	status = this->dgClient.init();
        if (status) {
                return status;
        }

	//
	// hash table size may need adjustment here?
	//
	resLibStatus = this->uintResTable<casRes>::init(this->pvCountEstimate*2u);
	if (resLibStatus) {
		ca_printf("CAS: integer resource id table init failed\n");
		return S_cas_noMemory;
	}

	this->haveBeenInitialized = TRUE;
	return S_cas_success;
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
	tsDLIterBD<casStrmClient> eol;
	tsDLIterBD<casStrmClient> tmp;
	while ( iter!=eol ) {
		tmp = iter;
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
        if (this->beaconPeriod >= CAServerMaxBeaconPeriod) {
                return;
        }

	this->beaconPeriod += this->beaconPeriod;

	if (this->beaconPeriod >= CAServerMaxBeaconPeriod) {
		this->beaconPeriod = CAServerMaxBeaconPeriod;
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

