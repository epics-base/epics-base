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
#include "addrList.h"
#include "bsdSocketResource.h"

//
// casDGIntfIO::casDGIntfIO()
//
casDGIntfIO::casDGIntfIO (caServerI &serverIn, const caNetAddr &addr, 
                          bool autoBeaconAddr, bool addConfigBeaconAddr) :
    casDGClient (serverIn)
{
    struct sockaddr_in serverAddr;
    struct sockaddr_in serverBCastAddr;
    int status;
    unsigned short beaconPort;
    
    ellInit(&this->beaconAddrList);
    
    if (!bsdSockAttach()) {
        throw S_cas_internal;
    }
    
    this->sock = casDGIntfIO::makeSockDG();
    if (this->sock==INVALID_SOCKET) {
        throw S_cas_internal;
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

    //
    // set up the primary address of the server
    //
    serverAddr = addr;
    serverAddr.sin_port = htons (this->dgPort);
    
    //
    // discover beacon addresses associated with this interface
    //
    {
        ELLLIST BCastAddrList;
        caAddrNode *pAddr;

        ellInit (&BCastAddrList);
        caDiscoverInterfaces (&BCastAddrList, this->sock, beaconPort,
            serverAddr.sin_addr); // match addr 

        if (ellCount(&BCastAddrList)<1) {
            errMessage (S_cas_noInterface, "unable to continue");
            socket_close (this->sock);
            throw S_cas_noInterface;
        }
        pAddr = (caAddrNode *) ellFirst (&BCastAddrList);
        serverBCastAddr = pAddr->destAddr.in; 
        serverBCastAddr.sin_port = htons (this->dgPort);

        if (autoBeaconAddr) {
            ellConcat(&this->beaconAddrList, &BCastAddrList);
        }
        else {
            ellFree(&BCastAddrList);
        }
    }
    
    status = bind(this->sock, (struct sockaddr *)&serverAddr,
        sizeof (serverAddr));
    if (status<0) {
        char buf[64];
        int errnoCpy = SOCKERRNO;
        ipAddrToA (&serverAddr, buf, sizeof(buf));
        errPrintf (S_cas_bindFail, __FILE__, __LINE__,
            ("- bind UDP IP addr=%s failed because %s", 
            buf, SOCKERRSTR(errnoCpy)) );
        socket_close (this->sock);
        throw S_cas_bindFail;
    }
    
    if (addConfigBeaconAddr) {
        
        //
        // by default use EPICS_CA_ADDR_LIST for the
        // beacon address list
        //
        const ENV_PARAM *pParam;
        
        if (envGetConfigParamPtr(&EPICS_CAS_INTF_ADDR_LIST) || 
            envGetConfigParamPtr(&EPICS_CAS_BEACON_ADDR_LIST)) {
            pParam = &EPICS_CAS_BEACON_ADDR_LIST;
        }
        else {
            pParam = &EPICS_CA_ADDR_LIST;
        }
        
        // 
        // add in the configured addresses
        //
        caAddConfiguredAddr(
            &this->beaconAddrList,
            pParam,
            this->sock,
            beaconPort);
    }
       
	//
    // Solaris specific:
	// If they are binding to a particular interface then
	// we will also need to bind to the broadcast address 
	// for that interface (if it has one). This allows
    // broadcast packets to be received, but we must reply
    // through the "normal" UDP binding or the client will
    // appear to receive replies from the broadcast address.
    // Since it should never be valid to fill in the UDP
    // source address as the broadcast address, then we must
    // conclude that the Solaris implementation is broken.
    //
    // WIN32 specific:
    // On windows this appears to only create problems because
    // they correctly allow broadcast to be received when
    // binding to a particular interface's IP address, and
    // always fill in this interface's address as the reply 
    // address.
    // 
#if !defined(_WIN32)
	if (serverAddr.sin_addr.s_addr != htonl(INADDR_ANY)) {

        this->bcastRecvSock = casDGIntfIO::makeSockDG ();
        if (this->bcastRecvSock==INVALID_SOCKET) {
            socket_close (this->sock);
            throw S_cas_internal;
        }

        status = bind (this->bcastRecvSock, (struct sockaddr *)&serverBCastAddr,
                        sizeof (serverAddr));
        if (status<0) {
            char buf[64];
            int errnoCpy = SOCKERRNO;
            ipAddrToA (&serverBCastAddr, buf, sizeof(buf));
            errPrintf (S_cas_bindFail, __FILE__, __LINE__,
                "- bind UDP IP addr=%s failed because %s", 
                buf, SOCKERRSTR(errnoCpy));
            socket_close (this->sock);
            socket_close (this->bcastRecvSock);
            throw S_cas_bindFail;
        }
    }
    else {
        this->bcastRecvSock=INVALID_SOCKET;
    }
#else
    this->bcastRecvSock=INVALID_SOCKET;
#endif
}

//
// use an initialize routine ?
//
casDGIntfIO::~casDGIntfIO()
{
    if (this->sock!=INVALID_SOCKET) {
        socket_close(this->sock);
    }

    if (this->bcastRecvSock!=INVALID_SOCKET) {
        socket_close(this->bcastRecvSock);
    }
    
    ellFree(&this->beaconAddrList);
    
    bsdSockRelease();
}

//
// casDGIntfIO::show()
//
void casDGIntfIO::osdShow (unsigned level) const
{
	printf ("casDGIntfIO at %p\n", this);
}

//
// casDGIntfIO::xSetNonBlocking()
//
void casDGIntfIO::xSetNonBlocking() 
{
    int status;
    osiSockIoctl_t yes = TRUE;
    
    status = socket_ioctl(this->sock, FIONBIO, &yes);
    if (status<0) {
        ca_printf("%s:CAS: UDP non blocking IO set fail because \"%s\"\n",
            __FILE__, SOCKERRSTR(SOCKERRNO));
    }
}

//
// casDGIntfIO::osdRecv()
//
inBuf::fillCondition casDGIntfIO::osdRecv(char *pBuf, bufSizeT size, 
	fillParameter parm, bufSizeT &actualSize, caNetAddr &fromOut)
{
	int status;
	int addrSize;
    sockaddr addr;
    SOCKET sockThisTime;

    if (parm==fpUseBroadcastInterface) {
        sockThisTime = this->bcastRecvSock;
    }
    else {
        sockThisTime = this->sock;
    }

	addrSize = sizeof (addr);
	status = recvfrom (this->sock, pBuf, size, 0,
					&addr, &addrSize);
	if (status<=0) {
        if (status<0) {
            int errnoCpy = SOCKERRNO;
            if( errnoCpy != SOCK_EWOULDBLOCK ){
			    ca_printf("CAS: UDP recv error was %s",
			        SOCKERRSTR(errnoCpy));
            }
		}
        return casFillNone;
	}
    else {
	    fromOut = addr;
	    actualSize = (bufSizeT) status;
	    return casFillProgress;
    }
}

//
// casDGIntfIO::osdSend()
//
outBuf::flushCondition casDGIntfIO::osdSend (const char *pBuf, bufSizeT size, 
				const caNetAddr &to)
{
	int		status;

	//
	// (char *) cast below is for brain dead wrs prototype
	//
	struct sockaddr dest = to;
	status = sendto (this->sock, (char *) pBuf, size, 0,
                        &dest, sizeof(dest));
	if (status>=0) {
        assert ( size == (unsigned) status );
        return outBuf::flushProgress;
	}
	else {
        int errnoCpy = SOCKERRNO;
		if (errnoCpy != SOCK_EWOULDBLOCK) {
			char buf[64];
            sockAddrToA (&dest, buf, sizeof(buf));
			ca_printf (
	"CAS: UDP socket send to \"%s\" failed because \"%s\"\n",
				buf, SOCKERRSTR(errnoCpy));
		}
        return outBuf::flushNone;
	}
}

//
// casDGIntfIO::incommingBytesPresent()
//
// OK to return a size of one here when a datagram is present, and
// zero otherwise.
//
bufSizeT casDGIntfIO::incommingBytesPresent () const
{
	int status;
	osiSockIoctl_t nchars;

	status = socket_ioctl (this->sock, FIONREAD, &nchars);
	if (status<0) {
		ca_printf ("CAS: FIONREAD failed because \"%s\"\n",
			SOCKERRSTR(SOCKERRNO));
		return 0u;
	}
	else if (nchars<0) {
		return 0u;
	}
	else {
		return (bufSizeT) nchars;
	}
}


//
// casDGIntfIO::sendBeacon()
// 
void casDGIntfIO::sendBeaconIO (char &msg, unsigned length, aitUint16 &m_port) 
{
	caAddrNode		*pAddr;
	int 			status;
	
	//
	// the broadcast bound socket is not used here because
	// it will have the wrong source address. 
	//
	for (pAddr = (caAddrNode *)ellFirst(&this->beaconAddrList);
				pAddr; pAddr = (caAddrNode *)ellNext(&pAddr->node)) {
		m_port = htons (this->dgPort);
		status = sendto (this->sock, &msg, length, 0,
					&pAddr->destAddr.sa, sizeof(pAddr->destAddr.sa));
		if (status < 0) {
			char buf[64];
            int errnoCpy = SOCKERRNO;

			ipAddrToA(&pAddr->destAddr.in, buf, sizeof(buf));
			
			ca_printf(
				"CAS:beacon error was \"%s\" dest=%s sock=%d\n",
				SOCKERRSTR(errnoCpy), buf, this->sock);
		}
	}
}

//
// casDGIntfIO::optimumInBufferSize()
//
bufSizeT casDGIntfIO::optimumInBufferSize () 
{
    
#if 1
    //
    // must update client before the message size can be
    // increased here
    //
    return ETHERNET_MAX_UDP;
#else
    int n;
    int size;
    int status;
    
    /* fetch the TCP send buffer size */
    n = sizeof(size);
    status = getsockopt(
        this->sock,
        SOL_SOCKET,
        SO_RCVBUF,
        (char *)&size,
        &n);
    if(status < 0 || n != sizeof(size)){
        size = ETEHRNET_MAX_UDP;
    }
    
    if (size<=0) {
        size = ETHERNET_MAX_UDP;
    }
    return (bufSizeT) size;
#endif
}

//
// casDGIntfIO::optimumOutBufferSize()
//
bufSizeT casDGIntfIO::optimumOutBufferSize () 
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
// casDGIntfIO::makeSockDG ()
//
SOCKET casDGIntfIO::makeSockDG ()
{
    int yes = TRUE;
    int status;
    SOCKET newSock;

    newSock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (newSock == INVALID_SOCKET) {
        errMessage(S_cas_noMemory, "CAS: unable to create cast socket\n");
        return INVALID_SOCKET;
    }
    
    status = setsockopt(
        newSock,
        SOL_SOCKET,
        SO_BROADCAST,
        (char *)&yes,
        sizeof(yes));
    if (status<0) {
        socket_close (newSock);
        errMessage(S_cas_internal,
            "CAS: unable to set up cast socket\n");
        return INVALID_SOCKET;
    }
    
    //
    // some concern that vxWorks will run out of mBuf's
    // if this change is made
    //
    // joh 11-10-98
    //
#if 0
    {
        //
        //
        // this allows for faster connects by queuing
        // additional incomming UDP search frames
        //
        // this allocates a 32k buffer
        // (uses a power of two)
        //
        int size = 1u<<15u;
        status = setsockopt(
            newSock,
            SOL_SOCKET,
            SO_RCVBUF,
            (char *)&size,
            sizeof(size));
        if (status<0) {
            socket_close (newSock);
            errMessage(S_cas_internal,
                "CAS: unable to set cast socket size\n");
            return INVALID_SOCKET;
        }
    }
#endif
    
    //
    // release the port in case we exit early. Also if
    // on a kernel with MULTICAST mods then we can have
    // two UDP servers on the same port number (requires
    // setting SO_REUSEADDR prior to the bind step below).
    //
    status = setsockopt(
        newSock,
        SOL_SOCKET,
        SO_REUSEADDR,
        (char *) &yes,
        sizeof (yes));
    if (status<0) {
        socket_close (newSock);
        errMessage(S_cas_internal,
            "CAS: unable to set SO_REUSEADDR on UDP socket?\n");
        return INVALID_SOCKET;
    }

    return newSock;
}
