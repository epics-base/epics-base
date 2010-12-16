/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 */

//
// Should I fetch the MTU from the outgoing interface?
//

#include "addrList.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "casDGIntfIO.h"
#include "ipIgnoreEntry.h"

static void  forcePort (ELLLIST *pList, unsigned short port)
{
    osiSockAddrNode *pNode;

    pNode  = reinterpret_cast < osiSockAddrNode * > ( ellFirst ( pList ) );
    while ( pNode ) {
        if ( pNode->addr.sa.sa_family == AF_INET ) {
            pNode->addr.ia.sin_port = htons (port);
        }
        pNode = reinterpret_cast < osiSockAddrNode * > ( ellNext ( &pNode->node ) );
    }
}


casDGIntfIO::casDGIntfIO ( caServerI & serverIn, clientBufMemoryManager & memMgr,
    const caNetAddr & addr, bool autoBeaconAddr, bool addConfigBeaconAddr ) :
    casDGClient ( serverIn, memMgr )
{
    ELLLIST BCastAddrList;
    osiSockAddr serverAddr;
    osiSockAddr serverBCastAddr;
    unsigned short beaconPort;
    int status;
    
    ellInit ( &BCastAddrList );
    ellInit ( &this->beaconAddrList );
    
    if ( ! osiSockAttach () ) {
        throw S_cas_internal;
    }
    
    this->sock = casDGIntfIO::makeSockDG();
    if (this->sock==INVALID_SOCKET) {
        throw S_cas_internal;
    }

    this->beaconSock = casDGIntfIO::makeSockDG();
    if (this->beaconSock==INVALID_SOCKET) {
        epicsSocketDestroy (this->sock);
        throw S_cas_internal;
    }

    {
        // this connect is to supress a warning message on Linux
        // when we shutdown the read side of the socket. If it
        // fails (and it will on old ip kernels) we just ignore 
        // the failure.
        osiSockAddr sockAddr;
        sockAddr.ia.sin_family = AF_UNSPEC;
        sockAddr.ia.sin_port = htons ( 0 );
        sockAddr.ia.sin_addr.s_addr = htonl ( 0 );
        connect ( this->beaconSock,
		        & sockAddr.sa, sizeof ( sockAddr.sa ) );
        shutdown ( this->beaconSock, SHUT_RD );
    }

    //
    // Fetch port configuration from EPICS environment variables
    //
    if (envGetConfigParamPtr(&EPICS_CAS_SERVER_PORT)) {
        this->dgPort = envGetInetPortConfigParam (&EPICS_CAS_SERVER_PORT, 
            static_cast <unsigned short> (CA_SERVER_PORT));
    }
    else {
        this->dgPort = envGetInetPortConfigParam (&EPICS_CA_SERVER_PORT, 
            static_cast <unsigned short> (CA_SERVER_PORT));
    }
    if (envGetConfigParamPtr(&EPICS_CAS_BEACON_PORT)) {
        beaconPort = envGetInetPortConfigParam (&EPICS_CAS_BEACON_PORT, 
            static_cast <unsigned short> (CA_REPEATER_PORT));
    }
    else {
        beaconPort = envGetInetPortConfigParam (&EPICS_CA_REPEATER_PORT, 
            static_cast <unsigned short> (CA_REPEATER_PORT));
    }

    //
    // set up the primary address of the server
    //
    serverAddr.ia = addr;
    serverAddr.ia.sin_port = htons (this->dgPort);
    
    //
    // discover beacon addresses associated with this interface
    //
    {
        osiSockAddrNode *pAddr;
        ELLLIST tmpList;

        ellInit ( &tmpList );
        osiSockDiscoverBroadcastAddresses (&tmpList, 
            this->sock, &serverAddr); // match addr 
        forcePort ( &tmpList, beaconPort );
		removeDuplicateAddresses ( &BCastAddrList, &tmpList, 1 );
        if (ellCount(&BCastAddrList)<1) {
            errMessage (S_cas_noInterface, "- unable to continue");
            epicsSocketDestroy (this->sock);
            throw S_cas_noInterface;
        }
        pAddr = reinterpret_cast < osiSockAddrNode * > ( ellFirst ( &BCastAddrList ) );
        serverBCastAddr.ia = pAddr->addr.ia; 
        serverBCastAddr.ia.sin_port = htons (this->dgPort);

        if ( ! autoBeaconAddr ) {
            // avoid use of ellFree because problems on windows occur if the
            // free is in a different DLL than the malloc
            while ( ELLNODE * pnode = ellGet ( & BCastAddrList ) ) {
                free ( pnode );
            }
        }
    }
    
    status = bind ( this->sock, &serverAddr.sa, sizeof (serverAddr) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        char buf[64];
        ipAddrToA ( &serverAddr.ia, buf, sizeof ( buf ) );
        errPrintf ( S_cas_bindFail, __FILE__, __LINE__, 
            "- bind UDP IP addr=%s failed because %s", buf, sockErrBuf );
        epicsSocketDestroy (this->sock);
        throw S_cas_bindFail;
    }
    
    if ( addConfigBeaconAddr ) {
        
        //
        // by default use EPICS_CA_ADDR_LIST for the
        // beacon address list
        //
        const ENV_PARAM *pParam;
        
        if ( envGetConfigParamPtr ( & EPICS_CAS_INTF_ADDR_LIST ) || 
            envGetConfigParamPtr ( & EPICS_CAS_BEACON_ADDR_LIST ) ) {
            pParam = & EPICS_CAS_BEACON_ADDR_LIST;
        }
        else {
            pParam = & EPICS_CA_ADDR_LIST;
        }
        
        // 
        // add in the configured addresses
        //
        addAddrToChannelAccessAddressList (
            & BCastAddrList, pParam, beaconPort, pParam == & EPICS_CA_ADDR_LIST );
    }
 
    removeDuplicateAddresses ( & this->beaconAddrList, & BCastAddrList, 0 );

    {
        ELLLIST parsed, filtered;
        ellInit ( & parsed );
        ellInit ( & filtered );
        // we dont care what port they are coming from
        addAddrToChannelAccessAddressList ( & parsed, & EPICS_CAS_IGNORE_ADDR_LIST, 0, false );
        removeDuplicateAddresses ( & filtered, & parsed, true );

        while ( ELLNODE * pRawNode  = ellGet ( & filtered ) ) {
		    STATIC_ASSERT ( offsetof (osiSockAddrNode, node) == 0 );
		    osiSockAddrNode * pNode = reinterpret_cast < osiSockAddrNode * > ( pRawNode );
            if ( pNode->addr.sa.sa_family == AF_INET ) {
                ipIgnoreEntry * pIPI = new ( this->ipIgnoreEntryFreeList )
                                ipIgnoreEntry ( pNode->addr.ia.sin_addr.s_addr );
                this->ignoreTable.add ( * pIPI );
            }
            else {
                errlogPrintf ( 
                    "Expected IP V4 address - EPICS_CAS_IGNORE_ADDR_LIST entry ignored\n" );
            }
            free ( pNode );
        }
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
    // conclude that the Solaris implementation is at least
    // partially broken.
    //
    // WIN32 specific:
    // On windows this appears to only create problems because
    // they allow broadcast to be received when
    // binding to a particular interface's IP address, and
    // always fill in this interface's address as the reply 
    // address.
    // 
#if !defined(_WIN32)
	if (serverAddr.ia.sin_addr.s_addr != htonl(INADDR_ANY)) {

        this->bcastRecvSock = casDGIntfIO::makeSockDG ();
        if (this->bcastRecvSock==INVALID_SOCKET) {
            epicsSocketDestroy (this->sock);
            throw S_cas_internal;
        }

        status = bind ( this->bcastRecvSock, &serverBCastAddr.sa,
                        sizeof (serverBCastAddr.sa) );
        if (status<0) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            char buf[64];
            ipAddrToA ( & serverBCastAddr.ia, buf, sizeof ( buf ) );
            errPrintf ( S_cas_bindFail, __FILE__, __LINE__,
                "- bind UDP IP addr=%s failed because %s", 
                buf, sockErrBuf );
            epicsSocketDestroy ( this->sock );
            epicsSocketDestroy ( this->bcastRecvSock );
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
    if ( this->sock != INVALID_SOCKET ) {
        epicsSocketDestroy ( this->sock );
    }

    if ( this->bcastRecvSock != INVALID_SOCKET ) {
        epicsSocketDestroy ( this->bcastRecvSock );
    }

    if ( this->beaconSock != INVALID_SOCKET ) {
        epicsSocketDestroy ( this->beaconSock );
    }
    
    // avoid use of ellFree because problems on windows occur if the
    // free is in a different DLL than the malloc
    ELLNODE * nnode = this->beaconAddrList.node.next;
    while ( nnode )
    {
        ELLNODE * pnode = nnode;
        nnode = nnode->next;
        free ( pnode );
    }

    tsSLList < ipIgnoreEntry > tmp;
    this->ignoreTable.removeAll ( tmp );
    while ( ipIgnoreEntry * pEntry = tmp.get() ) {
        pEntry->~ipIgnoreEntry ();
        this->ipIgnoreEntryFreeList.release ( pEntry );
    }
    
    osiSockRelease ();
}

void casDGIntfIO::show (unsigned level) const
{
	printf ( "casDGIntfIO at %p\n", 
        static_cast <const void *> ( this ) );
    printChannelAccessAddressList (&this->beaconAddrList);
    this->casDGClient::show (level);
}

void casDGIntfIO::xSetNonBlocking() 
{
    osiSockIoctl_t yes = true;
    
    int status = socket_ioctl ( this->sock, FIONBIO, &yes ); // X aCC 392
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "%s:CAS: UDP non blocking IO set fail because \"%s\"\n",
            __FILE__, sockErrBuf );
    }

    if ( this->bcastRecvSock != INVALID_SOCKET ) {
        yes = true;
        int status = socket_ioctl ( this->bcastRecvSock, // X aCC 392
                                    FIONBIO, &yes );
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "%s:CAS: Broadcast receive UDP non blocking IO set failed because \"%s\"\n",
                __FILE__, sockErrBuf );
        }
    }
}

inBufClient::fillCondition
casDGIntfIO::osdRecv ( char * pBufIn, bufSizeT size, // X aCC 361
    fillParameter parm, bufSizeT & actualSize, caNetAddr & fromOut )
{
    int status;
    osiSocklen_t addrSize;
    sockaddr addr;
    SOCKET sockThisTime;

    if ( parm == fpUseBroadcastInterface ) {
        sockThisTime = this->bcastRecvSock;
    }
    else {
        sockThisTime = this->sock;
    }

    addrSize = ( osiSocklen_t ) sizeof ( addr );
    status = recvfrom ( sockThisTime, pBufIn, size, 0,
                       &addr, &addrSize );
    if ( status <= 0 ) {
        if ( status < 0 ) {
            if ( SOCKERRNO != SOCK_EWOULDBLOCK ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf ( "CAS: UDP recv error was \"%s\"\n", sockErrBuf );
            }
        }
        return casFillNone;
    }
    else {
        // filter out and discard frames received from the ignore list
        if ( this->ignoreTable.numEntriesInstalled () > 0 ) {
            if ( addr.sa_family == AF_INET ) {
                sockaddr_in * pIP = 
                    reinterpret_cast < sockaddr_in * > ( & addr );
                ipIgnoreEntry comapre ( pIP->sin_addr.s_addr );
                if ( this->ignoreTable.lookup ( comapre ) ) {
                    return casFillNone;
                }
            }
        }
        fromOut = addr;
        actualSize = static_cast < bufSizeT > ( status );
        return casFillProgress;
    }
}

outBufClient::flushCondition
casDGIntfIO::osdSend ( const char * pBufIn, bufSizeT size, // X aCC 361
                      const caNetAddr & to )
{
    int	status;

    //
    // (char *) cast below is for brain dead wrs prototype
    //
    struct sockaddr dest = to;
    status = sendto ( this->sock, (char *) pBufIn, size, 0,
                     & dest, sizeof ( dest ) );
    if ( status >= 0 ) {
        assert ( size == (unsigned) status );
        return outBufClient::flushProgress;
    }
    else {
        if ( SOCKERRNO != SOCK_EWOULDBLOCK ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            char buf[64];
            sockAddrToA ( & dest, buf, sizeof ( buf ) );
            errlogPrintf (
                "CAS: UDP socket send to \"%s\" failed because \"%s\"\n",
                buf, sockErrBuf );
        }
        return outBufClient::flushNone;
    }
}
    
bufSizeT casDGIntfIO ::
    dgInBytesPending () const 
{
	int status;
	osiSockIoctl_t nchars = 0;

	status = socket_ioctl ( this->sock, FIONREAD, & nchars ); 
	if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
		errlogPrintf ( "CAS: FIONREAD failed because \"%s\"\n",
			sockErrBuf );
		return 0u;
	}
	else if ( nchars < 0 ) {
		return 0u;
	}
	else {
		return ( bufSizeT ) nchars;
	}
}

void casDGIntfIO::sendBeaconIO ( char & msg, unsigned length,
                                aitUint16 & portField, aitUint32 & addrField ) 
{
    caNetAddr           addr = this->serverAddress ();
    struct sockaddr_in  inetAddr = addr.getSockIP();
	osiSockAddrNode		*pAddr;
	int 			    status;
    char                buf[64];

    portField = inetAddr.sin_port; // the TCP port

    for (pAddr = reinterpret_cast <osiSockAddrNode *> ( ellFirst ( & this->beaconAddrList ) );
         pAddr; pAddr = reinterpret_cast <osiSockAddrNode *> ( ellNext ( & pAddr->node ) ) ) {
        status = connect ( this->beaconSock, &pAddr->addr.sa, sizeof ( pAddr->addr.sa ) );
        if (status<0) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            ipAddrToDottedIP ( & pAddr->addr.ia, buf, sizeof ( buf ) );
            errlogPrintf ( "%s: CA beacon routing (connect to \"%s\") error was \"%s\"\n",
                __FILE__, buf, sockErrBuf );
        }
        else {
            osiSockAddr sockAddr;
            osiSocklen_t size = ( osiSocklen_t ) sizeof ( sockAddr.sa );
            status = getsockname ( this->beaconSock, &sockAddr.sa, &size );
            if ( status < 0 ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf ( "%s: CA beacon routing (getsockname) error was \"%s\"\n",
                    __FILE__, sockErrBuf );
            }
            else if ( sockAddr.sa.sa_family == AF_INET ) {
                addrField = sockAddr.ia.sin_addr.s_addr;

                status = send ( this->beaconSock, &msg, length, 0 );
                if ( status < 0 ) {
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                    ipAddrToA ( &pAddr->addr.ia, buf, sizeof(buf) );
                    errlogPrintf ( "%s: CA beacon (send to \"%s\") error was \"%s\"\n",
                        __FILE__, buf, sockErrBuf );
                }
                else {
                    unsigned statusAsLength = static_cast < unsigned > ( status );
                    assert ( statusAsLength == length );
                }
            }
        }
    }
}

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

bufSizeT casDGIntfIO :: 
    osSendBufferSize () const 
{
    /* fetch the TCP send buffer size */
    int size = MAX_UDP_SEND;
    osiSocklen_t n = sizeof ( size );
    int status = getsockopt( this->sock, SOL_SOCKET, SO_SNDBUF,
                    reinterpret_cast < char * > ( & size ), & n );
    if ( status < 0 || n != sizeof(size) ) {
        size = MAX_UDP_SEND;
    }
    if ( size <= MAX_UDP_SEND ) {
        size = MAX_UDP_SEND;
    }
    return static_cast < bufSizeT > ( size );
}

SOCKET casDGIntfIO::makeSockDG ()
{
    int yes = true;
    int status;
    SOCKET newSock;

    newSock = epicsSocketCreate (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
        epicsSocketDestroy (newSock);
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
        // additional incoming UDP search frames
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
            epicsSocketDestroy (newSock);
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
    epicsSocketEnableAddressUseForDatagramFanout ( newSock );

    return newSock;
}

int casDGIntfIO::getFD() const
{
    return this->sock;
}

