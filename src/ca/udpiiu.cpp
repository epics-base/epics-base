/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
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
#include "searchTimer.h"
#include "repeaterSubscribeTimer.h"
#include "disconnectGovernorTimer.h"

// UDP protocol dispatch table
const udpiiu::pProtoStubUDP udpiiu::udpJumpTableCAC [] = 
{
    &udpiiu::versionAction,
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
udpiiu::udpiiu ( 
    epicsTimerQueueActive & timerQueue, 
    epicsMutex & cbMutexIn, 
    epicsMutex & cacMutexIn,
    cacContextNotify & ctxNotifyIn,
    cac & cac ) :
    recvThread ( *this, ctxNotifyIn, cbMutexIn, "CAC-UDP", 
        epicsThreadGetStackSize ( epicsThreadStackMedium ),
        cac::lowestPriorityLevelAbove (
            cac::lowestPriorityLevelAbove (
                cac.getInitializingThreadsPriority () ) ) ),
    rtteMean ( 5.0e-3 ), // seconds
    cacRef ( cac ),
    cbMutex ( cbMutexIn ),
    cacMutex ( cacMutexIn ),
    nBytesInXmitBuf ( 0 ),
    sequenceNumber ( 0 ),
    rtteSequenceNumber ( 0 ),
    lastReceivedSeqNo ( 0 ),
    sock ( 0 ),
    pGovTmr ( new disconnectGovernorTimer ( *this, timerQueue ) ),
    // The udpiiu and the search timer share the same lock because
    // this is much more efficent with recursive locks. Also, access
    // to the udp's netiiu base list is protected.
    pSearchTmr ( new searchTimer ( *this, timerQueue, this->mutex ) ),
    pRepeaterSubscribeTmr ( 
        new repeaterSubscribeTimer ( 
            *this, timerQueue, cbMutexIn, ctxNotifyIn ) ),
    repeaterPort ( 0 ),
    serverPort ( 0 ),
    localPort ( 0 ),
    shutdownCmd ( false ),
    rtteActive ( false ),
    lastReceivedSeqNoIsValid ( false )
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

    this->sock = epicsSocketCreate ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( this->sock == INVALID_SOCKET ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ("CAC: unable to create datagram socket because = \"%s\"\n",
            sockErrBuf );
        throwWithLocation ( noSocket () );
    }

    status = setsockopt ( this->sock, SOL_SOCKET, SO_BROADCAST, 
                (char *) &boolValue, sizeof ( boolValue ) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ("CAC: IP broadcasting enable failed because = \"%s\"\n",
            sockErrBuf );
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
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAC: unable to set socket option SO_RCVBUF because \"%s\"\n",
                sockErrBuf );
        }
    }
#endif

    // force a bind to an unconstrained address so we can obtain
    // the local port number below
    memset ( (char *)&addr, 0 , sizeof (addr) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = epicsHTON32 (INADDR_ANY); 
    addr.ia.sin_port = epicsHTON16 (PORT_ANY); // X aCC 818
    status = bind (this->sock, &addr.sa, sizeof (addr) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        epicsSocketDestroy (this->sock);
        errlogPrintf ( "CAC: unable to bind to an unconstrained address because = \"%s\"\n",
            sockErrBuf );
        throwWithLocation ( noSocket () );
    }
    
    {
        osiSockAddr tmpAddr;
        osiSocklen_t saddr_length = sizeof ( tmpAddr );
        status = getsockname ( this->sock, &tmpAddr.sa, &saddr_length );
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            epicsSocketDestroy ( this->sock );
            errlogPrintf ( "CAC: getsockname () error was \"%s\"\n", sockErrBuf );
            throwWithLocation ( noSocket () );
        }
        if ( tmpAddr.sa.sa_family != AF_INET) {
            epicsSocketDestroy ( this->sock );
            errlogPrintf ( "CAC: UDP socket was not inet addr family\n" );
            throwWithLocation ( noSocket () );
        }
        this->localPort = epicsNTOH16 ( tmpAddr.ia.sin_port );
    }

    /*
     * load user and auto configured
     * broadcast address list
     */
    ellInit ( & this->dest );    // X aCC 392
    configureChannelAccessAddressList ( & this->dest, this->sock, this->serverPort );
  
    caStartRepeaterIfNotInstalled ( this->repeaterPort );

    this->pushVersionMsg ();

    this->recvThread.start ();
}

/*
 *  udpiiu::~udpiiu ()
 */
udpiiu::~udpiiu ()
{
    this->shutdown ();

    // no need to own CAC lock here because the CA context 
    // is being decomissioned
    tsDLIter < nciu > chan = this->disconnGovernor.firstIter ();
    while ( chan.valid () ) {
        tsDLIter < nciu > next = chan;
        next++;
        chan->serviceShutdownNotify ();
        chan = next;
    }
    chan = this->serverAddrRes.firstIter ();
    while ( chan.valid () ) {
        tsDLIter < nciu > next = chan;
        next++;
        chan->serviceShutdownNotify ();
        chan = next;
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
    
    epicsSocketDestroy ( this->sock );
}

void udpiiu::shutdown ()
{
    // stop all of the timers
    this->pGovTmr->shutdown ();
    this->pSearchTmr->shutdown ();
    this->pRepeaterSubscribeTmr->shutdown ();

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

udpRecvThread::udpRecvThread ( 
    udpiiu & iiuIn, cacContextNotify & ctxNotifyIn, epicsMutex & cbMutexIn,
    const char * pName, unsigned stackSize, unsigned priority ) :
        iiu ( iiuIn ), cbMutex ( cbMutexIn ), ctxNotify ( ctxNotifyIn ), 
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

void udpRecvThread::show ( unsigned /* level */ ) const
{
}

void udpRecvThread::run ()
{
    epicsThreadPrivateSet ( caClientCallbackThreadId, &this->iiu );

    if ( ellCount ( & this->iiu.dest ) == 0 ) { // X aCC 392
        callbackManager mgr ( this->ctxNotify, this->cbMutex );
        epicsGuard < epicsMutex > guard ( this->iiu.cacMutex );
        genLocalExcep ( mgr.cbGuard, guard, 
            this->iiu.cacRef, ECA_NOSEARCHADDR, NULL );
    }

    do {
        osiSockAddr src;
        osiSocklen_t src_size = sizeof ( src );
        int status = recvfrom ( this->iiu.sock, 
            this->iiu.recvBuf, sizeof ( this->iiu.recvBuf ), 0,
            & src.sa, & src_size );

        callbackManager mgr ( this->ctxNotify, this->cbMutex );

        if ( status <= 0 ) {

            if ( status < 0 ) {
                int errnoCpy = SOCKERRNO;
                if ( 
                    errnoCpy != SOCK_EINTR &&
                    errnoCpy != SOCK_SHUTDOWN &&
                    errnoCpy != SOCK_ENOTSOCK &&
                    errnoCpy != SOCK_EBADF &&
                    // Avoid spurious ECONNREFUSED bug in linux
                    errnoCpy != SOCK_ECONNREFUSED &&
                    // Avoid ECONNRESET from disconnected socket bug
                    // in windows
                    errnoCpy != SOCK_ECONNRESET ) {

                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString ( 
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    errlogPrintf ( "CAC: UDP recv error was \"%s\"\n", 
                        sockErrBuf );
                }
            }
        }
        else if ( status > 0 ) {
            this->iiu.postMsg ( mgr.cbGuard, src, this->iiu.recvBuf, 
                (arrayElementCount) status, epicsTime::getCurrent() );
        }

    } while ( ! this->iiu.shutdownCmd );
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
    osiSockAddr saddr;
    caHdr msg;
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
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            fprintf ( stderr, "error sending registration message to CA repeater daemon was \"%s\"\n", 
                sockErrBuf );
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

    if ( repeaterPort > 0xffff ) {
        fprintf ( stderr, "caStartRepeaterIfNotInstalled () : strange repeater port specified\n" );
        return;
    }

    tmpSock = epicsSocketCreate ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
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
    epicsSocketEnableAddressReuseDuringTimeWaitState ( tmpSock );

    epicsSocketDestroy ( tmpSock );

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

bool udpiiu::badUDPRespAction ( 
    epicsGuard < epicsMutex > & guard, const caHdr &msg, 
    const osiSockAddr &netAddr, const epicsTime &currentTime )
{
    char buf[64];
    sockAddrToDottedIP ( &netAddr.sa, buf, sizeof ( buf ) );
    char date[64];
    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");
    this->printf ( guard, "CAC: Undecipherable ( bad msg code %u ) UDP message from %s at %s\n", 
                msg.m_cmmd, buf, date );
    return false;
}

bool udpiiu::versionAction ( epicsGuard < epicsMutex > &,
    const caHdr & hdr, const osiSockAddr &, const epicsTime & currentTime )
{
    epicsGuard < udpMutex > guard ( this->mutex );

    // update the round trip time estimate
    if ( hdr.m_dataType & sequenceNoIsValid ) {
        if ( this->rtteActive ) {
            if ( this->rtteSequenceNumber == hdr.m_cid ) {
                static const double gain = 0.25;
                double measured = currentTime - this->rtteTimeStamp;
                double error = measured - this->rtteMean;
                this->rtteMean += gain * error;
                this->rtteSequenceNumber = 0;
                this->rtteTimeStamp = epicsTime ();
                this->rtteActive = false;
            }
        }
        this->lastReceivedSeqNo = hdr.m_cid;
        this->lastReceivedSeqNoIsValid = true;
    }

    return true;
}

bool udpiiu::searchRespAction ( // X aCC 361
        epicsGuard < epicsMutex > & cbGuard, const caHdr &msg,
        const osiSockAddr &  addr, const epicsTime & currentTime )
{
    if ( addr.sa.sa_family != AF_INET ) {
        return false;
    }

    /*
     * Starting with CA V4.1 the minor version number
     * is appended to the end of each search reply.
     * This value is ignored by earlier clients.
     */
    unsigned minorVersion;
    if ( msg.m_postsize >= sizeof (minorVersion) ){
        /*
         * care is taken here not to break gcc 3.2 aggressive alias
         * analysis rules
         */
        ca_uint8_t * pPayLoad = ( ca_uint8_t *) ( &msg + 1 );
        unsigned byte0 = pPayLoad[0];
        unsigned byte1 = pPayLoad[1];
        minorVersion = ( byte0 << 8u ) | byte1;
    }
    else {
        minorVersion = CA_UKN_MINOR_VERSION;
    }

    /*
     * the type field is abused to carry the port number
     * so that we can have multiple servers on one host
     */
    osiSockAddr serverAddr;
    serverAddr.ia.sin_family = AF_INET;
    if ( CA_V48 ( minorVersion ) ) {
        if ( msg.m_cid != INADDR_BROADCAST ) {
            serverAddr.ia.sin_addr.s_addr = htonl ( msg.m_cid );
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

    bool success;
    if ( CA_V42 ( minorVersion ) ) {
        success = this->cacRef.transferChanToVirtCircuit 
            ( cbGuard, msg.m_available, msg.m_cid, 0xffff, 
                0, minorVersion, serverAddr );
    }
    else {
        success = this->cacRef.transferChanToVirtCircuit 
            ( cbGuard, msg.m_available, msg.m_cid, msg.m_dataType, 
                msg.m_count, minorVersion, serverAddr );
    }

    if ( success ) {
        // deadlock can result if this is called while holding the primary
        // mutex (because the primary mutex is used in the search timer callback)
        epicsGuard < udpMutex > guard ( this->mutex );
        this->pSearchTmr->notifySuccessfulSearchResponse ( 
            guard, this->lastReceivedSeqNo,
            this->lastReceivedSeqNoIsValid, currentTime );
    }

    return true;
}

bool udpiiu::beaconAction ( 
    epicsGuard < epicsMutex > &, const caHdr & msg, 
    const osiSockAddr & net_addr, const epicsTime & currentTime )
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
    ina.sin_addr.s_addr = epicsHTON32 ( msg.m_available );
    if ( msg.m_count != 0 ) {
        ina.sin_port = epicsHTON16 ( msg.m_count );
    }
    else {
        /*
         * old servers dont supply this and the
         * default port must be assumed
         */
        ina.sin_port = epicsHTON16 ( this->serverPort );
    }
    unsigned protocolRevision = msg.m_dataType;
    ca_uint32_t beaconNumber = msg.m_cid;

    this->cacRef.beaconNotify ( ina, currentTime, 
        beaconNumber, protocolRevision );

    return true;
}

bool udpiiu::repeaterAckAction ( epicsGuard < epicsMutex > &, const caHdr &,  
        const osiSockAddr &, const epicsTime &)
{
    this->pRepeaterSubscribeTmr->confirmNotify ();
    return true;
}

bool udpiiu::notHereRespAction ( epicsGuard < epicsMutex > &, const caHdr &,  
        const osiSockAddr &, const epicsTime & )
{
    return true;
}

bool udpiiu::exceptionRespAction ( 
    epicsGuard < epicsMutex > & cbGuard, const caHdr &msg, 
    const osiSockAddr & net_addr, const epicsTime & currentTime )
{
    const caHdr &reqMsg = * ( &msg + 1 );
    char name[64];
    sockAddrToDottedIP ( &net_addr.sa, name, sizeof ( name ) );
    char date[64];
    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");

    if ( msg.m_postsize > sizeof ( caHdr ) ){
        this->cacRef.printf ( cbGuard, 
            "error condition \"%s\" detected by %s with context \"%s\" at %s\n", 
            ca_message ( msg.m_available ), 
            name, reinterpret_cast <const char *> ( &reqMsg + 1 ), date );
    }
    else{
        this->cacRef.printf ( cbGuard,
            "error condition \"%s\" detected by %s at %s\n", 
            ca_message ( msg.m_available ), name, date );
    }

    return true;
}

void udpiiu::postMsg ( epicsGuard < epicsMutex > & guard, 
              const osiSockAddr & net_addr, 
              char * pInBuf, arrayElementCount blockSize,
              const epicsTime & currentTime )
{
    caHdr *pCurMsg;

    this->lastReceivedSeqNoIsValid = false;
    this->lastReceivedSeqNo = 0u;

    while ( blockSize ) {
        arrayElementCount size;

        if ( blockSize < sizeof ( *pCurMsg ) ) {
            char buf[64];
            sockAddrToDottedIP ( &net_addr.sa, buf, sizeof ( buf ) );
            this->printf ( guard,
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
        pCurMsg->m_available = epicsNTOH32 ( pCurMsg->m_available );
        pCurMsg->m_cid = epicsNTOH32 ( pCurMsg->m_cid );

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
            this->printf ( guard,
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
            this->printf ( guard, "CAC: Undecipherable UDP message from %s\n", buf );
            return;
        }

        blockSize -= size;
        pInBuf += size;;
    }
}

bool udpiiu::pushVersionMsg ()
{
    epicsGuard < udpMutex > guard ( this->mutex );

    this->sequenceNumber++;

    caHdr msg;
    msg.m_cmmd = epicsHTON16 ( CA_PROTO_VERSION );
    msg.m_available = epicsHTON32 ( 0 );
    msg.m_dataType = epicsHTON16 ( sequenceNoIsValid ); 
    msg.m_count = epicsHTON16 ( CA_MINOR_PROTOCOL_REVISION );
    msg.m_cid = epicsHTON32 ( this->sequenceNumber ); // sequence number

    return pushDatagramMsg ( msg, 0, 0 );
}

bool udpiiu::pushDatagramMsg ( const caHdr & msg, 
    const void * pExt, ca_uint16_t extsize )
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

void udpiiu::datagramFlush ( 
    epicsGuard < udpMutex > &, const epicsTime & currentTime )
{
    // dont send the version header by itself
    if ( this->nBytesInXmitBuf <= sizeof ( caHdr ) ) {
        return;
    }

    if ( this->rtteActive ) {
        double delay = currentTime - this->rtteTimeStamp;
        if ( delay > 8 * this->rtteMean ) {
            this->rtteSequenceNumber = this->sequenceNumber;
            this->rtteTimeStamp = currentTime;
        }
    }
    else {
        this->rtteActive = true;
        this->rtteSequenceNumber = this->sequenceNumber;
        this->rtteTimeStamp = currentTime;
    }

    osiSockAddrNode *pNode = ( osiSockAddrNode * ) // X aCC 749
            ellFirst ( & this->dest ); 
    while ( pNode ) {
        int status;

        assert ( this->nBytesInXmitBuf <= INT_MAX );
        status = sendto ( this->sock, this->xmitBuf,   
                (int) this->nBytesInXmitBuf, 0, 
                &pNode->addr.sa, sizeof ( pNode->addr.sa ) );
        if ( status != (int) this->nBytesInXmitBuf ) {
            if ( status >= 0 ) {
                errlogPrintf ( "CAC: UDP sendto () call returned strange xmit count?\n" );
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
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString ( 
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    errlogPrintf (
                        "CAC: error = \"%s\" sending UDP msg to %s\n",
                        sockErrBuf, buf);
                    break;
                }
            }
        }
        pNode = (osiSockAddrNode *) ellNext ( &pNode->node ); // X aCC 749
    }

    this->nBytesInXmitBuf = 0u;

    this->pushVersionMsg ();
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
        ::printf ( "repeater subscribee timer:\n" );
        this->pRepeaterSubscribeTmr->show ( level - 2u );
        ::printf ( "disconnect governor subscribee timer:\n" );
        this->pGovTmr->show ( level - 2u );
        tsDLIterConst < nciu > pChan = this->disconnGovernor.firstIter ();
	    while ( pChan.valid () ) {
            pChan->show ( level - 2u );
            pChan++;
        }
        ::printf ( "search message timer:\n" );
        this->pSearchTmr->show ( level - 2u );
        pChan = this->serverAddrRes.firstIter ();
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
    int status = sendto ( this->sock, reinterpret_cast < char * > ( &msg ),  
            sizeof (msg), 0, &addr.sa, sizeof ( addr.sa ) );
    if ( status == sizeof (msg) ) {
        return true;
    }
    return false;
}

void udpiiu::beaconAnomalyNotify ( 
    epicsGuard < epicsMutex > & cacGuard, const epicsTime & currentTime ) 
{
    epicsGuard <udpMutex> guard ( this->mutex );

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

    this->pSearchTmr->beaconAnomalyNotify ( guard, currentTime, delay );
}

bool udpiiu::searchMsg ( epicsGuard < udpMutex > & /* guard */ )
{
    bool success;

    if ( nciu *pChan = this->serverAddrRes.get () ) {
        success = pChan->searchMsg ( *this );
        if ( success ) {
            this->serverAddrRes.add ( *pChan );
        }
        else {
            this->serverAddrRes.push ( *pChan );
        }
    }
    else {
        success = false;
    }

    return success;
}

void udpiiu::installNewChannel ( const epicsTime & currentTime, nciu & chan )
{
    bool firstChannel = false;
    epicsGuard < udpMutex > guard ( this->mutex );
    if ( this->serverAddrRes.count() == 0 ) {
        firstChannel = true;
    }
    // push them to the front of the list so that 
    // a search request is sent immediately, and 
    // so that the new channel's retry count is 
    // seen when calculating the minimum retry 
    // which is used to compute the search interval
    this->serverAddrRes.push ( chan );
    chan.channelNode::listMember = 
        channelNode::cs_serverAddrResPend;

    this->pSearchTmr->channelCreatedNotify ( 
        guard, currentTime, firstChannel );
}

void udpiiu::installDisconnectedChannel ( nciu & chan )
{
    epicsGuard < udpMutex > guard ( this->mutex );
    this->disconnGovernor.add ( chan );
    chan.channelNode::listMember = 
        channelNode::cs_disconnGov;
}

void udpiiu::govExpireNotify ( const epicsTime & currentTime )
{
    epicsGuard < udpMutex > guard ( this->mutex );
    if ( this->disconnGovernor.count () ) {
        bool firstChannel = this->serverAddrRes.count() == 0;
        // push them to the front of the list so that 
        // a search request is sent immediately, and 
        // so that the new channel's retry count is 
        // seen when calculating the minimum retry 
        // which is used to compute the search interval
        while ( nciu * pChan = this->disconnGovernor.get () ) {
            this->serverAddrRes.push ( *pChan );
            pChan->channelNode::listMember = 
                channelNode::cs_serverAddrResPend;
        }
        this->pSearchTmr->channelDisconnectedNotify ( 
            guard, currentTime, firstChannel );
    }
}

int udpiiu::printf ( epicsGuard < epicsMutex > & cbGuard, 
                    const char * pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->cacRef.vPrintf ( cbGuard, pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void udpiiu::uninstallChan (
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & cacGuard, 
    nciu & chan )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    cacGuard.assertIdenticalMutex ( this->cacMutex );

    epicsGuard < udpMutex > guard ( this->mutex );
    if ( chan.channelNode::listMember == channelNode::cs_disconnGov ) {
        this->disconnGovernor.remove ( chan );
    }
    else if ( chan.channelNode::listMember == channelNode::cs_serverAddrResPend ) {
        this->serverAddrRes.remove ( chan );
    }
    else {
        this->cacRef.printf ( cbGuard,
            "cac: attempt to uninstall channel from udp iiu, but it inst installed there?" );
    }
    chan.channelNode::listMember = channelNode::cs_none;
}

void udpiiu::hostName ( 
    epicsGuard < epicsMutex > & cacGuard,
    char *pBuf, unsigned bufLength ) const
{
    netiiu::hostName ( cacGuard, pBuf, bufLength );
}

const char * udpiiu::pHostName (
    epicsGuard < epicsMutex > & cacGuard ) const
{
    return netiiu::pHostName ( cacGuard );
}

bool udpiiu::ca_v42_ok (
    epicsGuard < epicsMutex > & cacGuard ) const
{
    return netiiu::ca_v42_ok ( cacGuard );
}

bool udpiiu::ca_v41_ok (
    epicsGuard < epicsMutex > & cacGuard ) const
{
    return netiiu::ca_v41_ok ( cacGuard );
}

void udpiiu::writeRequest ( 
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, unsigned type, 
    arrayElementCount nElem, const void * pValue )
{
    guard.assertIdenticalMutex ( this->cacMutex );
    netiiu::writeRequest ( guard, chan, type, nElem, pValue );
}

void udpiiu::writeNotifyRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netWriteNotifyIO & io, unsigned type, 
    arrayElementCount nElem, const void *pValue )
{
    guard.assertIdenticalMutex ( this->cacMutex );
    netiiu::writeNotifyRequest ( guard, chan, io, type, nElem, pValue );
}

void udpiiu::readNotifyRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netReadNotifyIO & io, unsigned type, arrayElementCount nElem )
{
    guard.assertIdenticalMutex ( this->cacMutex );
    netiiu::readNotifyRequest ( guard, chan, io, type, nElem );
}

void udpiiu::clearChannelRequest ( 
    epicsGuard < epicsMutex > & guard, 
    ca_uint32_t sid, ca_uint32_t cid )
{
    guard.assertIdenticalMutex ( this->cacMutex );
    netiiu::clearChannelRequest ( guard, sid, cid );
}

void udpiiu::subscriptionRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netSubscription & subscr )
{
    guard.assertIdenticalMutex ( this->cacMutex );
    netiiu::subscriptionRequest ( guard, chan, subscr );
}

void udpiiu::subscriptionUpdateRequest ( 
    epicsGuard < epicsMutex > &, nciu &, 
    netSubscription & )
{
}

void udpiiu::subscriptionCancelRequest ( 
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, netSubscription & subscr )
{
    guard.assertIdenticalMutex ( this->cacMutex );
    netiiu::subscriptionCancelRequest ( guard, chan, subscr );
}

void udpiiu::flushRequest ( 
    epicsGuard < epicsMutex > & guard )
{
    netiiu::flushRequest ( guard );
}

void udpiiu::eliminateExcessiveSendBacklog ( 
    epicsGuard < epicsMutex > *,
    epicsGuard < epicsMutex > & )
{
}

void udpiiu::requestRecvProcessPostponedFlush (
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->cacMutex );
    netiiu::requestRecvProcessPostponedFlush ( guard );
}

osiSockAddr udpiiu::getNetworkAddress (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->cacMutex );
    return netiiu::getNetworkAddress ( guard );
}

double udpiiu::receiveWatchdogDelay (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->cacMutex );
    return - DBL_MAX;
}




