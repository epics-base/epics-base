/*
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#ifdef _MSC_VER
#   pragma warning(disable:4355)
#endif

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "envDefs.h"
#include "osiProcess.h"
#include "osiWireFormat.h"

#define epicsExportSharedSymbols
#include "addrList.h"
#include "caerr.h" // for ECA_NOSEARCHADDR
#include "udpiiu.h"
#include "iocinf.h"
#include "inetAddrID.h"
#include "cac.h"
#include "repeaterSubscribeTimer.h"
#include "searchTimer.h"

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
udpiiu::udpiiu ( epicsTimerQueueActive & timerQueue, callbackMutex & cbMutex, cac & cac ) :
    recvThread ( *this, cbMutex,
        "CAC-UDP", 
        epicsThreadGetStackSize ( epicsThreadStackMedium ),
        cac::lowestPriorityLevelAbove 
            ( cac.getInitializingThreadsPriority () ) ),
    cacRef ( cac ),
    nBytesInXmitBuf ( 0 ),
    sock ( 0 ),
    // The udpiiu and the search timer share the same lock because
    // this is much more efficent with recursive locks. Also, access
    // to the udp's netiiu base list is protected.
    pSearchTmr ( new searchTimer ( *this, timerQueue ) ),
    pRepeaterSubscribeTmr ( new repeaterSubscribeTimer ( *this, timerQueue ) ),
    repeaterPort ( 0 ),
    serverPort ( 0 ),
    localPort ( 0 ),
    shutdownCmd ( false )
{
    static const unsigned short PORT_ANY = 0u;
    osiSockAddr addr;
    int boolValue = true;
    int status;

    this->repeaterPort = 
        envGetInetPortConfigParam ( &EPICS_CA_REPEATER_PORT,
                                    static_cast <unsigned short> (CA_REPEATER_PORT) );

    this->serverPort = 
        envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT,
                                    static_cast <unsigned short> (CA_SERVER_PORT) );

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
    addr.ia.sin_addr.s_addr = epicsHTON32 (INADDR_ANY); 
    addr.ia.sin_port = epicsHTON16 (PORT_ANY); // X aCC 818
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
            this->printf ( "CAC: getsockname () error was \"%s\"\n", SOCKERRSTR (SOCKERRNO) );
            throwWithLocation ( noSocket () );
        }
        if ( tmpAddr.sa.sa_family != AF_INET) {
            socket_close ( this->sock );
            this->printf ( "CAC: UDP socket was not inet addr family\n" );
            throwWithLocation ( noSocket () );
        }
        this->localPort = epicsNTOH16 ( tmpAddr.ia.sin_port );
    }

    /*
     * load user and auto configured
     * broadcast address list
     */
    ellInit ( & this->dest );    // X aCC 392
    configureChannelAccessAddressList ( &this->dest, this->sock, this->serverPort );
  
    caStartRepeaterIfNotInstalled ( this->repeaterPort );

    this->recvThread.start ();
}

/*
 *  udpiiu::~udpiiu ()
 */
udpiiu::~udpiiu ()
{
    this->shutdown ();

    static limboiiu limboIIU;
    while ( nciu * pChan = this->channelList.get () ) {
        // no need to own CAC lock here because the channel is being decomissioned
        pChan->disconnect ( limboIIU );
    }

    // avoid use of ellFree because problems on windows occur if the
    // free is in a different DLL than the malloc
    ELLNODE * nnode = this->dest.node.next;
    while ( nnode )
    {
        ELLNODE * pnode = nnode;
        nnode = nnode->next;
        free ( pnode );
    }
    
    socket_close ( this->sock );
}

void udpiiu::shutdown ()
{
    if ( ! this->recvThread.exitWait ( 0.0 ) ) {
        unsigned tries = 0u;

        this->shutdownCmd = true;

        this->wakeupMsg ();

        // wait for recv threads to exit
        double shutdownDelay = 1.0;
        while ( ! this->recvThread.exitWait ( shutdownDelay ) ) {
            this->wakeupMsg ();
            if ( shutdownDelay < 16.0 ) {
                shutdownDelay += shutdownDelay;
            }
            if ( ++tries > 3 ) {
                fprintf ( stderr, "cac: timing out waiting for UDP thread shutdown\n" );
            }
        }
    }
}

//
//  udpiiu::recvMsg ()
//
void udpiiu::recvMsg ( callbackMutex & cbMutex )
{
    osiSockAddr src;
    int status;

    if ( this->cacRef.preemptiveCallbakIsEnabled() ) {
        osiSocklen_t src_size = sizeof ( src );
        status = recvfrom ( this->sock, this->recvBuf, sizeof ( this->recvBuf ), 0,
                            &src.sa, &src_size );
    }
    else {
        // peek first at the message so that file descriptor managers will wake up
        // in single threaded applications
        osiSocklen_t src_size = sizeof ( src );
        char peek;
        recvfrom ( this->sock, & peek, sizeof ( peek ), MSG_PEEK,
                            &src.sa, &src_size );
        status = 0;
    }

    {
        epicsGuard < callbackMutex > guard ( cbMutex );

        if ( ! this->cacRef.preemptiveCallbakIsEnabled() ) {
            osiSocklen_t src_size = sizeof ( src );
            status = recvfrom ( this->sock, this->recvBuf, sizeof ( this->recvBuf ), 0,
                            &src.sa, &src_size );
        }
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
            // Avoid spurious ECONNREFUSED bug in linux
            if ( errnoCpy == SOCK_ECONNREFUSED ) {
                return;
            }
            // Avoid ECONNRESET from disconnected socket bug
            // in windows
            if ( errnoCpy == SOCK_ECONNRESET ) {
                return;
            }
            this->printf ( "Unexpected UDP recv error was \"%s\"\n", 
                SOCKERRSTR (errnoCpy) );
        }
        else if ( status > 0 ) {
            this->postMsg ( guard, src, this->recvBuf, 
                (arrayElementCount) status, epicsTime::getCurrent() );
        }
    }
    return;
}

udpRecvThread::udpRecvThread ( udpiiu & iiuIn, callbackMutex & cbMutexIn,
    const char * pName, unsigned stackSize, unsigned priority ) :
        iiu ( iiuIn ), cbMutex ( cbMutexIn ), 
        thread ( *this, pName, stackSize, priority ) {}

udpRecvThread::~udpRecvThread () 
{
}

void udpRecvThread::start ()
{
    this->thread.start ();
}

bool udpRecvThread::exitWait ( double delay )
{
    return this->thread.exitWait ( delay );
}

void udpRecvThread::run ()
{
    epicsThreadPrivateSet ( caClientCallbackThreadId, &this->iiu );

    {
        epicsGuard < callbackMutex > cbGuard ( this->cbMutex );
        this->iiu.cacRef.notifyNewFD ( cbGuard, this->iiu.sock );
        if ( ellCount ( & this->iiu.dest ) == 0 ) { // X aCC 392
            genLocalExcep ( cbGuard, this->iiu.cacRef, ECA_NOSEARCHADDR, NULL );
        }

    }

    do {
        this->iiu.recvMsg ( this->cbMutex );
    } while ( ! this->iiu.shutdownCmd );

    {
        epicsGuard < callbackMutex > cbGuard ( this->cbMutex );
        this->iiu.cacRef.notifyDestroyFD ( cbGuard, this->iiu.sock );
    }
}

/*
 *  udpiiu::repeaterRegistrationMessage ()
 *
 *  register with the repeater 
 */
void udpiiu::repeaterRegistrationMessage ( unsigned attemptNumber )
{
    epicsGuard < udpMutex > cbGuard ( this->mutex );
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

	assert ( repeaterPort <= USHRT_MAX );
	unsigned short port = static_cast <unsigned short> ( repeaterPort );

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
            saddr.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_LOOPBACK );
            saddr.ia.sin_port = epicsHTON16 ( port );
        }
        else {
            saddr.ia.sin_port = epicsHTON16 ( port );
        }
    }
    else {
        saddr.ia.sin_family = AF_INET;
        saddr.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_LOOPBACK );
        saddr.ia.sin_port = epicsHTON16 ( port );
    }

    memset ( (char *) &msg, 0, sizeof (msg) );
    msg.m_cmmd = epicsHTON16 ( REPEATER_REGISTER ); // X aCC 818
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

    status = sendto ( sock, (char *) &msg, len, 0,
                      &saddr.sa, sizeof ( saddr ) );
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
    union {
        struct sockaddr_in ia; 
        struct sockaddr sa;
    } bd;
    int flag;

    if ( repeaterPort > 0xffff ) {
        fprintf ( stderr, "caStartRepeaterIfNotInstalled () : strange repeater port specified\n" );
        return;
    }

    tmpSock = socket ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( tmpSock != INVALID_SOCKET ) {
        ca_uint16_t port = static_cast < ca_uint16_t > ( repeaterPort );
        memset ( (char *) &bd, 0, sizeof ( bd ) );
        bd.ia.sin_family = AF_INET;
        bd.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_ANY ); 
        bd.ia.sin_port = epicsHTON16 ( port );   
        status = bind ( tmpSock, &bd.sa, sizeof ( bd ) );
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

bool udpiiu::badUDPRespAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
    const osiSockAddr &netAddr, const epicsTime &currentTime )
{
    char buf[64];
    sockAddrToDottedIP ( &netAddr.sa, buf, sizeof ( buf ) );
    char date[64];
    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");
    this->printf ( "CAC: Undecipherable ( bad msg code %u ) UDP message from %s at %s\n", 
                msg.m_cmmd, buf, date );
    return false;
}

bool udpiiu::noopAction ( epicsGuard < callbackMutex > &,
                         const caHdr &, const osiSockAddr &, const epicsTime & )
{
    return true;
}

bool udpiiu::searchRespAction ( // X aCC 361
        epicsGuard < callbackMutex > & cbLocker, const caHdr &msg,
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
        minorVersion = epicsNTOH16 ( *pMinorVersion );      
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
        serverAddr.ia.sin_port = epicsHTON16 ( msg.m_dataType );
    }
    else if ( CA_V45 (minorVersion) ) {
        serverAddr.ia.sin_port = epicsHTON16 ( msg.m_dataType );
        serverAddr.ia.sin_addr = addr.ia.sin_addr;
    }
    else {
        serverAddr.ia.sin_port = epicsHTON16 ( this->serverPort );
        serverAddr.ia.sin_addr = addr.ia.sin_addr;
    }

    if ( CA_V42 ( minorVersion ) ) {
        return this->cacRef.lookupChannelAndTransferToTCP 
            ( cbLocker, msg.m_available, msg.m_cid, 0xffff, 
                0, minorVersion, serverAddr, currentTime );
    }
    else {
        return this->cacRef.lookupChannelAndTransferToTCP 
            ( cbLocker, msg.m_available, msg.m_cid, msg.m_dataType, 
                msg.m_count, minorVersion, serverAddr, currentTime );
    }
}

bool udpiiu::beaconAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
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
     *   2) set this field to epicsHTON32(INADDR_ANY)
     * new servers:
     *   always set this field to epicsHTON32(INADDR_ANY)
     *
     * clients always assume that if this
     * field is set to something that isnt epicsHTON32(INADDR_ANY)
     * then it is the overriding IP address of the server.
     */
    ina.sin_family = AF_INET;
    ina.sin_addr.s_addr = msg.m_available;
    if ( msg.m_count != 0 ) {
        ina.sin_port = epicsNTOH16 ( msg.m_count );
    }
    else {
        /*
         * old servers dont supply this and the
         * default port must be assumed
         */
        ina.sin_port = epicsNTOH16 ( this->serverPort );
    }
    unsigned protocolRevision = epicsNTOH16 ( msg.m_dataType );
    unsigned beaconNumber = epicsNTOH32 ( msg.m_cid );

    this->cacRef.beaconNotify ( ina, currentTime, 
        beaconNumber, protocolRevision );

    return true;
}

bool udpiiu::repeaterAckAction ( epicsGuard < callbackMutex > &, const caHdr &,  
        const osiSockAddr &, const epicsTime &)
{
    this->cacRef.repeaterSubscribeConfirmNotify ();
    return true;
}

bool udpiiu::notHereRespAction ( epicsGuard < callbackMutex > &, const caHdr &,  
        const osiSockAddr &, const epicsTime & )
{
    return true;
}

bool udpiiu::exceptionRespAction ( epicsGuard < callbackMutex > &, const caHdr &msg, 
        const osiSockAddr &net_addr, const epicsTime &currentTime )
{
    const caHdr &reqMsg = * ( &msg + 1 );
    char name[64];
    sockAddrToDottedIP ( &net_addr.sa, name, sizeof ( name ) );
    char date[64];
    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");

    if ( msg.m_postsize > sizeof ( caHdr ) ){
        errlogPrintf ( "error condition \"%s\" detected by %s with context \"%s\" at %s\n", 
            ca_message ( epicsHTON32 ( msg.m_available ) ), 
            name, reinterpret_cast <const char *> ( &reqMsg + 1 ), date );
    }
    else{
        errlogPrintf ( "error condition \"%s\" detected by %s at %s\n", 
            ca_message ( epicsHTON32 ( msg.m_available ) ), name, date );
    }

    return true;
}

void udpiiu::postMsg ( epicsGuard < callbackMutex > & guard, 
              const osiSockAddr & net_addr, 
              char * pInBuf, arrayElementCount blockSize,
              const epicsTime & currentTime )
{
    caHdr *pCurMsg;

    while ( blockSize ) {
        arrayElementCount size;

        if ( blockSize < sizeof ( *pCurMsg ) ) {
            char buf[64];
            sockAddrToDottedIP ( &net_addr.sa, buf, sizeof ( buf ) );
            this->printf (
                "%s: Undecipherable (too small) UDP msg from %s ignored\n", 
                    __FILE__,  buf );
            return;
        }

        pCurMsg = reinterpret_cast < caHdr * > ( pInBuf );

        /* 
         * fix endian of bytes 
         */
        pCurMsg->m_postsize = epicsNTOH16 ( pCurMsg->m_postsize );
        pCurMsg->m_cmmd = epicsNTOH16 ( pCurMsg->m_cmmd );
        pCurMsg->m_dataType = epicsNTOH16 ( pCurMsg->m_dataType );
        pCurMsg->m_count = epicsNTOH16 ( pCurMsg->m_count );

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
                "%s: Undecipherable (payload too small) UDP msg from %s ignored\n", __FILE__, 
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
        bool success = ( this->*pStub ) ( guard, *pCurMsg, net_addr, currentTime );
        if ( ! success ) {
            char buf[256];
            sockAddrToDottedIP ( &net_addr.sa, buf, sizeof ( buf ) );
            this->printf ( "CAC: Undecipherable UDP message from %s\n", buf );
            return;
        }

        blockSize -= size;
        pInBuf += size;;
    }
}

bool udpiiu::pushDatagramMsg ( const caHdr & msg, const void *pExt, ca_uint16_t extsize )
{
    epicsGuard < udpMutex > guard ( this->mutex );

    ca_uint16_t alignedExtSize = static_cast <ca_uint16_t> (CA_MESSAGE_ALIGN ( extsize ));
    arrayElementCount msgsize = sizeof ( caHdr ) + alignedExtSize;

    /* fail out if max message size exceeded */
    if ( msgsize >= sizeof ( this->xmitBuf ) - 7 ) {
        return false;
    }

    if ( msgsize + this->nBytesInXmitBuf > sizeof ( this->xmitBuf ) ) {
        return false;
    }

    caHdr * pbufmsg = ( caHdr * ) &this->xmitBuf[this->nBytesInXmitBuf];
    *pbufmsg = msg;
    memcpy ( pbufmsg + 1, pExt, extsize );
    if ( extsize != alignedExtSize ) {
        char *pDest = (char *) ( pbufmsg + 1 );
        memset ( pDest + extsize, '\0', alignedExtSize - extsize );
    }
    pbufmsg->m_postsize = epicsHTON16 ( alignedExtSize );
    this->nBytesInXmitBuf += msgsize;

    return true;
}

void udpiiu::datagramFlush ()
{
    epicsGuard < udpMutex > guard ( this->mutex );

    {
        osiSockAddrNode  *pNode;

        if ( this->nBytesInXmitBuf == 0u ) {
            return;
        }

        pNode = (osiSockAddrNode *) ellFirst ( &this->dest ); // X aCC 749
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
            pNode = (osiSockAddrNode *) ellNext ( &pNode->node ); // X aCC 749
        }

        this->nBytesInXmitBuf = 0u;
    }
}

void udpiiu::show ( unsigned level ) const
{
    epicsGuard < udpMutex > guard ( this->mutex );

    ::printf ( "Datagram IO circuit (and disconnected channel repository)\n");
    if ( level > 1u ) {
        ::printf ("\trepeater port %u\n", this->repeaterPort );
        ::printf ("\tdefault server port %u\n", this->serverPort );
        printChannelAccessAddressList ( & this->dest );
    }
    if ( level > 2u ) {
        ::printf ("\tsocket identifier %d\n", this->sock );
        ::printf ("\tbytes in xmit buffer %u\n", this->nBytesInXmitBuf );
        ::printf ("\tshut down command bool %u\n", this->shutdownCmd );
        ::printf ( "\trecv thread exit signal:\n" );
        this->recvThread.show ( level - 2u );
        ::printf ( "search message timer:\n" );
        this->pSearchTmr->show ( level - 2u );
        ::printf ( "repeater subscribee timer:\n" );
        this->pRepeaterSubscribeTmr->show ( level - 2u );
        tsDLIterConstBD < nciu > pChan = this->channelList.firstIter ();
	    while ( pChan.valid () ) {
            pChan->show ( level - 2u );
            pChan++;
        }
    }
}

bool udpiiu::wakeupMsg ()
{
    caHdr msg;
    msg.m_cmmd = epicsHTON16 ( CA_PROTO_VERSION );
    msg.m_available = epicsHTON32 ( 0u );
    msg.m_dataType = epicsHTON16 ( 0u );
    msg.m_count = epicsHTON16 ( 0u );
    msg.m_cid = epicsHTON32 ( 0u );
    msg.m_postsize = epicsHTON16 ( 0u );

    osiSockAddr addr;
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_LOOPBACK );
    addr.ia.sin_port = epicsHTON16 ( this->localPort );

    epicsGuard < udpMutex > guard ( this->mutex );

    // send a wakeup msg so the UDP recv thread will exit
    int status = sendto ( this->sock, reinterpret_cast < const char * > ( &msg ),  
            sizeof (msg), 0, &addr.sa, sizeof ( addr.sa ) );
    if ( status == sizeof (msg) ) {
        return true;
    }
    return false;
}

void udpiiu::repeaterConfirmNotify ()
{
    this->pRepeaterSubscribeTmr->confirmNotify ();
}

void udpiiu::notifySearchResponse ( unsigned short retrySeqNo, const epicsTime & currentTime )
{
    // deadlock can result if this is called while holding the primary
    // mutex (because the primary mutex is used in the search timer callback)
    this->pSearchTmr->notifySearchResponse ( retrySeqNo, currentTime );
}

void udpiiu::resetSearchTimerPeriod ( double delay ) 
{
    this->pSearchTmr->resetPeriod ( delay );
}

void udpiiu::beaconAnomalyNotify () 
{
    static const double portTicksPerSec = 1000u;
    static const unsigned portBasedDelayMask = 0xff;

    /*
     * This is needed when many machines
     * have channels in a disconnected state that 
     * dont exist anywhere on the network. This insures
     * that we dont have many CA clients synchronously
     * flooding the network with broadcasts (and swamping
     * out requests for valid channels).
     *
     * I fetch the local UDP port number and use the low 
     * order bits as a pseudo random delay to prevent every 
     * one from replying at once.
     */
    double delay = ( this->localPort & portBasedDelayMask );
    delay /= portTicksPerSec; 

    this->pSearchTmr->resetPeriod ( delay );

    {
        epicsGuard <udpMutex> guard ( this->mutex );
        tsDLIterBD < nciu > chan = this->channelList.firstIter ();
        while ( chan.valid () ) {
            chan->resetRetryCount ();
            chan++;
        }
    }
}

bool udpiiu::searchMsg ( unsigned short retrySeqNumber, unsigned & retryNoForThisChannel )
{
    bool success;

    epicsGuard < udpMutex> guard ( this->mutex );

    if ( nciu *pChan = this->channelList.get () ) {
        success = pChan->searchMsg ( *this, retrySeqNumber, retryNoForThisChannel );
        if ( success ) {
            this->channelList.add ( *pChan );
        }
        else {
            this->channelList.push ( *pChan );
        }
    }
    else {
        success = false;
    }

    return success;
}

void udpiiu::installChannel ( nciu & chan )
{
    {
        epicsGuard < udpMutex> guard ( this->mutex );
        this->channelList.add ( chan );
    }
    this->resetSearchPeriod ( 0.0 );
}

int udpiiu::printf ( const char *pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->cacRef.vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void udpiiu::uninstallChan ( 
    epicsGuard < cacMutex > &, nciu & chan )
{
    epicsGuard < udpMutex > guard ( this->mutex );
    this->channelList.remove ( chan );
}

void udpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{
    netiiu::hostName ( pBuf, bufLength );
}

const char * udpiiu::pHostName () const
{
    return netiiu::pHostName ();
}

bool udpiiu::ca_v42_ok () const
{
    return netiiu::ca_v42_ok ();
}

void udpiiu::writeRequest ( epicsGuard < cacMutex > & guard, nciu & chan, unsigned type, 
                unsigned nElem, const void * pValue )
{
    netiiu::writeRequest ( guard, chan, type, nElem, pValue );
}

void udpiiu::writeNotifyRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netWriteNotifyIO & io, unsigned type, unsigned nElem, const void *pValue )
{
    netiiu::writeNotifyRequest ( guard, chan, io, type, nElem, pValue );
}

void udpiiu::readNotifyRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netReadNotifyIO & io, unsigned type, unsigned nElem )
{
    netiiu::readNotifyRequest ( guard, chan, io, type, nElem );
}

void udpiiu::clearChannelRequest ( epicsGuard < cacMutex > & guard, 
                ca_uint32_t sid, ca_uint32_t cid )
{
    netiiu::clearChannelRequest ( guard, sid, cid );
}

void udpiiu::subscriptionRequest ( epicsGuard < cacMutex > & guard, nciu & chan, 
                netSubscription & subscr )
{
    netiiu::subscriptionRequest ( guard, chan, subscr );
}

void udpiiu::subscriptionCancelRequest ( epicsGuard < cacMutex > & guard, 
                nciu & chan, netSubscription & subscr )
{
    netiiu::subscriptionCancelRequest ( guard, chan, subscr );
}

void udpiiu::flushRequest ()
{
    netiiu::flushRequest ();
}

bool udpiiu::flushBlockThreshold ( epicsGuard < cacMutex > & guard ) const
{
    return netiiu::flushBlockThreshold ( guard );
}

void udpiiu::flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & guard )
{
    netiiu::flushRequestIfAboveEarlyThreshold ( guard );
}

void udpiiu::blockUntilSendBacklogIsReasonable 
    ( cacNotify & notify, epicsGuard < cacMutex > & guard )
{
    netiiu::blockUntilSendBacklogIsReasonable ( notify, guard );
}

void udpiiu::requestRecvProcessPostponedFlush ()
{
    netiiu::requestRecvProcessPostponedFlush ();
}

osiSockAddr udpiiu::getNetworkAddress () const
{
    return netiiu::getNetworkAddress ();
}

void udpiiu::resetSearchPeriod ( double delay )
{
    this->pSearchTmr->resetPeriod ( delay );
}


