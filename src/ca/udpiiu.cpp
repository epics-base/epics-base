
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

typedef void (*pProtoStubUDP) (udpiiu *piiu, caHdr *pMsg, const struct sockaddr_in *pnet_addr);

//
//  udpiiu::recvMsg ()
//
int udpiiu::recvMsg ()
{
    osiSockAddr         src;
    int                 src_size = sizeof (src);
    int                 status;

    status = recvfrom ( this->sock, this->recvBuf, sizeof ( this->recvBuf ), 0,
                        &src.sa, &src_size );
    if (status < 0) {
        int errnoCpy = SOCKERRNO;

        if ( errnoCpy == SOCK_SHUTDOWN ) {
            return -1;
        }
        if ( errnoCpy == SOCK_ENOTSOCK ) {
            return -1;
        }
        if ( errnoCpy == SOCK_EBADF ) {
            return -1;
        }
        if ( errnoCpy == SOCK_EINTR ) {
            if ( this->shutdownCmd ) {
                return -1;
            }
            else {
                return 0;
            }
        }
#       ifdef linux
            /*
             * Avoid spurious ECONNREFUSED bug
             * in linux
             */
            if ( errnoCpy == SOCK_ECONNREFUSED ) {
                return 0;
            }
#       endif
        ca_printf (
            "Unexpected UDP recv error %s\n", SOCKERRSTR(errnoCpy));
    }
    else if (status > 0) {
        status = this->post_msg ( &src.ia,
                    this->recvBuf, (unsigned long) status );
        if ( status != ECA_NORMAL ) {
            char buf[64];

            ipAddrToA (&src.ia, buf, sizeof(buf));

            ca_printf (
                "%s: bad UDP msg from %s because \"%s\"\n", __FILE__, 
                            buf, ca_message (status) );

            return 0;
        }
    }
    
    return 0;
}

/*
 *  cacRecvThreadUDP ()
 */
extern "C" void cacRecvThreadUDP (void *pParam)
{
    udpiiu *piiu = (udpiiu *) pParam;
    int status;

    do {
        status = piiu->recvMsg ();
    } while ( status == 0 );

    semBinaryGive (piiu->recvThreadExitSignal);
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
int repeater_installed (udpiiu *piiu)
{
    int                 installed = FALSE;
    int                 status;
    SOCKET              sock;
    struct sockaddr_in  bd;
    int                 flag;

    sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return installed;
    }

    memset ( (char *) &bd, 0, sizeof (bd) );
    bd.sin_family = AF_INET;
    bd.sin_addr.s_addr = htonl (INADDR_ANY); 
    bd.sin_port = htons (piiu->repeaterPort);   
    status = bind (sock, (struct sockaddr *) &bd, sizeof(bd) );
    if (status<0) {
        if (SOCKERRNO == SOCK_EADDRINUSE) {
            installed = TRUE;
        }
    }

    /*
     * turn on reuse only after the test so that
     * this works on kernels that support multicast
     */
    flag = TRUE;
    status = setsockopt ( sock, SOL_SOCKET, SO_REUSEADDR, 
                (char *)&flag, sizeof (flag) );
    if (status<0) {
        ca_printf ( "CAC: set socket option reuseaddr set failed\n");
    }

    socket_close (sock);

    return installed;
}

//
// udpiiu::udpiiu ()
//
udpiiu::udpiiu ( cac *pcac ) :
    netiiu ( pcac ),
    shutdownCmd ( false )
{
    static const unsigned short PORT_ANY = 0u;
    osiSockAddr addr;
    int boolValue = TRUE;
    int status;

    this->repeaterPort = 
        envGetInetPortConfigParam (&EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT);

    this->sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (this->sock == INVALID_SOCKET) {
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
    if (status<0) {
        socket_close (this->sock);
        ca_printf ("CAC: unable to bind to an unconstrained address because = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
        throwWithLocation ( noSocket () );
    }

    this->nBytesInXmitBuf = 0u;

    this->xmitBufLock = semMutexCreate ();
    if (!this->xmitBufLock) {
        socket_close (this->sock);
        throwWithLocation ( noMemory () );
    }

    this->recvThreadExitSignal = semBinaryCreate (semEmpty);
    if ( ! this->recvThreadExitSignal ) {
        semMutexDestroy (this->xmitBufLock);
        socket_close (this->sock);
        throwWithLocation ( noMemory () );
    }

    /*
     * load user and auto configured
     * broadcast address list
     */
    ellInit ( &this->dest );
    configureChannelAccessAddressList (&this->dest, this->sock, pcac->ca_server_port);
    if ( ellCount ( &this->dest ) == 0 ) {
        genLocalExcep ( this->pcas, ECA_NOSEARCHADDR, NULL );
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

        tid = threadCreate ("CAC-UDP", priorityOfRecv,
                threadGetStackSize (threadStackMedium), cacRecvThreadUDP, this);
        if (tid==0) {
            ca_printf ("CA: unable to create UDP receive thread\n");
            semBinaryDestroy (this->recvThreadExitSignal);
            semMutexDestroy (this->xmitBufLock);
            socket_close (this->sock);
            throwWithLocation ( noMemory () );
        }
    }

    if ( ! repeater_installed (this) ) {
        osiSpawnDetachedProcessReturn osptr;
        
	    /*
	     * This is not called if the repeater is known to be 
	     * already running. (in the event of a race condition 
	     * the 2nd repeater exits when unable to attach to the 
	     * repeater's port)
	     */
        osptr = osiSpawnDetachedProcess ("CA Repeater", "caRepeater");
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
    nciu *pChan, *pNext;

    // closes the udp socket
    this->shutdown ();

    this->pcas->lock ();
    tsDLIter<nciu> iter (this->chidList);
    pChan = iter ();
    while (pChan) {
        pNext = iter ();
        pChan->destroy ();
        pChan = pNext;
    }
    this->pcas->unlock ();

    // wait for recv threads to exit
    semBinaryMustTake (this->recvThreadExitSignal);

    semMutexDestroy (this->xmitBufLock);
    semBinaryDestroy (this->recvThreadExitSignal);

    ellFree (&this->dest);
}

/*
 *  udpiiu::shutdown ()
 */
void udpiiu::shutdown ()
{
    this->pcas->lock ();
    if ( ! this->shutdownCmd ) {
        int status;

        this->shutdownCmd = true;
        //
        // use of shutdown () for this purpose on UDP
        // sockets does not work on certain OS (i.e. solaris).
        //
        status = socket_close ( this->sock );
        if ( status ) {
            errlogPrintf ( "CAC UDP socket close error was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
        }
    }
    this->pcas->unlock ();
}

/*
 * bad_udp_resp_action ()
 */
LOCAL void bad_udp_resp_action (udpiiu * /* piiu */, 
	caHdr *pMsg, const struct sockaddr_in *pnet_addr)
{
    char buf[256];
    ipAddrToA ( pnet_addr, buf, sizeof (buf) );
    ca_printf ( "CAC: Bad response code in UDP message from %s was %u\n", 
        buf, pMsg->m_cmmd);
}

/*
 * udp_noop_action ()
 */
LOCAL void udp_noop_action (udpiiu * /* piiu */, caHdr * /* pMsg */, 
                            const struct sockaddr_in * /* pnet_addr */)
{
    return;
}

/*
 * search_resp_action ()
 */
LOCAL void search_resp_action (udpiiu *piiu, caHdr *pMsg, const struct sockaddr_in *pnet_addr)
{
    struct sockaddr_in  ina;
    nciu                *chan;
    tcpiiu              *allocpiiu;
    unsigned short      *pMinorVersion;
    unsigned            minorVersion;

    /*
     * ignore broadcast replies for deleted channels
     * 
     * lock required around use of the sprintf buffer
     */
    piiu->pcas->lock ();
    chan = piiu->pcas->lookupChan (pMsg->m_available);
    if ( ! chan ) {
        piiu->pcas->unlock ();
        return;
    }

    /*
     * Starting with CA V4.1 the minor version number
     * is appended to the end of each search reply.
     * This value is ignored by earlier clients.
     */
    if ( pMsg->m_postsize >= sizeof (*pMinorVersion) ){
        pMinorVersion = (unsigned short *) (pMsg+1);
        minorVersion = ntohs (*pMinorVersion);      
    }
    else {
        minorVersion = CA_UKN_MINOR_VERSION;
    }

    /*
     * the type field is abused to carry the port number
     * so that we can have multiple servers on one host
     */
    ina.sin_family = AF_INET;
    if ( CA_V48 (CA_PROTOCOL_VERSION,minorVersion) ) {
        if ( pMsg->m_cid != INADDR_BROADCAST ) {
            /*
             * Leave address in network byte order (m_cid has not been 
             * converted to the local byte order)
             */
            ina.sin_addr.s_addr = pMsg->m_cid;
        }
        else {
            ina.sin_addr = pnet_addr->sin_addr;
        }
        ina.sin_port = htons (pMsg->m_dataType);
    }
    else if ( CA_V45 (CA_PROTOCOL_VERSION,minorVersion) ) {
        ina.sin_port = htons (pMsg->m_dataType);
        ina.sin_addr = pnet_addr->sin_addr;
    }
    else {
        ina.sin_port = htons (piiu->pcas->ca_server_port);
        ina.sin_addr = pnet_addr->sin_addr;
    }

    /*
     * Ignore duplicate search replies
     */
    if ( chan->piiu->compareIfTCP (*chan, *pnet_addr) ) {
        piiu->pcas->unlock ();
        return;
    }

    allocpiiu = constructTCPIIU (piiu->pcas, &ina, minorVersion);
    if ( ! allocpiiu ) {
        piiu->pcas->unlock ();
        return;
    }

    /*
     * If this is the first channel to be
     * added to this niiu then communicate
     * the client's name to the server. 
     * (CA V4.1 or higher)
     */
    if ( ellCount ( &allocpiiu->chidList ) == 0 ) {
        allocpiiu->userNameSetMsg ();
        allocpiiu->hostNameSetMsg ();
    }

    piiu->pcas->notifySearchResponse ( chan->retrySeqNo );

    /*
     * Assume that we have access once connected briefly
     * until the server is able to tell us the correct
     * state for backwards compatibility.
     *
     * Their access rights call back does not get
     * called for the first time until the information 
     * arrives however.
     */
    chan->ar.read_access = TRUE;
    chan->ar.write_access = TRUE;

    /*
     * remove it from the broadcast niiu
     */
    chan->piiu->removeFromChanList ( *chan );

    /*
     * chan->piiu must be correctly set prior to issuing the
     * claim request
     *
     * add to the beginning of the list until we
     * have sent the claim message (after which we
     * move it to the end of the list)
     *
     * claim pending flag is set here
     */
    allocpiiu->addToChanList ( *chan );

    if ( CA_V42 ( CA_PROTOCOL_VERSION, minorVersion ) ) {
        chan->searchReplySetUp ( pMsg->m_cid, USHRT_MAX, 0 );
    }
    else {
        chan->searchReplySetUp ( pMsg->m_cid, pMsg->m_dataType, pMsg->m_count );
    }
    
    chan->claimMsg ( allocpiiu );
    cacRingBufferWriteFlush ( &allocpiiu->send );
    
    piiu->pcas->unlock ();
}


/*
 * beacon_action ()
 */
LOCAL void beacon_action ( udpiiu * piiu, 
	caHdr *pMsg, const struct sockaddr_in *pnet_addr)
{
    struct sockaddr_in ina;

    piiu->pcas->lock ();
        
    /* 
     * this allows a fan-out server to potentially
     * insert the true address of the CA server 
     *
     * old servers:
     *   1) set this field to one of the ip addresses of the host _or_
     *   2) set this field to htonl(INADDR_ANY)
     * new servers:
     *      always set this field to htonl(INADDR_ANY)
     *
     * clients always assume that if this
     * field is set to something that isnt htonl(INADDR_ANY)
     * then it is the overriding IP address of the server.
     */
    ina.sin_family = AF_INET;
    if (pMsg->m_available != htonl(INADDR_ANY)) {
        ina.sin_addr.s_addr = pMsg->m_available;
    }
    else {
        ina.sin_addr = pnet_addr->sin_addr;
    }
    if (pMsg->m_count != 0) {
        ina.sin_port = htons (pMsg->m_count);
    }
    else {
        /*
         * old servers dont supply this and the
         * default port must be assumed
         */
        ina.sin_port = htons (piiu->pcas->ca_server_port);
    }
    piiu->pcas->beaconNotify (ina);

    piiu->pcas->unlock ();

    return;
}

/*
 * repeater_ack_action ()
 */
LOCAL void repeater_ack_action (udpiiu * piiu, 
	caHdr * /* pMsg */,  const struct sockaddr_in * /* pnet_addr */)
{
    piiu->pcas->repeaterSubscribeConfirmNotify ();
}

/*
 * not_here_resp_action ()
 */
LOCAL void not_here_resp_action (udpiiu * /* piiu */, caHdr * /* pMsg */,  const struct sockaddr_in * /* pnet_addr */)
{
    return;
}

/*
 * udp_exception_resp_action ()
 */
LOCAL void udp_exception_resp_action ( udpiiu *piiu, 
	caHdr *pMsg, const struct sockaddr_in *pnet_addr )
{
    caHdr *pReqMsg = pMsg + 1;
    char name[64];

    ipAddrToA ( pnet_addr, name, sizeof ( name ) );

    if ( pMsg->m_postsize > sizeof (caHdr) ){
        errlogPrintf ( "error condition \"%s\" detected by %s with context \"%s\"\n", 
            ca_message ( ntohl ( pMsg->m_available ) ), 
            name, reinterpret_cast <char *> ( pReqMsg + 1 ) );
    }
    else{
        errlogPrintf ( "error condition \"%s\" detected by %s\n", 
            ca_message ( ntohl ( pMsg->m_available ) ), name );
    }

    return;
}


/*
 * UDP protocol jump table
 */
LOCAL const pProtoStubUDP udpJumpTableCAC[] = 
{
    udp_noop_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    search_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    udp_exception_resp_action,
    bad_udp_resp_action,
    beacon_action,
    not_here_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    repeater_ack_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action,
    bad_udp_resp_action
};

/*
 * post_udp_msg ()
 */
int udpiiu::post_msg (const struct sockaddr_in *pnet_addr, 
              char *pInBuf, unsigned long blockSize)
{
    caHdr *pCurMsg;

    while ( blockSize ) {
        unsigned long size;

        if ( blockSize < sizeof (*pCurMsg) ) {
            return ECA_TOLARGE;
        }

        pCurMsg = reinterpret_cast <caHdr *> (pInBuf);

        /* 
         * fix endian of bytes 
         */
        pCurMsg->m_postsize = ntohs (pCurMsg->m_postsize);
        pCurMsg->m_cmmd = ntohs (pCurMsg->m_cmmd);
        pCurMsg->m_dataType = ntohs (pCurMsg->m_dataType);
        pCurMsg->m_count = ntohs (pCurMsg->m_count);

#if 0
        printf ("UDP Cmd=%3d Type=%3d Count=%4d Size=%4d",
            pCurMsg->m_cmmd,
            pCurMsg->m_dataType,
            pCurMsg->m_count,
            pCurMsg->m_postsize);
        printf (" Avail=%8x Cid=%6d\n",
            pCurMsg->m_available,
            pCurMsg->m_cid);
#endif

        size = pCurMsg->m_postsize + sizeof (*pCurMsg);

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
        if ( pCurMsg->m_cmmd>=NELEMENTS (udpJumpTableCAC) ) {
            pStub = bad_udp_resp_action;
        }
        else {
            pStub = udpJumpTableCAC [pCurMsg->m_cmmd];
        }
        (*pStub) (this, pCurMsg, pnet_addr);

        blockSize -= size;
        pInBuf += size;;
    }
    return ECA_NORMAL;
}

void udpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        strncpy ( pBuf, "<disconnected>", bufLength );
        pBuf[bufLength - 1u] = '\0';
    }
}

bool udpiiu::ca_v42_ok () const
{
    return false;
}

bool udpiiu::ca_v41_ok () const
{
    return false;
}

bool udpiiu::compareIfTCP (nciu &, const sockaddr_in &) const
{
    return false;
}

/*
 * Add chan to iiu and guarantee that
 * one chan on the B cast iiu list is pointed to by
 * ca_pEndOfBCastList 
 */
void udpiiu::addToChanList ( nciu &chan )
{
    this->pcas->lock ();

    /*
     * add to the beginning of the list so that search requests for
     * this channel will be sent first (since the retry count is zero)
     */
    if ( ellCount ( &this->chidList ) == 0 ) {
        this->pcas->endOfBCastList = tsDLIterBD <nciu> ( &chan );
    }
    /*
     * add to the front of the list so that
     * search requests for new channels will be sent first
     */
    chan.retry = 0u;
    this->chidList.push ( chan );
    chan.piiu = this;

    this->pcas->unlock ();
}

void udpiiu::removeFromChanList ( nciu &chan )
{
    tsDLIterBD <nciu> iter ( &chan );

    this->pcas->lock ();
    if ( chan.piiu->pcas->endOfBCastList == iter ) {
        if ( iter.itemBefore () != tsDLIterBD<nciu>::eol () ) {
            chan.piiu->pcas->endOfBCastList = iter.itemBefore ();
        }
        else {
            chan.piiu->pcas->endOfBCastList = 
                tsDLIterBD<nciu> ( chan.piiu->chidList.last () );
        }
    }
    chan.piiu->chidList.remove ( chan );
    chan.piiu = NULL;
    this->pcas->unlock ();
}

void udpiiu::disconnect ( nciu & /* chan */ )
{
    // NOOP
}

/*
 *  udpiiu::pushDatagramMsg ()
 */ 
int udpiiu::pushDatagramMsg (const caHdr *pMsg, const void *pExt, ca_uint16_t extsize)
{
    unsigned long   msgsize;
    ca_uint16_t     allignedExtSize;
    caHdr           *pbufmsg;

    allignedExtSize = CA_MESSAGE_ALIGN (extsize);
    msgsize = sizeof (caHdr) + allignedExtSize;


    /* fail out if max message size exceeded */
    if ( msgsize >= sizeof (this->xmitBuf)-7 ) {
        return ECA_TOLARGE;
    }

    semMutexMustTake (this->xmitBufLock);

    if ( msgsize + this->nBytesInXmitBuf > sizeof ( this->xmitBuf ) ) {
        semMutexGive (this->xmitBufLock);
        return ECA_TOLARGE;
    }

    pbufmsg = (caHdr *) &this->xmitBuf[this->nBytesInXmitBuf];
    *pbufmsg = *pMsg;
    memcpy (pbufmsg+1, pExt, extsize);
    if ( extsize != allignedExtSize ) {
        char *pDest = (char *) (pbufmsg+1);
        memset (pDest + extsize, '\0', allignedExtSize - extsize);
    }
    pbufmsg->m_postsize = htons (allignedExtSize);
    this->nBytesInXmitBuf += msgsize;

    semMutexGive (this->xmitBufLock);

    return ECA_NORMAL;
}

//
// udpiiu::flush ()
//
void udpiiu::flush ()
{
    osiSockAddrNode  *pNode;

    semMutexMustTake (this->xmitBufLock);

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

                    ipAddrToA ( &pNode->addr.ia, buf, sizeof ( buf ) );

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

    semMutexGive ( this->xmitBufLock );
}

int udpiiu::pushStreamMsg ( const caHdr * /* pmsg */, 
	const void * /* pext */, bool /* blockingOk */ )
{
    ca_printf ("in pushStreamMsg () for a udp iiu?\n");
    return ECA_DISCONNCHID;
}

SOCKET udpiiu::getSock () const
{
    return this->sock;
}