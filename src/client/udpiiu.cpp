/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
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
#include "dbDefs.h"
#include "osiProcess.h"
#include "osiWireFormat.h"
#include "epicsAlgorithm.h"
#include "errlog.h"
#include "locationException.h"

#define epicsExportSharedSymbols
#include "addrList.h"
#include "caerr.h" // for ECA_NOSEARCHADDR
#include "udpiiu.h"
#include "iocinf.h"
#include "inetAddrID.h"
#include "cac.h"
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


static
double getMaxPeriod()
{
    double maxPeriod = maxSearchPeriodDefault;

    if ( envGetConfigParamPtr ( & EPICS_CA_MAX_SEARCH_PERIOD ) ) {
        long longStatus = envGetDoubleConfigParam (
            & EPICS_CA_MAX_SEARCH_PERIOD, & maxPeriod );
        if ( ! longStatus ) {
            if ( maxPeriod < maxSearchPeriodLowerLimit ) {
                epicsPrintf ( "\"%s\" out of range (low)\n",
                                EPICS_CA_MAX_SEARCH_PERIOD.name );
                maxPeriod = maxSearchPeriodLowerLimit;
                epicsPrintf ( "Setting \"%s\" = %f seconds\n",
                    EPICS_CA_MAX_SEARCH_PERIOD.name, maxPeriod );
            }
        }
        else {
            epicsPrintf ( "EPICS \"%s\" wasnt a real number\n",
                            EPICS_CA_MAX_SEARCH_PERIOD.name );
            epicsPrintf ( "Setting \"%s\" = %f seconds\n",
                EPICS_CA_MAX_SEARCH_PERIOD.name, maxPeriod );
        }
    }

    return maxPeriod;
}

static
unsigned getNTimers(double maxPeriod)
{
    unsigned nTimers = static_cast < unsigned > ( 1.0 + log ( maxPeriod / minRoundTripEstimate ) / log ( 2.0 ) );

    if ( nTimers > channelNode::getMaxSearchTimerCount () ) {
        nTimers = channelNode::getMaxSearchTimerCount ();
        epicsPrintf ( "\"%s\" out of range (high)\n",
                        EPICS_CA_MAX_SEARCH_PERIOD.name );
        epicsPrintf ( "Setting \"%s\" = %f seconds\n",
            EPICS_CA_MAX_SEARCH_PERIOD.name,
            (1<<(nTimers-1)) * minRoundTripEstimate );
    }

    return nTimers;
}

//
// udpiiu::udpiiu ()
//
udpiiu::udpiiu ( 
    epicsGuard < epicsMutex > & cacGuard,
    epicsTimerQueueActive & timerQueue, 
    epicsMutex & cbMutexIn, 
    epicsMutex & cacMutexIn,
    cacContextNotify & ctxNotifyIn,
    cac & cac,
    unsigned port,
    tsDLList < SearchDest > & searchDestListIn ) :
    recvThread ( *this, ctxNotifyIn, cbMutexIn, "CAC-UDP", 
        epicsThreadGetStackSize ( epicsThreadStackMedium ),
        cac::lowestPriorityLevelAbove (
            cac::lowestPriorityLevelAbove (
                cac.getInitializingThreadsPriority () ) ) ),
    m_repeaterTimerNotify ( *this ),
    repeaterSubscribeTmr (
        m_repeaterTimerNotify, timerQueue, cbMutexIn, ctxNotifyIn ),
    govTmr ( *this, timerQueue, cacMutexIn ),
    maxPeriod ( getMaxPeriod() ),
    rtteMean ( minRoundTripEstimate ),
    rtteMeanDev ( 0 ),
    cacRef ( cac ),
    cbMutex ( cbMutexIn ),
    cacMutex ( cacMutexIn ),
    nTimers ( getNTimers(maxPeriod) ),
    ppSearchTmr ( nTimers ),
    nBytesInXmitBuf ( 0 ),
    beaconAnomalyTimerIndex ( 0 ),
    sequenceNumber ( 0 ),
    lastReceivedSeqNo ( 0 ),
    sock ( 0 ),
    repeaterPort ( 0 ),
    serverPort ( port ),
    localPort ( 0 ),
    shutdownCmd ( false ),
    lastReceivedSeqNoIsValid ( false )
{
    cacGuard.assertIdenticalMutex ( cacMutex );

    double powerOfTwo = log ( beaconAnomalySearchPeriod / minRoundTripEstimate ) / log ( 2.0 );
    this->beaconAnomalyTimerIndex = static_cast < unsigned > ( powerOfTwo + 1.0 );
    if ( this->beaconAnomalyTimerIndex >= this->nTimers ) {
        this->beaconAnomalyTimerIndex = this->nTimers - 1;
    }

    for ( unsigned i = 0; i < this->nTimers; i++ ) {
        this->ppSearchTmr[i].reset ( 
            new searchTimer ( *this, timerQueue, i, cacMutexIn, 
                i > this->beaconAnomalyTimerIndex ) ); 
    }

    this->repeaterPort = 
        envGetInetPortConfigParam ( &EPICS_CA_REPEATER_PORT,
                                    static_cast <unsigned short> (CA_REPEATER_PORT) );

    this->sock = epicsSocketCreate ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( this->sock == INVALID_SOCKET ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ("CAC: unable to create datagram socket because = \"%s\"\n",
            sockErrBuf );
        throwWithLocation ( noSocket () );
    }

#ifdef IP_ADD_MEMBERSHIP
    {
        int flag = 1;
        if ( setsockopt ( this->sock, IPPROTO_IP, IP_MULTICAST_LOOP,
                          (char *) &flag, sizeof ( flag ) ) == -1 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf("CAC: failed to set mcast loopback\n");
        }
    }
#endif

#ifdef IP_MULTICAST_TTL
    {
        int ttl;
        long val;
        if(envGetLongConfigParam(&EPICS_CA_MCAST_TTL, &val))
            val =1;
        ttl = val;
        if ( setsockopt(this->sock, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl))) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf("CAC: failed to set mcast ttl %d\n", ttl);
        }
    }
#endif

    int boolValue = true;
    int status = setsockopt ( this->sock, SOL_SOCKET, SO_BROADCAST, 
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
    static const unsigned short PORT_ANY = 0u;
    osiSockAddr addr;
    memset ( (char *)&addr, 0 , sizeof (addr) );
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl ( INADDR_ANY ); 
    addr.ia.sin_port = htons ( PORT_ANY );
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
        this->localPort = ntohs ( tmpAddr.ia.sin_port );
    }

    /*
     * load user and auto configured
     * broadcast address list
     */
    ELLLIST dest;
    ellInit ( & dest );
    configureChannelAccessAddressList ( & dest, this->sock, this->serverPort );
    while ( osiSockAddrNode * 
        pNode = reinterpret_cast < osiSockAddrNode * > ( ellGet ( & dest ) ) ) {
        SearchDestUDP & searchDest = * 
            new SearchDestUDP ( pNode->addr, *this );
        _searchDestList.add ( searchDest );
        free ( pNode );
    }

    /* add list of tcp name service addresses */
    _searchDestList.add ( searchDestListIn );
    
    caStartRepeaterIfNotInstalled ( this->repeaterPort );

    this->pushVersionMsg ();

    // start timers and receive thread
    for ( unsigned j =0; j < this->nTimers; j++ ) {
        this->ppSearchTmr[j]->start ( cacGuard ); 
    }
    this->govTmr.start ();
    this->repeaterSubscribeTmr.start ();
    this->recvThread.start ();
}

/*
 *  udpiiu::~udpiiu ()
 */
udpiiu::~udpiiu ()
{
    {
        epicsGuard < epicsMutex > cbGuard ( this->cbMutex );
        epicsGuard < epicsMutex > guard ( this->cacMutex );
        this->shutdown ( cbGuard, guard );
    }

    tsDLIter < SearchDest > iter ( _searchDestList.firstIter () );
    while ( iter.valid () )
    {
        SearchDest & curr ( *iter );
        iter++;
        delete & curr;
    }
    
    epicsSocketDestroy ( this->sock );
}

void udpiiu::shutdown ( 
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & guard )
{
    // stop all of the timers
    this->repeaterSubscribeTmr.shutdown ( cbGuard, guard );
    this->govTmr.shutdown ( cbGuard, guard );
    for ( unsigned i =0; i < this->nTimers; i++ ) {
        this->ppSearchTmr[i]->shutdown ( cbGuard, guard ); 
    }

    {
        this->shutdownCmd = true;
        epicsGuardRelease < epicsMutex > unguard ( guard );
        {
            epicsGuardRelease < epicsMutex > cbUnguard ( cbGuard );

            if ( ! this->recvThread.exitWait ( 0.0 ) ) {
                unsigned tries = 0u;

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
    
    if ( this->iiu._searchDestList.count () == 0 ) { 
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
            this->iiu.postMsg ( src, this->iiu.recvBuf, 
                (arrayElementCount) status, epicsTime::getCurrent() );
        }

    } while ( ! this->iiu.shutdownCmd );
}

/* for sunpro compiler */
udpiiu::M_repeaterTimerNotify::~M_repeaterTimerNotify () 
{
}

/*
 *  udpiiu::M_repeaterTimerNotify::repeaterRegistrationMessage ()
 *
 *  register with the repeater 
 */
void udpiiu :: M_repeaterTimerNotify :: repeaterRegistrationMessage ( unsigned attemptNumber )
{
    epicsGuard < epicsMutex > cbGuard ( m_udpiiu.cacMutex );
    caRepeaterRegistrationMessage ( m_udpiiu.sock, m_udpiiu.repeaterPort, attemptNumber );
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
            saddr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
            saddr.ia.sin_port = htons ( port );
        }
        else {
            saddr.ia.sin_port = htons ( port );
        }
    }
    else {
        saddr.ia.sin_family = AF_INET;
        saddr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
        saddr.ia.sin_port = htons ( port );
    }

    memset ( (char *) &msg, 0, sizeof (msg) );
    AlignedWireRef < epicsUInt16 > ( msg.m_cmmd ) = REPEATER_REGISTER;
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
        bd.ia.sin_addr.s_addr = htonl ( INADDR_ANY ); 
        bd.ia.sin_port = htons ( port );   
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
    const caHdr &msg, const osiSockAddr &netAddr, const epicsTime &currentTime )
{
    char buf[64];
    sockAddrToDottedIP ( &netAddr.sa, buf, sizeof ( buf ) );
    char date[64];
    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");
    errlogPrintf ( "CAC: Undecipherable ( bad msg code %u ) UDP message from %s at %s\n", 
                msg.m_cmmd, buf, date );
    return false;
}

bool udpiiu::versionAction ( 
    const caHdr & hdr, const osiSockAddr &, const epicsTime & /* currentTime */ )
{
    epicsGuard < epicsMutex > guard ( this->cacMutex );

    // update the round trip time estimate
    if ( hdr.m_dataType & sequenceNoIsValid ) {
        this->lastReceivedSeqNo = hdr.m_cid;
        this->lastReceivedSeqNoIsValid = true;
    }

    return true;
}

bool udpiiu :: searchRespAction ( 
    const caHdr & msg, const osiSockAddr & addr, 
    const epicsTime & currentTime )
{
    /*
     * we dont currently know what to do with channel's 
     * found to be at non-IP type addresses
     */
    if ( addr.sa.sa_family != AF_INET ) {
        return true;
    }

    /*
     * Starting with CA V4.1 the minor version number
     * is appended to the end of each UDP search reply.
     * This value is ignored by earlier clients.
     */
    ca_uint32_t minorVersion;
    if ( msg.m_postsize >= sizeof ( minorVersion ) ){
        /*
         * care is taken here not to break gcc 3.2 aggressive alias
         * analysis rules
         */
        const ca_uint8_t * pPayLoad = 
            reinterpret_cast < const ca_uint8_t *> ( & msg + 1 );
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
       cacRef.transferChanToVirtCircuit 
            ( msg.m_available, msg.m_cid, 0xffff, 
                0, minorVersion, serverAddr, currentTime );
    }
    else {
        cacRef.transferChanToVirtCircuit 
            ( msg.m_available, msg.m_cid, msg.m_dataType, 
                msg.m_count, minorVersion, serverAddr, currentTime );
    }

    return true;
}

bool udpiiu::beaconAction ( 
    const caHdr & msg, 
    const osiSockAddr & net_addr, const epicsTime & currentTime )
{
    struct sockaddr_in ina;

    memset(&ina, 0, sizeof(struct sockaddr_in));

    if ( net_addr.sa.sa_family != AF_INET ) {
        return false;
    }

    /* 
     * this allows a fan-out server to potentially
     * insert the true address of the CA server 
     *
     * old servers:
     *   1) set this field to one of the ip addresses of the host _or_
     *   2) set this field to INADDR_ANY
     * new servers:
     *   always set this field to INADDR_ANY
     *
     * clients always assume that if this
     * field is set to something that isnt INADDR_ANY
     * then it is the overriding IP address of the server.
     */
    ina.sin_family = AF_INET;
    ina.sin_addr.s_addr = htonl ( msg.m_available );
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
    unsigned protocolRevision = msg.m_dataType;
    ca_uint32_t beaconNumber = msg.m_cid;

    this->cacRef.beaconNotify ( ina, currentTime, 
        beaconNumber, protocolRevision );

    return true;
}

bool udpiiu::repeaterAckAction ( 
    const caHdr &,  
    const osiSockAddr &, const epicsTime &)
{
    this->repeaterSubscribeTmr.confirmNotify ();
    return true;
}

bool udpiiu::notHereRespAction ( 
    const caHdr &,  
        const osiSockAddr &, const epicsTime & )
{
    return true;
}

bool udpiiu::exceptionRespAction ( 
    const caHdr &msg, 
    const osiSockAddr & net_addr, const epicsTime & currentTime )
{
    const caHdr &reqMsg = * ( &msg + 1 );
    char name[64];
    sockAddrToDottedIP ( &net_addr.sa, name, sizeof ( name ) );
    char date[64];
    currentTime.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");

    if ( msg.m_postsize > sizeof ( caHdr ) ){
        errlogPrintf ( 
            "error condition \"%s\" detected by %s with context \"%s\" at %s\n", 
            ca_message ( msg.m_available ), 
            name, reinterpret_cast <const char *> ( &reqMsg + 1 ), date );
    }
    else{
        errlogPrintf ( 
            "error condition \"%s\" detected by %s at %s\n", 
            ca_message ( msg.m_available ), name, date );
    }

    return true;
}

void udpiiu::postMsg ( 
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
            errlogPrintf ( 
                "%s: Undecipherable (too small) UDP msg from %s ignored\n", 
                    __FILE__,  buf );
            return;
        }

        pCurMsg = reinterpret_cast < caHdr * > ( pInBuf );

        /* 
         * fix endian of bytes 
         */
        pCurMsg->m_postsize = AlignedWireRef < epicsUInt16 > ( pCurMsg->m_postsize );
        pCurMsg->m_cmmd = AlignedWireRef < epicsUInt16 > ( pCurMsg->m_cmmd );
        pCurMsg->m_dataType = AlignedWireRef < epicsUInt16 > ( pCurMsg->m_dataType );
        pCurMsg->m_count = AlignedWireRef < epicsUInt16 > ( pCurMsg->m_count );
        pCurMsg->m_available = AlignedWireRef < epicsUInt32 > ( pCurMsg->m_available );
        pCurMsg->m_cid = AlignedWireRef < epicsUInt32 > ( pCurMsg->m_cid );

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
            errlogPrintf ( 
                "%s: Undecipherable (payload too small) UDP msg from %s ignored\n", 
                __FILE__, buf );
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
            errlogPrintf ( "CAC: Undecipherable UDP message from %s\n", buf );
            return;
        }

        blockSize -= size;
        pInBuf += size;;
    }
}

bool udpiiu::pushVersionMsg ()
{
    epicsGuard < epicsMutex > guard ( this->cacMutex );

    this->sequenceNumber++;

    caHdr msg;
    AlignedWireRef < epicsUInt16 > ( msg.m_cmmd ) = CA_PROTO_VERSION;
    AlignedWireRef < epicsUInt32 > ( msg.m_available ) = 0;
    AlignedWireRef < epicsUInt16 > ( msg.m_dataType ) = sequenceNoIsValid; 
    AlignedWireRef < epicsUInt16 > ( msg.m_count ) = CA_MINOR_PROTOCOL_REVISION;
    AlignedWireRef < epicsUInt32 > ( msg.m_cid ) = this->sequenceNumber; // sequence number

    return this->pushDatagramMsg ( guard, msg, 0, 0 );
}

bool udpiiu::pushDatagramMsg ( epicsGuard < epicsMutex > & guard, 
    const caHdr & msg, const void * pExt, ca_uint16_t extsize )
{
    guard.assertIdenticalMutex ( this->cacMutex );

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
    if ( extsize ) {
        memcpy ( pbufmsg + 1, pExt, extsize );
        if ( extsize != alignedExtSize ) {
            char *pDest = (char *) ( pbufmsg + 1 );
            memset ( pDest + extsize, '\0', alignedExtSize - extsize );
        }
    }
    AlignedWireRef < epicsUInt16 > ( pbufmsg->m_postsize ) = alignedExtSize;
    this->nBytesInXmitBuf += msgsize;

    return true;
}

udpiiu :: SearchDestUDP :: SearchDestUDP ( 
    const osiSockAddr & destAddr, udpiiu & udpiiuIn ) :
    _destAddr ( destAddr ), _udpiiu ( udpiiuIn )
{
}

void udpiiu :: SearchDestUDP :: searchRequest ( 
            epicsGuard < epicsMutex > & guard, const char * pBuf, size_t bufSize )
{
    guard.assertIdenticalMutex ( _udpiiu.cacMutex );
    assert ( bufSize <= INT_MAX );
    int bufSizeAsInt = static_cast < int > ( bufSize );
    while ( true ) {
        // This const_cast is needed for vxWorks:
        int status = sendto ( _udpiiu.sock, const_cast<char *>(pBuf), bufSizeAsInt, 0, 
                & _destAddr.sa, sizeof ( _destAddr.sa ) );
        if ( status == bufSizeAsInt ) {
            break;
        }
        if ( status >= 0 ) {
            errlogPrintf ( "CAC: UDP sendto () call returned strange xmit count?\n" );
            break;
        }
        else {
            int localErrno = SOCKERRNO;

            if ( localErrno == SOCK_EINTR ) {
                if ( _udpiiu.shutdownCmd ) {
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
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                char buf[64];
                sockAddrToDottedIP ( &_destAddr.sa, buf, sizeof ( buf ) );
                errlogPrintf (
                    "CAC: error = \"%s\" sending UDP msg to %s\n",
                    sockErrBuf, buf);
                break;
            }
        }
    }
}
            
void udpiiu :: SearchDestUDP :: show ( 
    epicsGuard < epicsMutex > & guard, unsigned level ) const
{
    guard.assertIdenticalMutex ( _udpiiu.cacMutex );
    char buf[64];
    sockAddrToDottedIP ( &_destAddr.sa, buf, sizeof ( buf ) );
    :: printf ( "UDP Search destination \"%s\"\n", buf );
}

udpiiu :: SearchRespCallback :: SearchRespCallback ( udpiiu & udpiiuIn ) : 
    _udpiiu ( udpiiuIn )
{
}
    
void udpiiu :: SearchRespCallback :: notify (
    const caHdr & msg, const void * pPayloadUntyped,
        const osiSockAddr & addr, const epicsTime & currentTime )
{   
    /*
     * we dont currently know what to do with channel's 
     * found to be at non-IP type addresses
     */
    if ( addr.sa.sa_family != AF_INET ) {
        return;
    }

    /*
     * Starting with CA V4.1 the minor version number
     * is appended to the end of each search reply.
     * This value is ignored by earlier clients.
     */
    ca_uint32_t minorVersion;
    if ( msg.m_postsize >= sizeof ( minorVersion ) ){
        /*
         * care is taken here not to break gcc 3.2 aggressive alias
         * analysis rules
         */
        const ca_uint8_t * pPayLoad = reinterpret_cast < const ca_uint8_t *> ( pPayloadUntyped );
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
        serverAddr.ia.sin_port = htons ( msg.m_dataType );
    }
    else if ( CA_V45 (minorVersion) ) {
        serverAddr.ia.sin_port = htons ( msg.m_dataType );
        serverAddr.ia.sin_addr = addr.ia.sin_addr;
    }
    else {
        serverAddr.ia.sin_port = htons ( _udpiiu.serverPort );
        serverAddr.ia.sin_addr = addr.ia.sin_addr;
    }

    if ( CA_V42 ( minorVersion ) ) {
       _udpiiu.cacRef.transferChanToVirtCircuit 
            ( msg.m_available, msg.m_cid, 0xffff, 
                0, minorVersion, serverAddr, currentTime );
    }
    else {
        _udpiiu.cacRef.transferChanToVirtCircuit 
            ( msg.m_available, msg.m_cid, msg.m_dataType, 
                msg.m_count, minorVersion, serverAddr, currentTime );
    }
}

void udpiiu :: SearchRespCallback :: show ( 
    epicsGuard < epicsMutex > & guard, unsigned level ) const
{
    guard.assertIdenticalMutex ( _udpiiu.cacMutex );
    ::printf ( "udpiiu :: SearchRespCallback\n" );
}

bool udpiiu :: datagramFlush ( 
    epicsGuard < epicsMutex > & guard, const epicsTime & currentTime )
{
    guard.assertIdenticalMutex ( cacMutex );

    // dont send the version header by itself
    if ( this->nBytesInXmitBuf <= sizeof ( caHdr ) ) {
        return false;
    }

    tsDLIter < SearchDest > iter ( _searchDestList.firstIter () );
    while ( iter.valid () )
    {
        iter->searchRequest ( guard, this->xmitBuf, this->nBytesInXmitBuf );
        iter++;
    }

    this->nBytesInXmitBuf = 0u;

    this->pushVersionMsg ();

    return true;
}

void udpiiu :: show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->cacMutex );

    ::printf ( "Datagram IO circuit (and disconnected channel repository)\n");
    if ( level > 1u ) {
        ::printf ("\trepeater port %u\n", this->repeaterPort );
        ::printf ("\tdefault server port %u\n", this->serverPort );
        ::printf ( "Search Destination List with %u items\n", 
            _searchDestList.count () );
        if ( level > 2u ) {
            tsDLIterConst < SearchDest > iter ( 
                _searchDestList.firstIter () );
            while ( iter.valid () )
            {
                iter->show ( guard, level - 2 );
                iter++;
            }
        }
    }
    if ( level > 2u ) {
        ::printf ("\tsocket identifier %d\n", this->sock );
        ::printf ("\tbytes in xmit buffer %u\n", this->nBytesInXmitBuf );
        ::printf ("\tshut down command bool %u\n", this->shutdownCmd );
        ::printf ( "\trecv thread exit signal:\n" );
        this->recvThread.show ( level - 2u );
        this->repeaterSubscribeTmr.show ( level - 2u );
        this->govTmr.show ( level - 2u );
    }
    if ( level > 3u ) {
        for ( unsigned i =0; i < this->nTimers; i++ ) {
            this->ppSearchTmr[i]->show ( level - 3u );
        }
    }
}

bool udpiiu::wakeupMsg ()
{
    caHdr msg;
    AlignedWireRef < epicsUInt16 > ( msg.m_cmmd ) = CA_PROTO_VERSION;
    AlignedWireRef < epicsUInt32 > ( msg.m_available ) = 0u;
    AlignedWireRef < epicsUInt16 > ( msg.m_dataType ) = 0u;
    AlignedWireRef < epicsUInt16 > ( msg.m_count ) = 0u;
    AlignedWireRef < epicsUInt32 > ( msg.m_cid ) = 0u;
    AlignedWireRef < epicsUInt16 > ( msg.m_postsize ) = 0u;

    osiSockAddr addr;
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
    addr.ia.sin_port = htons ( this->localPort );

    // send a wakeup msg so the UDP recv thread will exit
    int status = sendto ( this->sock, reinterpret_cast < char * > ( &msg ),  
            sizeof (msg), 0, &addr.sa, sizeof ( addr.sa ) );
    if ( status == sizeof (msg) ) {
        return true;
    }
    return false;
}

void udpiiu::beaconAnomalyNotify ( 
    epicsGuard < epicsMutex > & cacGuard ) 
{
    for ( unsigned i = this->beaconAnomalyTimerIndex+1u; 
            i < this->nTimers; i++ ) {
        this->ppSearchTmr[i]->moveChannels ( cacGuard, 
            *this->ppSearchTmr[this->beaconAnomalyTimerIndex] );
    }
}

void udpiiu::uninstallChanDueToSuccessfulSearchResponse ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    const epicsTime & currentTime )
{
    channelNode::channelState chanState = 
        chan.channelNode::listMember;
    if ( chanState == channelNode::cs_disconnGov ) {
        this->govTmr.uninstallChan ( guard, chan );
    }
    else {
        this->ppSearchTmr[ chan.getSearchTimerIndex ( guard ) ]-> 
            uninstallChanDueToSuccessfulSearchResponse ( 
            guard, chan, this->lastReceivedSeqNo, 
            this->lastReceivedSeqNoIsValid, currentTime );
    }
}

void udpiiu::uninstallChan ( 
        epicsGuard < epicsMutex > & guard, nciu & chan )
{
    channelNode::channelState chanState = 
        chan.channelNode::listMember;
    if ( chanState == channelNode::cs_disconnGov ) {
        this->govTmr.uninstallChan ( guard, chan );
    }
    else {
        this->ppSearchTmr[ chan.getSearchTimerIndex ( guard ) ]-> 
            uninstallChan ( guard, chan );
    }
}

bool udpiiu::searchMsg (
    epicsGuard < epicsMutex > & guard, ca_uint32_t id, 
        const char * pName, unsigned nameLength )
{
    caHdr msg;
    AlignedWireRef < epicsUInt16 > ( msg.m_cmmd ) = CA_PROTO_SEARCH;
    AlignedWireRef < epicsUInt32 > ( msg.m_available ) = id;
    AlignedWireRef < epicsUInt16 > ( msg.m_dataType ) = DONTREPLY;
    AlignedWireRef < epicsUInt16 > ( msg.m_count ) = CA_MINOR_PROTOCOL_REVISION;
    AlignedWireRef < epicsUInt32 > ( msg.m_cid ) = id;
    return this->pushDatagramMsg ( 
        guard, msg, pName, (ca_uint16_t) nameLength );
}

void udpiiu::installNewChannel ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, netiiu * & piiu )
{
    piiu = this;
    this->ppSearchTmr[0]->installChannel ( guard, chan );
}

void udpiiu::installDisconnectedChannel ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    chan.setServerAddressUnknown ( *this, guard );
    this->govTmr.installChan ( guard, chan );
}

void udpiiu::noSearchRespNotify ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, unsigned index )
{
    const unsigned nTimersMinusOne = this->nTimers - 1;
    if ( index < nTimersMinusOne ) {
        index++;
    }
    else {
        index = nTimersMinusOne;
    }
    this->ppSearchTmr[index]->installChannel ( guard, chan );
}

void udpiiu::boostChannel ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    this->ppSearchTmr[this->beaconAnomalyTimerIndex]->
            installChannel ( guard, chan );
}

void udpiiu::govExpireNotify ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    this->ppSearchTmr[0]->installChannel ( guard, chan );
}

int udpiiu :: M_repeaterTimerNotify :: printFormated ( 
                            epicsGuard < epicsMutex > & cbGuard, 
                            const char * pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = m_udpiiu.cacRef.varArgsPrintFormated ( cbGuard, pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void udpiiu::updateRTTE ( epicsGuard < epicsMutex > & guard, double measured )
{
    guard.assertIdenticalMutex ( this->cacMutex );
    if ( measured > maxRoundTripEstimate ) {
        measured = maxRoundTripEstimate;
    }
    if ( measured < minRoundTripEstimate ) {
        measured = minRoundTripEstimate;
    }
    double error = measured - this->rtteMean;
    this->rtteMean += 0.125 * error;
    if ( error < 0.0 ) {
        error = - error;
    }
    this->rtteMeanDev = this->rtteMeanDev + .25 * ( error - this->rtteMeanDev );
}

double udpiiu::getRTTE ( epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->cacMutex );
    return this->rtteMean + 4 * this->rtteMeanDev;
}

unsigned udpiiu::getHostName ( 
    epicsGuard < epicsMutex > & cacGuard,
    char *pBuf, unsigned bufLength ) const throw ()
{
    return netiiu::getHostName ( cacGuard, pBuf, bufLength );
}

const char * udpiiu::pHostName (
    epicsGuard < epicsMutex > & cacGuard ) const throw ()
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
    netiiu::writeRequest ( guard, chan, type, nElem, pValue );
}

void udpiiu::writeNotifyRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netWriteNotifyIO & io, unsigned type, 
    arrayElementCount nElem, const void *pValue )
{
    netiiu::writeNotifyRequest ( guard, chan, io, type, nElem, pValue );
}

void udpiiu::readNotifyRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netReadNotifyIO & io, unsigned type, arrayElementCount nElem )
{
    netiiu::readNotifyRequest ( guard, chan, io, type, nElem );
}

void udpiiu::clearChannelRequest ( 
    epicsGuard < epicsMutex > & guard, 
    ca_uint32_t sid, ca_uint32_t cid )
{
    netiiu::clearChannelRequest ( guard, sid, cid );
}

void udpiiu::subscriptionRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netSubscription & subscr )
{
    netiiu::subscriptionRequest ( guard, chan, subscr );
}

void udpiiu::subscriptionUpdateRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    netSubscription & subscr )
{
    netiiu::subscriptionUpdateRequest (
        guard, chan, subscr );
}

void udpiiu::subscriptionCancelRequest ( 
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, netSubscription & subscr )
{
    netiiu::subscriptionCancelRequest ( guard, chan, subscr );
}

void udpiiu::flushRequest ( 
    epicsGuard < epicsMutex > & guard )
{
    netiiu::flushRequest ( guard );
}

unsigned udpiiu::requestMessageBytesPending ( 
    epicsGuard < epicsMutex > & guard )
{
    return netiiu::requestMessageBytesPending ( guard );
}

void udpiiu::flush ( 
    epicsGuard < epicsMutex > & guard )
{
    netiiu::flush ( guard );
}

void udpiiu::requestRecvProcessPostponedFlush (
    epicsGuard < epicsMutex > & guard )
{
    netiiu::requestRecvProcessPostponedFlush ( guard );
}

osiSockAddr udpiiu::getNetworkAddress (
    epicsGuard < epicsMutex > & guard ) const
{
    return netiiu::getNetworkAddress ( guard );
}

double udpiiu::receiveWatchdogDelay (
    epicsGuard < epicsMutex > & guard ) const
{
    return netiiu::receiveWatchdogDelay ( guard );
}

ca_uint32_t udpiiu::datagramSeqNumber (
    epicsGuard < epicsMutex > & ) const
{
    return this->sequenceNumber;
}

