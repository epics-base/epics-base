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
#include <server.h>

VERSIONID(casAccessc,"%W% %G%")


//
// caServerI::show()
//
void caServerI::show (unsigned level)
{
        casStrmClient		*pClient;
	tsDLIter<casStrmClient>	iter(this->clientList);
        int			bytes_reserved;

        printf( "Channel Access Server Status V%d.%d\n",
                CA_PROTOCOL_VERSION, CA_MINOR_VERSION);

        this->lock();
        while ( (pClient = iter()) ) {
                pClient->show(level);
        }

        this->dgClient.show(level);
        this->unlock();

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
                this->lock();
                this->uintResTable<casRes>::show(level);
                this->unlock();
                printf( 
	"The server's character string resource id conversion table:\n");
		this->lock();
                this->stringResTbl.show(level);
               	this->unlock();
        }

        // @@@@@@ caPrintAddrList(&destAddr);

        return;
}


//
// caServerI::caServerI()
//
caServerI::caServerI (caServer &tool, unsigned maxNameLength, 
		unsigned nPV, unsigned maxSimultIO) :
	dgClient(*this),
        //
        // Set up periodic beacon interval
        // (exponential back off to a plateau
        // from this intial period)
        //
	beaconPeriod(CAServerMinBeaconPeriod),
	adapter(tool),
	pvCount(0u),
	debugLevel(0u),
	nExistTestInProg(0u),
	pvMaxNameLength(maxNameLength), 
	pvCountEstimate(nPV<100u?100u:nPV), 
	maxSimultaneousIO(maxSimultIO),
	caServerOS(*this)
{
	assert(&adapter);
	ctx.setServer(this);
}


//
// caServerI::init()
//
caStatus caServerI::init()
{
	int	status;
	int	resLibStatus;

        if (this->osiMutex::init()) {
                return S_cas_noMemory;
        }

        status = caServerIO::init();
        if (status) {
                return status;
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
	resLibStatus = this->uintResTable<casRes>::init(this->pvCountEstimate*8u);
	if (resLibStatus) {
		ca_printf("CAS: integer resource id table init failed\n");
		return S_cas_noMemory;
	}

	//
	// hash table size may need adjustment here?
	//
	resLibStatus = this->stringResTbl.init(this->pvCountEstimate*2u);
	if (resLibStatus) {
		ca_printf("CAS: string resource id table init failed\n");
		return S_cas_noMemory;
	}

	return S_cas_success;
}



/*
 * caServerI::~caServerI()
 */
caServerI::~caServerI()
{
	casClient *pClient;

	this->lock();

	//
	// delete all clients
	//
	while ( (pClient = this->clientList.get()) ) {
		delete pClient;
	}

	//
	// verify that we didnt leak a PV
	//
	assert (this->pvCount==0u);
}


//
// caServerI::installClient()
//
void caServerI::installClient(casStrmClient *pClient)
{
	this->lock();
	this->clientList.add(*pClient);
	this->unlock();
}


//
// caServerI::removeClient()
//
void caServerI::removeClient(casStrmClient *pClient)
{
	this->lock();
	this->clientList.remove(*pClient);
	this->unlock();
}


//
// caServerI::connectCB()
//
void caServerI::connectCB()
{
        casStreamOS *pNewClient;
	casMsgIO *pIO;
        caStatus status;
 
	pIO = this->newStreamIO();
	if (!pIO) {
                errMessage(S_cas_noMemory, NULL);
                return;
	}

	status = pIO->init();
	if (status) {
                errMessage(status, NULL);
		delete pIO;
                return;
	}

        pNewClient = new casStreamOS(*this, *pIO);
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
        fprintf(stderr,
"We suspect internal problems at line %u in \"%s\" because \"%s\"==0\n",
                        line, pFile, pExp);
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

