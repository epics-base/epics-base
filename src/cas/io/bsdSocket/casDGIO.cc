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

#include <server.h>

//
// casDGIO::casDGIO()
//
casDGIO::casDGIO()
{
        this->sockState=casOffLine;
	ellInit(&this->destAddrList);
	memset((char *)&this->lastRecvAddr, '\0', sizeof(this->lastRecvAddr));
	this->sock = -1;
	this->lastRecvAddrInit = FALSE;
}

//
// casDGIO::init()
//
caStatus casDGIO::init()
{
	int		yes = TRUE;
	caAddr		serverAddr;
	int		status;
	unsigned short 	port;

        this->sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (this->sock < 0) {
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

        memset ((char *)&serverAddr, '\0', sizeof(serverAddr));
        serverAddr.sa.sa_family = AF_INET;
        serverAddr.in.sin_addr.s_addr = INADDR_ANY;
	port = caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);
        serverAddr.in.sin_port = htons (port);

        status = bind(
                        this->sock,
                        &serverAddr.sa,
                        sizeof (serverAddr.sa));
        if (status<0) {
		errPrintf(S_cas_portInUse,
			__FILE__, __LINE__,
                        "CAS: bind to the broadcast port = %u fail: %s\n",
			(unsigned) port,
                        strerror(SOCKERRNO));
                return S_cas_portInUse;
        }

	port = caFetchPortConfig(&EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT);

        caDiscoverInterfaces(
                        &this->destAddrList,
                        this->sock,
                        port);

        caAddConfiguredAddr(
                &this->destAddrList,
                &EPICS_CA_ADDR_LIST,
                this->sock,
                port);

#       if defined(DEBUG)
                caPrintAddrList(&destAddrList);
#       endif
        this->sockState=casOnLine;

	return S_cas_success;
}

//
// use an initialize routine ?
//
casDGIO::~casDGIO()
{
        caAddrNode      *pAddr;

        if(this->sock>=0){
                socket_close(this->sock);
        }

	while ( (pAddr = (caAddrNode *)ellGet(&this->destAddrList)) ) {
		free((char *)pAddr);
	}
}

//
// casDGIO::hostName()
//
void casDGIO::hostNameFromAddr (char *pBuf, unsigned bufSize) 
{
	hostNameFromIPAddr(&this->lastRecvAddr, pBuf, bufSize);
}

//
// casDGIO::show()
//
void casDGIO::osdShow (unsigned level) const
{
	printf ("casDGIO at %x\n", (unsigned) this);
	if (level>=1u) {
        	printf(
               		"client address=%x, port=%x\n",
                	(unsigned) ntohl(this->lastRecvAddr.in.sin_addr.s_addr),
                	ntohs(this->lastRecvAddr.in.sin_port));
	}
}

//
// casDGIO::setNonBlocking()
//
void casDGIO::setNonBlocking() 
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
// casDGIO::osdRecv()
//
xRecvStatus casDGIO::osdRecv(char *pBuf, bufSizeT size, bufSizeT &actualSize)
{
	int status;
	int addrSize;

        if (this->sockState!=casOnLine) {
                return xRecvDisconnect;
        }

	addrSize = sizeof(this->lastRecvAddr.sa);
        status = recvfrom(this->sock, pBuf, size, 0,
                        &this->lastRecvAddr.sa, &addrSize);
        if (status<0) {
                if(SOCKERRNO == EWOULDBLOCK){
			actualSize = 0u;
			return xRecvOK;
                }
                else {
                        ca_printf("CAS: UDP recv error",strerror(SOCKERRNO));
			actualSize = 0u;
                        return xRecvOK;
                }
        }

	this->lastRecvAddrInit = TRUE;

#if 0
        if(pRsrv->CASDEBUG>1){
                char buf[64];

                (*pRsrv->osm.hostName)
                        (&pCastClient->addr, buf, sizeof(buf));
                ca_printf(
                        "CAS: incomming %d byte UDP msg from %s\n",
                        pCastClient->pRecv->cnt,
                        buf);
        }
#endif

	actualSize = (bufSizeT) status;
	return xRecvOK;
}

//
// casDGIO::osdSend()
//
xSendStatus casDGIO::osdSend(const char *pBuf, bufSizeT size, 
				bufSizeT &actualSize)
{
        int	status;
        int	anerrno;

	assert(this->lastRecvAddrInit);

#if 0
        if(this->pCAS->debugLevel>2u){
                char    buf[64];

                (*pCAS->osm.hostName) (
                        &this->addr,
                        buf,
                        sizeof(buf));
                ca_printf(
                        "CAS: Sending a %d byte reply to %s\n",
                        this->pSend->stk,
                        buf);
        }
#endif

        if (this->sockState!=casOnLine) {
                return xSendDisconnect;
        }

        if (size==0u) {
		actualSize = 0u;
                return xSendOK;
        }

        status = sendto(this->sock, pBuf, size, 0,
                        &this->lastRecvAddr.sa,
                        sizeof(this->lastRecvAddr.sa));
	if (status>0) {
        	if (size != (unsigned) status) {
                        printf ("CAS: partial UDP msg discarded??\n");
		}
	}
	else if (status==0) {
		this->sockState = casOffLine;
		printf ("CAS: UDP send returns zero??\n");
		return xSendDisconnect;
	}
	else {
		anerrno = SOCKERRNO;

		if (anerrno != EWOULDBLOCK) {
			ca_printf(
				"CAS: UDP send failed \"%s\"\n",
				strerror(anerrno));
		}
	}

	actualSize = size;
	return xSendOK;
}


//
// casDGIO::sendBeacon()
// 
void casDGIO::sendBeacon(char &msg, unsigned length, aitUint32 &m_avail) 
{
        caAddrNode      *pAddr;
        int             status;

        if (this->sockState!=casOnLine) {
                return;
        }

        for(    pAddr = (caAddrNode *)ellFirst(&this->destAddrList);
                pAddr;
                pAddr = (caAddrNode *)ellNext(&pAddr->node)) {

                m_avail = htonl(pAddr->srcAddr.in.sin_addr.s_addr);

                status = sendto(
                                this->sock,
                                &msg,
                                length,
                                0,
                                &pAddr->destAddr.sa,
                                sizeof(pAddr->destAddr.sa));
                if (status < 0) {
                        ca_printf(
        "CAS:beacon error was \"%s\" at addr=%x sock=%d\n",
                                strerror(SOCKERRNO),
                                pAddr->destAddr.in.sin_addr.s_addr,
                                this->sock);
                }
        }
}

//
// casDGIO::optimumBufferSize()
//
// this returns 9000 on the suns - perhaps this will result in
// to much IP fragmentation
//
bufSizeT casDGIO::optimumBufferSize ()
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
// casDGIO::state()
//
casIOState casDGIO::state() const 
{
	return this->sockState;
}

//
// casDGIO::getFileDescriptor()
//
int casDGIO::getFileDescriptor() const
{
	return this->sock;
}

