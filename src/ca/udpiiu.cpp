
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "envDefs.h"
#include "osiProcess.h"

#define epicsExportSharedSymbols
#include "addrList.h"
#include "caerr.h" // for ECA_NOSEARCHADDR
#include "udpiiu.h"
#undef epicsExportSharedSymbols

#include "iocinf.h"
#include "inetAddrID.h"
#include "cac.h"

// UDP protocol dispatch table
const udpiiu::pProtoStubUDP udpiiu::udpJumpTableCAC [] = 
{
    &udpiiu::noopAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::searchRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::exceptionRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::beaconAction,
    &udpiiu::notHereRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::badUDPRespAction,
    &udpiiu::repeaterAckAction,
};

//
// udpiiu::udpiiu ()
//
udpiiu::udpiiu ( cac &cac, epicsThreadPrivateId isRecvProcessIdIn ) :
    netiiu ( &cac ), isRecvProcessId ( isRecvProcessIdIn ), 
        shutdownCmd ( false ), sockCloseCompleted ( false )
{
    static const unsigned short PORT_ANY = 0u;
    osiSockAddr addr;
    int boolValue = true;
    int status;

    this->repeaterPort = 
        envGetInetPortConfigParam ( &EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT );

    this->serverPort = 
        envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT, CA_SERVER_PORT );

    this->sock = socket ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( this->sock == INVALID_SOCKET ) {
        this->printf ("CAC: unable to create datagram socket because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        throwWithLocation ( noSocket () );
    }

    status = setsockopt ( this->sock, SOL_SOCKET, SO_BROADCAST, 
                (char *) &boolValue, sizeof ( boolValue ) );
    if ( status < 0 ) {
        this->printf ("CAC: IP broadcasting enable failed because = \"%s\"\n",
            SOCKERRSTR ( SOCKERRNO ) );
    }

#if 0
    {
        /*
         * some concern that vxWorks will run out of mBuf's
         * if this change is made joh 11-10-98
         *
         * bump up the UDP recv buffer
         */
        int size = 1u<<15u;
        status = setsockopt ( this->sock, SOL_SOCKET, SO_RCVBUF,
                (char *)&size, sizeof (size) );
        if (status<0) {
            this->printf ("CAC: unable to set socket option SO_RCVBUF because \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
        }
    }
#endif

    /*
     * force a bind to an unconstrained address because we may end
     * up receiving first
     */
    memset ( (char *)&addr, 0 , sizeof (addr) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl (INADDR_ANY); 
    addr.ia.sin_port = htons (PORT_ANY);   
    status = bind (this->sock, &addr.sa, sizeof (addr) );
    if ( status < 0 ) {
        socket_close (this->sock);
        this->printf ("CAC: unable to bind to an unconstrained address because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        throwWithLocation ( noSocket () );
    }
    
    {
        osiSockAddr tmpAddr;
        osiSocklen_t saddr_length = sizeof ( tmpAddr );
        status = getsockname ( this->sock, &tmpAddr.sa, &saddr_length );
        if ( status < 0 ) {
            socket_close ( this->sock );
            this->pCAC ()->printf ( "CAC: getsockname () error was \"%s\"\n", SOCKERRSTR (SOCKERRNO) );
            throwWithLocation ( noSocket () );
        }
        if ( tmpAddr.sa.sa_family != AF_INET) {
            socket_close ( this->sock );
            this->pCAC ()->printf ( "CAC: UDP socket was not inet addr family\n" );
            throwWithLocation ( noSocket () );
        }
        this->localPort = ntohs ( tmpAddr.ia.sin_port );
    }

    this->nBytesInXmitBuf = 0u;

    this->recvThreadExitSignal = epicsEventMustCreate ( epicsEventEmpty );
    if ( ! this->recvThreadExitSignal ) {
        socket_close ( this->sock );
        throwWithLocation ( std::bad_alloc () );
    }

    /*
     * load user and auto configured
     * broadcast address list
     */
    ellInit ( &this->dest );
    configureChannelAccessAddressList ( &this->dest, this->sock, this->serverPort );
    if ( ellCount ( &this->dest ) == 0 ) {
        genLocalExcep ( *this->pCAC (), ECA_NOSEARCHADDR, NULL );
    }
  
    {
        unsigned priorityOfRecv;
        epicsThreadBooleanStatus tbs;

        tbs  = epicsThreadLowestPriorityLevelAbove ( 
            this->pCAC ()->getInitializingThreadsPriority (), &priorityOfRecv );
        if ( tbs != epicsThreadBooleanStatusSuccess ) {
            priorityOfRecv = this->pCAC ()->getInitializingThreadsPriority ();
        }

        this->recvThreadId = epicsThreadCreate ( "CAC-UDP", priorityOfRecv,
                epicsThreadGetStackSize (epicsThreadStackMedium), cacRecvThreadUDP, this );
        if ( this->recvThreadId == 0 ) {
            this->printf ("CA: unable to create UDP receive thread\n");
            epicsEventDestroy (this->recvThreadExitSignal);
            socket_close (this->sock);
            throwWithLocation ( std::bad_alloc () );
        }
    }

    caStartRepeaterIfNotInstalled ( this->repeaterPort );
}

/*
 *  udpiiu::~udpiiu ()
 */
udpiiu::~udpiiu ()
{
    // closes the udp socket and waits for its recv thread to exit
    this->shutdown ();

    epicsEventDestroy ( this->recvThreadExitSignal );

    ellFree ( &this->dest );
    
    if ( ! this->sockCloseCompleted ) {
        socket_close ( this->sock );
    }
}

//
//  udpiiu::recvMsg ()
//
void udpiiu::recvMsg ()
{
    osiSockAddr src;
    osiSocklen_t src_size = sizeof ( src );
    int status;

    status = recvfrom ( this->sock, this->recvBuf, sizeof ( this->recvBuf ), 0,
                        &src.sa, &src_size );
    if ( status <= 0 ) {

        if ( status == 0 ) {
            return;
        }

        int errnoCpy = SOCKERRNO;

        if ( errnoCpy == SOCK_SHUTDOWN ) {
            return;
        }
        if ( errnoCpy == SOCK_ENOTSOCK ) {
            return;
        }
        if ( errnoCpy == SOCK_EBADF ) {
            return;
        }
        if ( errnoCpy == SOCK_EINTR ) {
            return;
        }
#       ifdef linux
            /*
             * Avoid spurious ECONNREFUSED bug
             * in linux
             */
            if ( errnoCpy == SOCK_ECONNREFUSED ) {
                return;
            }
#       endif
        this->printf ( "Unexpected UDP recv error was \"%s\"\n", 
            SOCKERRSTR (errnoCpy) );
    }
    else if ( status > 0 ) {
        this->postMsg ( src, this->recvBuf, (arrayElementCount) status, 
            epicsTime::getCurrent() );
    }
    return;
}

/*
 *  cacRecvThreadUDP ()
 */
extern "C" void cacRecvThreadUDP ( void *pParam )
{
    udpiiu *piiu = (udpiiu *) pParam;
    epicsThreadPrivateSet ( piiu->isRecvProcessId, pParam );
    do {
        piiu->recvMsg ();
    } while ( ! piiu->shutdownCmd );
    epicsEventSignal ( piiu->recvThreadExitSignal );
}

/*
 *  udpiiu::repeaterRegistrationMessage ()
 *
 *  register with the repeater 
 */
void udpiiu::repeaterRegistrationMessage ( unsigned attemptNumber )
{
    caRepeaterRegistrationMessage ( this->sock, this->repeaterPort, attemptNumber );
}

/*
 *  caRepeaterRegistrationMessage ()
 *
 *  register with the repeater 
 */
void epicsShareAPI caRepeaterRegistrationMessage ( 
           SOCKET sock, unsigned repeaterPort, unsigned attemptNumber )
{
    caHdr msg;
    osiSockAddr saddr;
    int status;
    int len;

    /*
     * In 3.13 beta 11 and before the CA repeater calls local_addr() 
     * to determine a local address and does not allow registration 
     * messages originating from other addresses. In these 
     * releases local_addr() returned the address of the first enabled
     * interface found, and this address may or may not have been the loop
     * back address. Starting with 3.13 beta 12 local_addr() was
     * changed to always return the address of the first enabled 
     * non-loopback interface because a valid non-loopback local
     * address is required in the beacon messages. Therefore, to 
     * guarantee compatibility with past versions of the repeater
     * we alternate between the address returned by local_addr()
     * and the loopback address here.
     *
     * CA repeaters in R3.13 beta 12 and higher allow
     * either the loopback address or the address returned
     * by local address (the first non-loopback address found)
     */
    if ( attemptNumber & 1 ) {
        saddr = osiLocalAddr ( sock );
        if ( saddr.sa.sa_family != AF_INET ) {
            /*
             * use the loop back address to communicate with the CA repeater
             * if this os does not have interface query capabilities
             *
             * this will only work with 3.13 beta 12 CA repeaters or later
             */
            saddr.ia.sin_family = AF_INET;
            saddr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
            saddr.ia.sin_port = htons ( repeaterPort );   
        }
        else {
            saddr.ia.sin_port = htons ( repeaterPort );   
        }
    }
    else {
        saddr.ia.sin_family = AF_INET;
        saddr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
        saddr.ia.sin_port = htons ( repeaterPort );   
    }

    memset ( (char *) &msg, 0, sizeof (msg) );
    msg.m_cmmd = htons ( REPEATER_REGISTER );
    msg.m_available = saddr.ia.sin_addr.s_addr;

    /*
     * Intentionally sending a zero length message here
     * until most CA repeater daemons have been restarted
     * (and only then will they accept the above protocol)
     * (repeaters began accepting this protocol
     * starting with EPICS 3.12)
     */
#   if defined ( DOES_NOT_ACCEPT_ZERO_LENGTH_UDP )
        len = sizeof (msg);
#   else 
        len = 0;
#   endif 

    status = sendto ( sock, (char *) &msg, len,  
                0, (struct sockaddr *) &saddr, sizeof ( saddr ) );
    if ( status < 0 ) {
        int errnoCpy = SOCKERRNO;
        /*
         * Different OS return different codes when the repeater isnt running.
         * Its ok to supress these messages because I print another warning message
         * if we time out registerring with the repeater.
         *
         * Linux returns SOCK_ECONNREFUSED
         * Windows 2000 returns SOCK_ECONNRESET
         */
        if (    errnoCpy != SOCK_EINTR && 
                errnoCpy != SOCK_ECONNREFUSED && 
                errnoCpy != SOCK_ECONNRESET ) {
            fprintf ( stderr, "error sending registration message to CA repeater daemon was \"%s\"\n", 
                SOCKERRSTR ( errnoCpy ) );
        }
    }
}

/*
 *  caStartRepeaterIfNotInstalled ()
 *
 *  Test for the repeater already installed
 *
 *  NOTE: potential race condition here can result
 *  in two copies of the repeater being spawned
 *  however the repeater detects this, prints a message,
 *  and lets the other task start the repeater.
 *
 *  QUESTION: is there a better way to test for a port in use? 
 *  ANSWER: none that I can find.
 *
 *  Problems with checking for the repeater installed
 *  by attempting to bind a socket to its address
 *  and port.
 *
 *  1) Closed socket may not release the bound port
 *  before the repeater wakes up and tries to grab it.
 *  Attempting to bind the open socket to another port
 *  also does not work.
 *
 *  072392 - problem solved by using SO_REUSEADDR
 */
void epicsShareAPI caStartRepeaterIfNotInstalled ( unsigned repeaterPort )
{
    bool installed = false;
    int status;
    SOCKET tmpSock;
    struct sockaddr_in bd;
    int flag;

    if ( repeaterPort > 0xffff ) {
        fprintf ( stderr, "caStartRepeaterIfNotInstalled () : strange repeater port specified\n" );
        return;
    }

    tmpSock = socket ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( tmpSock != INVALID_SOCKET ) {
        ca_uint16_t port = static_cast < ca_uint16_t > ( repeaterPort );
        memset ( (char *) &bd, 0, sizeof ( bd ) );
        bd.sin_family = AF_INET;
        bd.sin_addr.s_addr = htonl ( INADDR_ANY ); 
        bd.sin_port = htons ( port );   
        status = bind ( tmpSock, (struct sockaddr *) &bd, sizeof ( bd ) );
        if ( status < 0 ) {
            if ( SOCKERRNO == SOCK_EADDRINUSE ) {
                installed = true;
            }
            else {
                fprintf ( stderr, "caStartRepeaterIfNotInstalled () : bind failed\n" );
            }
        }
    }

    /*
     * turn on reuse only after the test so that
     * this works on kernels that support multicast
     */
    flag = true;
    status = setsockopt ( tmpSock, SOL_SOCKET, SO_REUSEADDR, 
                (char *) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        fprintf ( stderr, "caStartRepeaterIfNotInstalled () : set socket option reuseaddr set failed\n" );
    }

    socket_close ( tmpSock );

    if ( ! installed ) {
        
	    /*
	     * This is not called if the repeater is known to be 
	     * already running. (in the event of a race condition 
	     * the 2nd repeater exits when unable to attach to the 
	     * repeater's port)
	     */
        osiSpawnDetachedProcessReturn osptr = 
            osiSpawnDetachedProcess ( "CA Repeater", "caRepeater" );
        if ( osptr == osiSpawnDetachedProcessNoSupport ) {
            epicsThreadId tid;

            tid = epicsThreadCreate ( "CAC-repeater", epicsThreadPriorityLow,
                    epicsThreadGetStackSize ( epicsThreadStackMedium ), caRepeaterThread, 0);
            if ( tid == 0 ) {
                fprintf ( stderr, "caStartRepeaterIfNotInstalled : unable to create CA repeater daemon thread\n" );
            }
        }
        else if ( osptr == osiSpawnDetachedProcessFail ) {
            fprintf ( stderr, "caStartRepeaterIfNotInstalled (): unable to start CA repeater daemon detached process\n" );
        }
    }
}

void udpiiu::shutdown ()
{
    if ( this->shutdownCmd ) {
        return;
    }
    this->shutdownCmd = true;

    caHdr msg;
    msg.m_cmmd = htons ( CA_PROTO_NOOP );
    msg.m_available = htonl ( 0u );
    msg.m_dataType = htons ( 0u );
    msg.m_count = htons ( 0u );
    msg.m_cid = htonl ( 0u );
    msg.m_postsize = htons ( 0u );

    osiSockAddr addr;
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
    addr.ia.sin_port = htons ( this->localPort );

    // send a wakeup msg so the UDP recv thread will exit
    int status = sendto ( this->sock, reinterpret_cast < const char * > ( &msg ),  
            sizeof (msg), 0, &addr.sa, sizeof ( addr.sa ) );
    if ( status < 0 ) {
        // this knocks the UDP input thread out of recv ()
        // on all os except linux
        status = socket_close ( this->sock );
        if ( status == 0 ) {
            this->sockCloseCompleted = true;
        }
        else {
            errlogPrintf ("CAC UDP socket close error was %s\n", 
                SOCKERRSTR ( SOCKERRNO ) );
        }
    }

    // wait for recv threads to exit
    epicsEventMustWait ( this->recvThreadExitSignal );
}

bool udpiiu::badUDPRespAction ( const caHdr &msg, 
    const osiSockAddr &netAddr, const epicsTime &currentTime )
{
    char buf[64];
    sockAddrToDottedIP ( &netAddr.sa, buf, sizeof ( buf ) );
    char date[64];
    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");
    this->printf ( "CAC: undecipherable ( bad msg code %u ) UDP message from %s at %s\n", 
                msg.m_cmmd, buf, date );
    return false;
}

bool udpiiu::noopAction ( const caHdr &, const osiSockAddr &, const epicsTime & )
{
    return true;
}

bool udpiiu::searchRespAction ( const caHdr &msg, 
    const osiSockAddr &addr, const epicsTime &currentTime )
{
    osiSockAddr serverAddr;
    unsigned minorVersion;
    ca_uint16_t *pMinorVersion;

    if ( addr.sa.sa_family != AF_INET ) {
        return false;
    }

    /*
     * Starting with CA V4.1 the minor version number
     * is appended to the end of each search reply.
     * This value is ignored by earlier clients.
     */
    if ( msg.m_postsize >= sizeof (*pMinorVersion) ){
        pMinorVersion = (ca_uint16_t *) ( &msg + 1 );
        minorVersion = ntohs ( *pMinorVersion );      
    }
    else {
        minorVersion = CA_UKN_MINOR_VERSION;
    }

    /*
     * the type field is abused to carry the port number
     * so that we can have multiple servers on one host
     */
    serverAddr.ia.sin_family = AF_INET;
    if ( CA_V48 ( minorVersion ) ) {
        if ( msg.m_cid != INADDR_BROADCAST ) {
            /*
             * Leave address in network byte order (m_cid has not been 
             * converted to the local byte order)
             */
            serverAddr.ia.sin_addr.s_addr = msg.m_cid;
        }
        else {
            serverAddr.ia.sin_addr = addr.ia.sin_addr;
        }
        serverAddr.ia.sin_port = htons ( msg.m_dataType );
    }
    else if ( CA_V45 (minorVersion) ) {
        serverAddr.ia.sin_port = htons ( msg.m_dataType );
        serverAddr.ia.sin_addr = addr.ia.sin_addr;
    }
    else {
        serverAddr.ia.sin_port = htons ( this->serverPort );
        serverAddr.ia.sin_addr = addr.ia.sin_addr;
    }

    if ( CA_V42 ( minorVersion ) ) {
        return this->pCAC ()->lookupChannelAndTransferToTCP 
            ( msg.m_available, msg.m_cid, 0xffff, 
                0, minorVersion, serverAddr, currentTime );
    }
    else {
        return this->pCAC ()->lookupChannelAndTransferToTCP 
            ( msg.m_available, msg.m_cid, msg.m_dataType, 
                msg.m_count, minorVersion, serverAddr, currentTime );
    }
}

bool udpiiu::beaconAction ( const caHdr &msg, 
    const osiSockAddr &net_addr, const epicsTime &currentTime )
{
    struct sockaddr_in ina;

    if ( net_addr.sa.sa_family != AF_INET ) {
        return false;
    }

    /* 
     * this allows a fan-out server to potentially
     * insert the true address of the CA server 
     *
     * old servers:
     *   1) set this field to one of the ip addresses of the host _or_
     *   2) set this field to htonl(INADDR_ANY)
     * new servers:
     *   always set this field to htonl(INADDR_ANY)
     *
     * clients always assume that if this
     * field is set to something that isnt htonl(INADDR_ANY)
     * then it is the overriding IP address of the server.
     */
    ina.sin_family = AF_INET;
    ina.sin_addr.s_addr = msg.m_available;
    if ( msg.m_count != 0 ) {
        ina.sin_port = htons ( msg.m_count );
    }
    else {
        /*
         * old servers dont supply this and the
         * default port must be assumed
         */
        ina.sin_port = htons ( this->serverPort );
    }

    this->pCAC ()->beaconNotify ( ina, currentTime );

    return true;
}

bool udpiiu::repeaterAckAction ( const caHdr &,  
        const osiSockAddr &, const epicsTime &)
{
    this->pCAC ()->repeaterSubscribeConfirmNotify ();
    return true;
}

bool udpiiu::notHereRespAction ( const caHdr &,  
        const osiSockAddr &, const epicsTime & )
{
    return true;
}

bool udpiiu::exceptionRespAction ( const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime &currentTime )
{
    const caHdr &reqMsg = * ( &msg + 1 );
    char name[64];
    sockAddrToDottedIP ( &net_addr.sa, name, sizeof ( name ) );
    char date[64];
    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");

    if ( msg.m_postsize > sizeof ( caHdr ) ){
        errlogPrintf ( "error condition \"%s\" detected by %s with context \"%s\" at %s\n", 
            ca_message ( htonl ( msg.m_available ) ), 
            name, reinterpret_cast <const char *> ( &reqMsg + 1 ), date );
    }
    else{
        errlogPrintf ( "error condition \"%s\" detected by %s at %s\n", 
            ca_message ( htonl ( msg.m_available ) ), name, date );
    }

    return true;
}

void udpiiu::postMsg ( const osiSockAddr &net_addr, 
              char *pInBuf, arrayElementCount blockSize,
              const epicsTime &currentTime)
{
    caHdr *pCurMsg;

    while ( blockSize ) {
        arrayElementCount size;

        if ( blockSize < sizeof ( *pCurMsg ) ) {
            char buf[64];
            sockAddrToDottedIP ( &net_addr.sa, buf, sizeof ( buf ) );
            this->printf (
                "%s: undecipherable (too small) UDP msg from %s ignored\n", __FILE__, 
                            buf );
            return;
        }

        pCurMsg = reinterpret_cast < caHdr * > ( pInBuf );

        /* 
         * fix endian of bytes 
         */
        pCurMsg->m_postsize = ntohs ( pCurMsg->m_postsize );
        pCurMsg->m_cmmd = ntohs ( pCurMsg->m_cmmd );
        pCurMsg->m_dataType = ntohs ( pCurMsg->m_dataType );
        pCurMsg->m_count = ntohs ( pCurMsg->m_count );

#if 0
        printf ( "UDP Cmd=%3d Type=%3d Count=%4d Size=%4d",
            pCurMsg->m_cmmd,
            pCurMsg->m_dataType,
            pCurMsg->m_count,
            pCurMsg->m_postsize );
        printf (" Avail=%8x Cid=%6d\n",
            pCurMsg->m_available,
            pCurMsg->m_cid );
#endif

        size = pCurMsg->m_postsize + sizeof ( *pCurMsg );

        /*
         * dont allow msg body extending beyond frame boundary
         */
        if ( size > blockSize ) {
            char buf[64];
            sockAddrToDottedIP ( &net_addr.sa, buf, sizeof ( buf ) );
            this->printf (
                "%s: undecipherable (payload too small) UDP msg from %s ignored\n", __FILE__, 
                            buf );
            return;
        }

        /*
         * execute the response message
         */
        pProtoStubUDP pStub;
        if ( pCurMsg->m_cmmd < NELEMENTS ( udpJumpTableCAC ) ) {
            pStub = udpJumpTableCAC [pCurMsg->m_cmmd];
        }
        else {
            pStub = &udpiiu::badUDPRespAction;
        }
        bool success = ( this->*pStub ) ( *pCurMsg, net_addr, currentTime );
        if ( ! success ) {
            char buf[256];
            sockAddrToDottedIP ( &net_addr.sa, buf, sizeof ( buf ) );
            this->printf ( "CAC: undecipherable UDP message from %s\n", buf );
            return;
        }

        blockSize -= size;
        pInBuf += size;;
    }
}

/*
 *  udpiiu::pushDatagramMsg ()
 */ 
bool udpiiu::pushDatagramMsg ( const caHdr &msg, const void *pExt, ca_uint16_t extsize )
{
    arrayElementCount   msgsize;
    ca_uint16_t         allignedExtSize;
    caHdr               *pbufmsg;

    allignedExtSize = CA_MESSAGE_ALIGN ( extsize );
    msgsize = sizeof ( caHdr ) + allignedExtSize;


    /* fail out if max message size exceeded */
    if ( msgsize >= sizeof ( this->xmitBuf ) - 7 ) {
        return false;
    }

    if ( msgsize + this->nBytesInXmitBuf > sizeof ( this->xmitBuf ) ) {
        return false;
    }

    pbufmsg = (caHdr *) &this->xmitBuf[this->nBytesInXmitBuf];
    *pbufmsg = msg;
    memcpy ( pbufmsg + 1, pExt, extsize );
    if ( extsize != allignedExtSize ) {
        char *pDest = (char *) ( pbufmsg + 1 );
        memset ( pDest + extsize, '\0', allignedExtSize - extsize );
    }
    pbufmsg->m_postsize = htons ( allignedExtSize );
    this->nBytesInXmitBuf += msgsize;

    return true;
}

void udpiiu::datagramFlush ()
{
    osiSockAddrNode  *pNode;

    if ( this->nBytesInXmitBuf == 0u ) {
        return;
    }

    pNode = (osiSockAddrNode *) ellFirst ( &this->dest );
    while ( pNode ) {
        int status;

        assert ( this->nBytesInXmitBuf <= INT_MAX );
        status = sendto ( this->sock, this->xmitBuf,   
                (int) this->nBytesInXmitBuf, 0, 
                &pNode->addr.sa, sizeof ( pNode->addr.sa ) );
        if ( status != (int) this->nBytesInXmitBuf ) {
            if ( status >= 0 ) {
                this->printf ( "CAC: UDP sendto () call returned strange xmit count?\n" );
                break;
            }
            else {
                int localErrno = SOCKERRNO;

                if ( localErrno == SOCK_EINTR ) {
                    if ( this->shutdownCmd ) {
                        break;
                    }
                    else {
                        continue;
                    }
                }
                else if ( localErrno == SOCK_SHUTDOWN ) {
                    break;
                }
                else if ( localErrno == SOCK_ENOTSOCK ) {
                    break;
                }
                else if ( localErrno == SOCK_EBADF ) {
                    break;
                }
                else {
                    char buf[64];

                    sockAddrToDottedIP ( &pNode->addr.sa, buf, sizeof ( buf ) );

                    this->printf (
                        "CAC: error = \"%s\" sending UDP msg to %s\n",
                        SOCKERRSTR ( localErrno ), buf);
                    break;
                }
            }
        }
        pNode = (osiSockAddrNode *) ellNext ( &pNode->node );
    }

    this->nBytesInXmitBuf = 0u;
}

void udpiiu::show ( unsigned level ) const
{
    ::printf ( "Datagram IO circuit (and disconnected channel repository)\n");
    if ( level > 1u ) {
        this->netiiu::show ( level - 1u );
    }
    if ( level > 2u ) {
        ::printf ("\trepeater port %u\n", this->repeaterPort );
        ::printf ("\tdefault server port %u\n", this->serverPort );
        printChannelAccessAddressList ( &this->dest );
    }
    if ( level > 3u ) {
        ::printf ("\tsocket identifier %d\n", this->sock );
        ::printf ("\tbytes in xmit buffer %u\n", this->nBytesInXmitBuf );
        ::printf ("\tshut down command bool %u\n", this->shutdownCmd );
        ::printf ( "\trecv thread exit signal:\n" );
        epicsEventShow ( this->recvThreadExitSignal, level-3u );
    }
}

