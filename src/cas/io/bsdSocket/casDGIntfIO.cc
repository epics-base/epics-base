/*
 *
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 *	History
 */

//
//
// Should I fetch the MTU from the outgoing interface?
//
//

#include "server.h"
#include "ipAddrToA.h"
#include "addrList.h"

//
// casDGIntfIO::casDGIntfIO()
//
casDGIntfIO::casDGIntfIO(casDGClient &clientIn) :
	pAltOutIO(NULL),
	client(clientIn),
	sock(INVALID_SOCKET),
	sockState(casOffLine),
	connectWithThisPort(0),
	dgPort(0)
{
	ellInit(&this->beaconAddrList);
}

//
// casDGIntfIO::init()
//
caStatus casDGIntfIO::init(const caNetAddr &addr, unsigned connectWithThisPortIn,
		int autoBeaconAddr, int addConfigBeaconAddr, 
		int useBroadcastAddr, casDGIntfIO *pAltOutIn)
{
	int			yes = TRUE;
	struct sockaddr_in	serverAddr;
	int			status;
	aitInt16 		beaconPort;
	ELLLIST			BCastAddrList;

	if (pAltOutIn) {
		this->pAltOutIO = pAltOutIn;
	}
	else {
		this->pAltOutIO = this;
	}

        this->sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (this->sock == INVALID_SOCKET) {
		errMessage(S_cas_noMemory, 
			"CAS: unable to create cast socket\n");
                return S_cas_noMemory;
        }

        status = setsockopt(
                        this->sock,
                        SOL_SOCKET,
                        SO_BROADCAST,
                        (char *)&yes,
                        sizeof(yes));
        if (status<0) {
                errMessage(S_cas_internal,
			"CAS: unable to set up cast socket\n");
                return S_cas_internal;
        }

	{
		/*
		 *
		 * this allows for faster connects by queuing
		 * additional incomming UDP search frames
		 *
		 * this allocates a 32k buffer
		 * (uses a power of two)
		 */
		int size = 1u<<15u;
        	status = setsockopt(
               	         	this->sock,
                        	SOL_SOCKET,
                        	SO_RCVBUF,
                        	(char *)&size,
                        	sizeof(size));
		if (status<0) {
			errMessage(S_cas_internal,
				"CAS: unable to set cast socket size\n");
		}
	}

        /*
         * release the port in case we exit early. Also if
         * on a kernel with MULTICAST mods then we can have
         * two UDP servers on the same port number (requires
         * setting SO_REUSEADDR prior to the bind step below).
         */
        status = setsockopt(
                        this->sock,
                        SOL_SOCKET,
                        SO_REUSEADDR,
                        (char *) &yes,
                        sizeof (yes));
        if (status<0) {
                errMessage(S_cas_internal,
			"CAS: unable to set SO_REUSEADDR on UDP socket?\n");
        }

	//
	// Fetch port configuration from EPICS environment variables
	//
	if (envGetConfigParamPtr(&EPICS_CAS_SERVER_PORT)) {
		this->dgPort = caFetchPortConfig(&EPICS_CAS_SERVER_PORT, 
					CA_SERVER_PORT);
	}
	else {
		this->dgPort = caFetchPortConfig(&EPICS_CA_SERVER_PORT, 
					CA_SERVER_PORT);
	}
	beaconPort = caFetchPortConfig(&EPICS_CA_REPEATER_PORT, 
					CA_REPEATER_PORT);

	/*
	 * discover beacon addresses associated with this interface
	 */
	serverAddr = addr;
	if (autoBeaconAddr || useBroadcastAddr) {
		ellInit(&BCastAddrList);
		caDiscoverInterfaces(
				&BCastAddrList,
				this->sock,
				beaconPort,
				serverAddr.sin_addr); /* match addr */
		if (useBroadcastAddr) {
			caAddrNode *pAddr;
			if (ellCount(&BCastAddrList)!=1) {
				return S_cas_noInterface;
			}
			pAddr = (caAddrNode *) ellFirst(&BCastAddrList);
			serverAddr.sin_addr = pAddr->destAddr.in.sin_addr; 
		}
		if (autoBeaconAddr) {
			ellConcat(&this->beaconAddrList, &BCastAddrList);
		}
		else {
			ellFree(&BCastAddrList);
		}
	}

        serverAddr.sin_port = htons (this->dgPort);
        status = bind(
                        this->sock,
                        (struct sockaddr *)&serverAddr,
                        sizeof (serverAddr));
        if (status<0) {
		errPrintf(S_cas_bindFail,
			__FILE__, __LINE__,
                        "- bind UDP IP addr=%s port=%u failed because %s",
			inet_ntoa(serverAddr.sin_addr),
			(unsigned) this->dgPort,
                        strerror(SOCKERRNO));
                return S_cas_bindFail;
        }

	if (addConfigBeaconAddr) {

		/*
		 * by default use EPICS_CA_ADDR_LIST for the
		 * beacon address list
		 */
		const ENV_PARAM *pParam;

		if (envGetConfigParamPtr(&EPICS_CAS_INTF_ADDR_LIST) || 
			envGetConfigParamPtr(&EPICS_CAS_BEACON_ADDR_LIST)) {
			pParam = &EPICS_CAS_BEACON_ADDR_LIST;
		}
		else {
			pParam = &EPICS_CA_ADDR_LIST;
		}

		/* 
		 * add in the configured addresses
		 */
		caAddConfiguredAddr(
			&this->beaconAddrList,
			pParam,
			this->sock,
			beaconPort);
	}

	this->connectWithThisPort = connectWithThisPortIn;
        this->sockState=casOnLine;

	return S_cas_success;
}

//
// use an initialize routine ?
//
casDGIntfIO::~casDGIntfIO()
{
        if(this->sock!=INVALID_SOCKET){
                socket_close(this->sock);
        }

	ellFree(&this->beaconAddrList);
}

//
// casDGIntfIO::clientHostName()
//
void casDGIntfIO::clientHostName (char *pBuf, unsigned /* bufSize */) const
{
	//
	// should eventually get the address of the last DG
	// received
	//
	pBuf[0] = '\0';
}

//
// casDGIntfIO::show()
//
void casDGIntfIO::osdShow (unsigned level) const
{
	printf ("casDGIntfIO at %p\n", this);
	if (level>=1u) {
		char buf[64];
		this->clientHostName(buf, sizeof(buf));
        	printf("Client Host=%s\n", buf);
	}
}

//
// casDGIntfIO::xSetNonBlocking()
//
void casDGIntfIO::xSetNonBlocking() 
{
        int status;
        int yes = TRUE;
 
        if (this->sockState!=casOnLine) {
                return;
        }
 
        status = socket_ioctl(this->sock, FIONBIO, &yes);
        if (status<0) {
                ca_printf("%s:CAS: UDP non blocking IO set fail because \"%s\"\n",
                                __FILE__, strerror(SOCKERRNO));
                this->sockState = casOffLine;
        }
}

//
// casDGIntfIO::osdRecv()
//
xRecvStatus casDGIntfIO::osdRecv(char *pBuf, bufSizeT size, 
	bufSizeT &actualSize, caNetAddr &fromOut)
{
	int status;
	int addrSize;
	struct sockaddr from;

        if (this->sockState!=casOnLine) {
                return xRecvDisconnect;
        }

	addrSize = sizeof(from);
        status = recvfrom(this->sock, pBuf, size, 0,
                        &from, &addrSize);
        if (status<0) {
                if(SOCKERRNO == EWOULDBLOCK){
			actualSize = 0u;
			return xRecvOK;
                }
                else {
                        ca_printf("CAS: UDP recv error was %s",
				strerror(SOCKERRNO));
			actualSize = 0u;
                        return xRecvOK;
                }
        }

	fromOut = from;
	actualSize = (bufSizeT) status;
	return xRecvOK;
}

//
// casDGIntfIO::osdSend()
//
xSendStatus casDGIntfIO::osdSend(const char *pBuf, bufSizeT size, 
				bufSizeT &actualSize, const caNetAddr &to)
{
        int		status;
        int		anerrno;

        if (this->sockState!=casOnLine) {
                return xSendDisconnect;
        }

        if (size==0u) {
		actualSize = 0u;
                return xSendOK;
        }

	//
	// (char *) cast below is for brain dead wrs prototype
	//
	struct sockaddr dest = to;
        status = sendto(this->sock, (char *) pBuf, size, 0,
                        &dest, sizeof(dest));
	if (status>0) {
        	if (size != (unsigned) status) {
                        ca_printf ("CAS: partial UDP msg discarded??\n");
		}
	}
	else if (status==0) {
		this->sockState = casOffLine;
		ca_printf ("CAS: UDP send returns zero??\n");
		return xSendDisconnect;
	}
	else {
		anerrno = SOCKERRNO;

		if (anerrno != EWOULDBLOCK) {
			char buf[64];
			this->clientHostName(buf, sizeof(buf));
			ca_printf(
	"CAS: UDP socket send to \"%s\" failed because \"%s\"\n",
				buf, strerror(anerrno));
		}
	}

	actualSize = size;
	return xSendOK;
}


//
// casDGIntfIO::sendBeacon()
// 
void casDGIntfIO::sendBeacon(char &msg, unsigned length, aitUint32 &m_ipa, aitUint16 &m_port) 
{
        caAddrNode      *pAddr;
        int             status;

        if (this->sockState!=casOnLine) {
                return;
        }

        for(    pAddr = (caAddrNode *)ellFirst(&this->beaconAddrList);
                pAddr;
                pAddr = (caAddrNode *)ellNext(&pAddr->node)) {

                m_ipa = htonl(pAddr->srcAddr.in.sin_addr.s_addr);
		m_port = htons(this->dgPort);
                status = sendto(
                                this->sock,
                                &msg,
                                length,
                                0,
                                &pAddr->destAddr.sa,
                                sizeof(pAddr->destAddr.sa));
                if (status < 0) {
			char buf[64];
			ipAddrToA(&pAddr->destAddr.in, buf, sizeof(buf));

                        ca_printf(
        "CAS:beacon error was \"%s\" dest=%s sock=%d\n",
                                strerror(SOCKERRNO),
				buf,
                                this->sock);
                }
        }
}

//
// casDGIntfIO::optimumBufferSize()
//
bufSizeT casDGIntfIO::optimumBufferSize () 
{

#if 1
	//
	// must update client before the message size can be
	// increased here
	//
	return MAX_UDP;
#else
        int n;
        int size;
        int status;

        if (this->sockState!=casOnLine) {
                return MAX_UDP;
        }

        /* fetch the TCP send buffer size */
        n = sizeof(size);
        status = getsockopt(
                        this->sock,
                        SOL_SOCKET,
                        SO_SNDBUF,
                        (char *)&size,
                        &n);
        if(status < 0 || n != sizeof(size)){
                size = MAX_UDP;
        }

        if (size<=0) {
                size = MAX_UDP;
        }
        return (bufSizeT) size;
#endif
}


//
// casDGIntfIO::state()
//
casIOState casDGIntfIO::state() const 
{
	return this->sockState;
}

//
// casDGIntfIO::getFD()
//
int casDGIntfIO::getFD() const
{
	return this->sock;
}

//
// casDGIntfIO::serverPortNumber()
//
// the server's port number
// (to be used for connection requests)
//
unsigned casDGIntfIO::serverPortNumber()
{
	return this->connectWithThisPort;
}

//
// this was moved from casIODIL.h to here because
// of problems inline functions and g++ -g 2.7.2
//
// casDGIntfIO::processDG()
//
void casDGIntfIO::processDG()
{
        assert(this->pAltOutIO);
 
        //
        // process the request DG and send a response DG
        // if it is required
        //
        this->client.processDG(*this,*this->pAltOutIO);
}

