
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

#include "osiProcess.h"

#include "iocinf.h"
#include "addrList.h"
#include "inetAddrID_IL.h"
#include "netiiu_IL.h"

typedef void (*pProtoStubUDP) (udpiiu *piiu, caHdr *pMsg, const struct sockaddr_in *pnet_addr);

// UDP protocol dispatch table
const udpiiu::pProtoStubUDP udpiiu::udpJumpTableCAC[] = 
{
    udpiiu::noopAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::searchRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::exceptionRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::beaconAction,
    udpiiu::notHereRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::repeaterAckAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction,
    udpiiu::badUDPRespAction
};

//
//  udpiiu::recvMsg ()
//
void udpiiu::recvMsg ()
{
    osiSockAddr src;
    osiSocklen_t src_size = sizeof (src);
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
        ca_printf ( "Unexpected UDP recv error was \"%s\"\n", 
            SOCKERRSTR (errnoCpy) );
    }
    else if ( status > 0 ) {
        status = this->postMsg ( src,
                    this->recvBuf, (unsigned long) status );
        if ( status != ECA_NORMAL ) {
            char buf[64];

            sockAddrToA ( &src.sa, buf, sizeof ( buf ) );

            ca_printf (
                "%s: bad UDP msg from %s because \"%s\"\n", __FILE__, 
                            buf, ca_message (status) );
            return;
        }
    }
    
    return;
}

/*
 *  cacRecvThreadUDP ()
 */
extern "C" void cacRecvThreadUDP (void *pParam)
{
    udpiiu *piiu = (udpiiu *) pParam;

    do {
        piiu->recvMsg ();
    } while ( ! piiu->shutdownCmd );

    semBinaryGive ( piiu->recvThreadExitSignal );
}

/*
 *  udpiiu::repeaterRegistrationMessage ()
 *
 *  register with the repeater 
 */
void udpiiu::repeaterRegistrationMessage ( unsigned attemptNumber )
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
        saddr = osiLocalAddr ( this->sock );
        if ( saddr.sa.sa_family != AF_INET ) {
            /*
             * use the loop back address to communicate with the CA repeater
             * if this os does not have interface query capabilities
             *
             * this will only work with 3.13 beta 12 CA repeaters or later
             */
            saddr.ia.sin_family = AF_INET;
            saddr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
            saddr.ia.sin_port = htons ( this->repeaterPort );   
        }
        else {
            saddr.ia.sin_port = htons ( this->repeaterPort );   
        }
    }
    else {
        saddr.ia.sin_family = AF_INET;
        saddr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
        saddr.ia.sin_port = htons ( this->repeaterPort );   
    }

    memset ( (char *) &msg, 0, sizeof (msg) );
    msg.m_cmmd = htons ( REPEATER_REGISTER );
    msg.m_available = saddr.ia.sin_addr.s_addr;

    /*
     * Intentionally sending a zero length message here
     * until most CA repeater daemons have been restarted
     * (and only then will accept the above protocol)
     * (repeaters began accepting this protocol
     * starting with EPICS 3.12)
     *
     * SOLARIS will not accept a zero length message
     * and we are just porting there for 3.12 so
     * we will use the new protocol for 3.12
     *
     * recent versions of UCX will not accept a zero 
     * length message and we will assume that folks
     * using newer versions of UCX have rebooted (and
     * therefore restarted the CA repeater - and therefore
     * moved it to an EPICS release that accepts this protocol)
     */
#   if defined ( DOES_NOT_ACCEPT_ZERO_LENGTH_UDP )
        len = sizeof (msg);
#   else 
        len = 0;
#   endif 

    status = sendto ( this->sock, (char *) &msg, len,  
                0, (struct sockaddr *) &saddr, sizeof ( saddr ) );
    if ( status < 0 ) {
        int errnoCpy = SOCKERRNO;
        if ( errnoCpy != SOCK_EINTR && 
            /*
             * This is returned from Linux when
             * the repeater isnt running
             */
            errnoCpy != SOCK_ECONNREFUSED ) {
            ca_printf ( "CAC: error sending to repeater was \"%s\"\n", 
                SOCKERRSTR (errnoCpy) );
        }
    }
}

/*
 *  repeater_installed ()
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
bool udpiiu::repeaterInstalled ()
{
    bool                installed = false;
    int                 status;
    SOCKET              sock;
    struct sockaddr_in  bd;
    int                 flag;

    sock = socket ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( sock == INVALID_SOCKET ) {
        return installed;
    }

    memset ( (char *) &bd, 0, sizeof ( bd ) );
    bd.sin_family = AF_INET;
    bd.sin_addr.s_addr = htonl ( INADDR_ANY ); 
    bd.sin_port = htons ( this->repeaterPort );   
    status = bind ( sock, (struct sockaddr *) &bd, sizeof ( bd ) );
    if ( status < 0 ) {
        if ( SOCKERRNO == SOCK_EADDRINUSE ) {
            installed = true;
        }
    }

    /*
     * turn on reuse only after the test so that
     * this works on kernels that support multicast
     */
    flag = TRUE;
    status = setsockopt ( sock, SOL_SOCKET, SO_REUSEADDR, 
                (char *)&flag, sizeof ( flag ) );
    if (status<0) {
        ca_printf ( "CAC: set socket option reuseaddr set failed\n");
    }

    socket_close ( sock );

    return installed;
}

//
// udpiiu::udpiiu ()
//
udpiiu::udpiiu ( cac &cac ) :
    netiiu ( cac ), shutdownCmd ( false )
{
    static const unsigned short PORT_ANY = 0u;
    osiSockAddr addr;
    int boolValue = TRUE;
    int status;

    this->repeaterPort = 
        envGetInetPortConfigParam (&EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT);

    this->serverPort = 
        envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT, CA_SERVER_PORT );

    this->sock = socket ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( this->sock == INVALID_SOCKET ) {
        ca_printf ("CAC: unable to create datagram socket because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        throwWithLocation ( noSocket () );
    }

    status = setsockopt ( this->sock, SOL_SOCKET, SO_BROADCAST, 
                (char *) &boolValue, sizeof (boolValue) );
    if (status<0) {
        ca_printf ("CAC: unable to enable IP broadcasting because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
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
            ca_printf ("CAC: unable to set socket option SO_RCVBUF because \"%s\"\n",
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
        ca_printf ("CAC: unable to bind to an unconstrained address because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        throwWithLocation ( noSocket () );
    }

    this->nBytesInXmitBuf = 0u;

    this->recvThreadExitSignal = semBinaryCreate ( semEmpty );
    if ( ! this->recvThreadExitSignal ) {
        socket_close ( this->sock );
        throwWithLocation ( noMemory () );
    }

    /*
     * load user and auto configured
     * broadcast address list
     */
    ellInit ( &this->dest );
    configureChannelAccessAddressList ( &this->dest, this->sock, this->serverPort );
    if ( ellCount ( &this->dest ) == 0 ) {
        genLocalExcep ( this->clientCtx (), ECA_NOSEARCHADDR, NULL );
    }
  
    {
        unsigned priorityOfSelf = threadGetPrioritySelf ();
        unsigned priorityOfRecv;
        threadId tid;
        threadBoolStatus tbs;

        tbs  = threadLowestPriorityLevelAbove ( priorityOfSelf, &priorityOfRecv );
        if ( tbs != tbsSuccess ) {
            priorityOfRecv = priorityOfSelf;
        }

        tid = threadCreate ( "CAC-UDP", priorityOfRecv,
                threadGetStackSize (threadStackMedium), cacRecvThreadUDP, this );
        if (tid==0) {
            ca_printf ("CA: unable to create UDP receive thread\n");
            semBinaryDestroy (this->recvThreadExitSignal);
            socket_close (this->sock);
            throwWithLocation ( noMemory () );
        }
    }

    if ( ! this->repeaterInstalled () ) {
        osiSpawnDetachedProcessReturn osptr;
        
	    /*
	     * This is not called if the repeater is known to be 
	     * already running. (in the event of a race condition 
	     * the 2nd repeater exits when unable to attach to the 
	     * repeater's port)
	     */
        osptr = osiSpawnDetachedProcess ( "CA Repeater", "caRepeater" );
        if ( osptr == osiSpawnDetachedProcessNoSupport ) {
            threadId tid;

            tid = threadCreate ( "CAC-repeater", threadPriorityLow,
                    threadGetStackSize (threadStackMedium), caRepeaterThread, 0);
            if ( tid == 0 ) {
                ca_printf ("CA: unable to create CA repeater daemon thread\n");
            }
        }
        else if ( osptr == osiSpawnDetachedProcessFail ) {
            ca_printf ( "CA: unable to start CA repeater daemon detached process\n" );
        }
    }
}

/*
 *  udpiiu::~udpiiu ()
 */
udpiiu::~udpiiu ()
{

    // closes the udp socket
    this->shutdown ();

    this->detachAllChan ();

    // wait for recv threads to exit
    semBinaryMustTake ( this->recvThreadExitSignal );

    semBinaryDestroy ( this->recvThreadExitSignal );

    ellFree ( &this->dest );

    if ( this->sock != INVALID_SOCKET ) {
        socket_close ( this->sock );
    }
}

/*
 *  udpiiu::shutdown ()
 */
void udpiiu::shutdown ()
{
    bool laborRequired;

    this->lock ();
    if ( ! this->shutdownCmd ) {
        this->shutdownCmd = true;
        laborRequired = true;
    }
    else {
        laborRequired = false;
    }
    this->unlock ();

    if ( laborRequired ) {
        int status;
        osiSockAddr addr;
        osiSocklen_t size = sizeof ( addr.sa );

        status = getsockname ( this->sock, &addr.sa, &size );
        if ( status < 0 ) {
            // this knocks the UDP input thread out of recv ()
            // on all os except linux
            socket_close ( this->sock );
            this->sock = INVALID_SOCKET;
        }
        else {
            caHdr msg;
            msg.m_cmmd = htons ( CA_PROTO_NOOP );
            msg.m_available = htonl ( 0u );
            msg.m_dataType = htons ( 0u );
            msg.m_count = htons ( 0u );
            msg.m_cid = htonl ( 0u );
            msg.m_postsize = htons ( 0u );

            addr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );

            // send a wakeup msg so the UDP recv thread will exit
            status = sendto ( this->sock, reinterpret_cast < const char * > ( &msg ),  sizeof (msg), 0, 
                    &addr.sa, sizeof ( addr.sa ) );
            if ( status < 0 ) {
                // this knocks the UDP input thread out of recv ()
                // on all os except linux
                socket_close ( this->sock );
                this->sock = INVALID_SOCKET;
            }
        }
    }
}

void udpiiu::badUDPRespAction ( const caHdr &msg, const osiSockAddr &netAddr )
{
    char buf[256];
    sockAddrToA ( &netAddr.sa, buf, sizeof ( buf ) );
    ca_printf ( "CAC: Bad response code in UDP message from %s was %u\n", 
        buf, msg.m_cmmd);
}

void udpiiu::noopAction ( const caHdr &, const osiSockAddr & )
{
    return;
}

void udpiiu::searchRespAction ( const caHdr &msg, const osiSockAddr &addr )
{
    osiSockAddr serverAddr;
    unsigned minorVersion;
    ca_uint16_t *pMinorVersion;

    if ( addr.sa.sa_family != AF_INET ) {
        return;
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
    if ( CA_V48 (CA_PROTOCOL_VERSION,minorVersion) ) {
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
        serverAddr.ia.sin_port = htons (msg.m_dataType);
    }
    else if ( CA_V45 (CA_PROTOCOL_VERSION,minorVersion) ) {
        serverAddr.ia.sin_port = htons ( msg.m_dataType );
        serverAddr.ia.sin_addr = addr.ia.sin_addr;
    }
    else {
        serverAddr.ia.sin_port = htons ( this->serverPort );
        serverAddr.ia.sin_addr = addr.ia.sin_addr;
    }

    if ( CA_V42 ( CA_PROTOCOL_VERSION, minorVersion ) ) {
        this->clientCtx ().lookupChannelAndTransferToTCP 
            ( msg.m_available, msg.m_cid, USHRT_MAX, 0, 
            minorVersion, serverAddr );
    }
    else {
        this->clientCtx ().lookupChannelAndTransferToTCP 
            ( msg.m_available, msg.m_cid, msg.m_dataType, 
                minorVersion, msg.m_count, serverAddr );
    }
}

void udpiiu::beaconAction ( const caHdr &msg, const osiSockAddr &net_addr )
{
    struct sockaddr_in ina;

    if ( net_addr.sa.sa_family != AF_INET ) {
        return;
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
    if ( msg.m_available != htonl (INADDR_ANY) ) {
        ina.sin_addr.s_addr = msg.m_available;
    }
    else {
        ina.sin_addr = net_addr.ia.sin_addr;
    }
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

    this->clientCtx ().beaconNotify ( ina );

    return;
}

void udpiiu::repeaterAckAction ( const caHdr &,  const osiSockAddr &)
{
    this->clientCtx ().repeaterSubscribeConfirmNotify ();
}

void udpiiu::notHereRespAction ( const caHdr &,  const osiSockAddr &)
{
    return;
}

void udpiiu::exceptionRespAction ( const caHdr &msg, const osiSockAddr &net_addr )
{
    const caHdr &reqMsg = * ( &msg + 1 );
    char name[64];

    sockAddrToA ( &net_addr.sa, name, sizeof ( name ) );

    if ( msg.m_postsize > sizeof ( caHdr ) ){
        errlogPrintf ( "error condition \"%s\" detected by %s with context \"%s\"\n", 
            ca_message ( msg.m_available ), 
            name, reinterpret_cast <const char *> ( &reqMsg + 1 ) );
    }
    else{
        errlogPrintf ( "error condition \"%s\" detected by %s\n", 
            ca_message ( msg.m_available ), name );
    }

    return;
}


/*
 * post_udp_msg ()
 */
int udpiiu::postMsg ( const osiSockAddr &net_addr, 
              char *pInBuf, unsigned long blockSize )
{
    caHdr *pCurMsg;

    while ( blockSize ) {
        unsigned long size;

        if ( blockSize < sizeof ( *pCurMsg ) ) {
            return ECA_TOLARGE;
        }

        pCurMsg = reinterpret_cast <caHdr *> ( pInBuf );

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
            return ECA_TOLARGE;
        }

        /*
         * execute the response message
         */
        pProtoStubUDP      pStub;
        if ( pCurMsg->m_cmmd >= NELEMENTS ( udpJumpTableCAC ) ) {
            pStub = badUDPRespAction;
        }
        else {
            pStub = udpJumpTableCAC [pCurMsg->m_cmmd];
        }
        (this->*pStub) ( *pCurMsg, net_addr);

        blockSize -= size;
        pInBuf += size;;
    }
    return ECA_NORMAL;
}

void udpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        strncpy ( pBuf, this->pHostName (), bufLength );
        pBuf[bufLength - 1u] = '\0';
    }
}

const char * udpiiu::pHostName () const
{
    return "<disconnected>";
}

/*
 *  udpiiu::pushDatagramMsg ()
 */ 
bool udpiiu::pushDatagramMsg ( const caHdr &msg, const void *pExt, ca_uint16_t extsize )
{
    unsigned long   msgsize;
    ca_uint16_t     allignedExtSize;
    caHdr           *pbufmsg;

    allignedExtSize = CA_MESSAGE_ALIGN ( extsize );
    msgsize = sizeof ( caHdr ) + allignedExtSize;


    /* fail out if max message size exceeded */
    if ( msgsize >= sizeof ( this->xmitBuf ) - 7 ) {
        return false;
    }

    this->lock ();

    if ( msgsize + this->nBytesInXmitBuf > sizeof ( this->xmitBuf ) ) {
        this->unlock ();
        return false;
    }

    pbufmsg = (caHdr *) &this->xmitBuf[this->nBytesInXmitBuf];
    *pbufmsg = msg;
    memcpy ( pbufmsg+1, pExt, extsize );
    if ( extsize != allignedExtSize ) {
        char *pDest = (char *) ( pbufmsg + 1 );
        memset ( pDest + extsize, '\0', allignedExtSize - extsize );
    }
    pbufmsg->m_postsize = htons ( allignedExtSize );
    this->nBytesInXmitBuf += msgsize;

    this->unlock ();

    return true;
}

//
// udpiiu::flush ()
//
void udpiiu::flush ()
{
    osiSockAddrNode  *pNode;

    this->lock ();

    if ( this->nBytesInXmitBuf == 0u ) {
        this->unlock ();
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
                ca_printf ( "CAC: UDP sendto () call returned strange xmit count?\n" );
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

                    sockAddrToA ( &pNode->addr.sa, buf, sizeof ( buf ) );

                    ca_printf (
                        "CAC: error = \"%s\" sending UDP msg to %s\n",
                        SOCKERRSTR ( localErrno ), buf);
                    break;
                }
            }
        }
        pNode = (osiSockAddrNode *) ellNext ( &pNode->node );
    }

    this->nBytesInXmitBuf = 0u;

    this->unlock ();
}

SOCKET udpiiu::getSock () const
{
    return this->sock;
}


