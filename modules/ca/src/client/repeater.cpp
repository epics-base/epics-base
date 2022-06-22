/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *
 * REPEATER.cpp
 *
 * CA broadcast repeater
 *
 * Author:  Jeff Hill
 * Date:    3-27-90
 *
 *  PURPOSE:
 *  Broadcasts fan out over the LAN, but old IP kernels do not allow
 *  two processes on the same machine to get the same broadcast
 *  (and modern IP kernels do not allow two processes on the same machine
 *  to receive the same unicast).
 *
 *  This code fans out UDP messages sent to the CA repeater port
 *  to all CA client processes that have subscribed.
 *
 */

/*
 * It would be preferable to avoid using the repeater on multicast enhanced
 * IP kernels, but this is not going to work in all situations because
 * (according to Steven's TCP/IP illustrated volume I) if a broadcast is
 * received it goes to all sockets on the same port, but if a unicast is
 * received it goes to only one of the sockets on the same port (we can only
 * guess at which one it will be).
 *
 * I have observed this behavior under winsock II:
 * o only one of the sockets on the same port receives the message if we
 *   send to the loopback address
 * o both of the sockets on the same port receives the message if we send
 *   to the broadcast address
 */

/* verifyClients() Mechanism
 *
 * This is required because Solaris and HPUX have half baked versions
 * of sockets.
 *
 * As written, the repeater should be robust against situations where the
 * IP kernel doesn't implement UDP disconnect on receiving ICMP port
 * unreachable errors from the destination process. As I recall, this
 * change was required in the repeater code when we ported from sunos4 to
 * Solaris. To avoid unreasonable overhead, I decided at the time to check
 * the validity of all existing connections only when a new client
 * registers with the repeater (and not when fanning out each beacon
 * received).                                           -- Jeff
 */

#include <string>
#include <stdexcept>
#include <stdio.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "tsDLList.h"
#include "envDefs.h"
#include "tsFreeList.h"
#include "osiWireFormat.h"
#include "taskwd.h"
#include "errlog.h"

#include "iocinf.h"
#include "caProto.h"
#include "udpiiu.h"
#include "repeaterClient.h"
#include "addrList.h"
#include "osiSock.h"
#include "epicsSock.h"

#include "epicsBaseDebugLog.h"
#ifdef AF_INET6
#include "cantProceed.h" /* callocMustSucceed */
#endif

#ifdef NETDEBUG
#define SEND_FROM_REPEATER(a,b,c,d) send(a,b,c,d)
#else
// Use the send() function with builtin debug print
#define SEND_FROM_REPEATER(a,b,c,d) epicsSocket46Send(a,b,c,d)
#endif

/*
 *  these can be external since there is only one instance
 *  per machine so we don't care about reentrancy
 */
static tsDLList < repeaterClient > client_list;

static const unsigned short PORT_ANY = 0u;

/*
 * makeSocket()
 */

#ifdef NETDEBUG
#define makeSocket(a,b,c,d) makeSocketFL(__FILE__, __LINE__, a,b,c,d)
static int makeSocketFL (const char *filename, int lineno,
                         int family, unsigned short port, bool reuseAddr, SOCKET * pSock )
 {
    SOCKET sock = epicsSocket46CreateFL( filename, lineno, family, SOCK_DGRAM, 0 );
#else
static int makeSocket ( int family, unsigned short port, bool reuseAddr, SOCKET * pSock )
 {
    SOCKET sock = epicsSocket46Create ( family, SOCK_DGRAM, 0 );
#endif

    if ( sock == INVALID_SOCKET ) {
        *pSock = sock;
        return SOCKERRNO;
    }

    /*
     * no need to bind if unconstrained
     */
    if ( port != PORT_ANY ) {
        int status;
#ifdef NETDEBUG
        status = epicsSocket46BindLocalPortFL(filename, lineno,
                                              sock, family, port);
#else
        status = epicsSocket46BindLocalPort(sock, family, port);
#endif
        if ( status < 0 ) {
            status = SOCKERRNO;
            epicsSocketDestroy ( sock );
            return status;
        }
        if ( reuseAddr ) {
            epicsSocketEnableAddressReuseDuringTimeWaitState ( sock );
        }
    }
    *pSock = sock;
    return 0;
}

repeaterClient::repeaterClient ( const osiSockAddr46 &fromIn46 ) :
    from46 ( fromIn46 ), sock ( INVALID_SOCKET )
{
#ifdef DEBUG
    unsigned port = ntohs ( from.ia.sin_port );
    debugPrintf ( ( "new client %u\n", port ) );
#endif
}

bool repeaterClient::connect ()
{
    int status;

    if ( int sockerrno = makeSocket ( this->from46.sa.sa_family,
                                      PORT_ANY, false, & this->sock ) ) {
        char sockErrBuf[64];
        epicsSocketConvertErrorToString (
            sockErrBuf, sizeof ( sockErrBuf ), sockerrno );
        fprintf ( stderr, "%s: no client sock because \"%s\"\n",
                __FILE__, sockErrBuf );
        return false;
    }

    status = epicsSocket46Connect ( this->sock, this->from46.sa.sa_family,
                                    &this->from46 );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (
            sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf ( stderr, "%s: unable to connect client sock because \"%s\"\n",
            __FILE__, sockErrBuf );
        return false;
    }

    return true;
}

bool repeaterClient::sendConfirm ()
{
    int status;

    caHdr confirm;
    memset ( (char *) &confirm, '\0', sizeof (confirm) );
    AlignedWireRef < epicsUInt16 > ( confirm.m_cmmd ) = REPEATER_CONFIRM;
    if ( this->from46.sa.sa_family == AF_INET ) {
        confirm.m_available = this->from46.ia.sin_addr.s_addr;
    }
    status = SEND_FROM_REPEATER ( this->sock,  (char *) &confirm,
                                 sizeof (confirm), 0 );
    if ( status >= 0 ) {
        assert ( status == sizeof ( confirm ) );
        return true;
    }
    else if ( SOCKERRNO == SOCK_ECONNREFUSED ) {
        return false;
    }
    else {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (
            sockErrBuf, sizeof ( sockErrBuf ) );
        debugPrintf ( ( "CA Repeater: confirm req err was \"%s\"\n", sockErrBuf) );
        return false;
    }
}

bool repeaterClient::sendMessage ( const void *pBuf, unsigned bufSize )
{
    int status;

    status = SEND_FROM_REPEATER ( this->sock, (char *) pBuf, bufSize, 0 );
    if ( status >= 0 ) {
        assert ( static_cast <unsigned> ( status ) == bufSize );
#ifdef DEBUG
        epicsUInt16 port = ntohs ( this->from46.ia.sin_port );
        debugPrintf ( ("Sent to %u\n", port ) );
#endif
        return true;
    }
    else {
        int errnoCpy = SOCKERRNO;
        if ( errnoCpy == SOCK_ECONNREFUSED ) {
#ifdef DEBUG
            epicsUInt16 port = ntohs ( this->from46.ia.sin_port );
            debugPrintf ( ("Client refused message %u\n", port ) );
#endif
        }
        else {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            debugPrintf ( ( "CA Repeater: UDP send err was \"%s\"\n", sockErrBuf) );
        }
        return false;
    }
}

repeaterClient::~repeaterClient ()
{
    if ( this->sock != INVALID_SOCKET ) {
        epicsSocketDestroy ( this->sock );
    }
#ifdef DEBUG
    epicsUInt16 port = ntohs ( this->from46.ia.sin_port );
    debugPrintf ( ( "Deleted client %u\n", port ) );
#endif
}

void repeaterClient::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void * repeaterClient::operator new ( size_t size,
    tsFreeList < repeaterClient, 0x20 > & freeList )
{
    return freeList.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
void repeaterClient::operator delete ( void *pCadaver,
    tsFreeList < repeaterClient, 0x20 > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

unsigned short repeaterClient::get_family () const
{
    return this->from46.sa.sa_family;
}

inline unsigned short repeaterClient::port () const
{
    return ntohs ( this->from46.ia.sin_port );
}

inline bool repeaterClient::identicalAddress ( const osiSockAddr46 &fromIn )
{
    if (sockIPsAreIdentical46 ( &fromIn, &this->from46 ) ) {
        return true;
    }
    return false;
}

inline bool repeaterClient::identicalPort ( const osiSockAddr46 &fromIn )
{
    if ( sockPortAreIdentical46 ( &fromIn, &this->from46 ) ) {
        return true;
    }
    return false;
}

bool repeaterClient::verify ()
{
    SOCKET tmpSock;
    int sockerrno = makeSocket ( AF_INET, this->port (), false, & tmpSock );

    if ( sockerrno == SOCK_EADDRINUSE ) {
        // Normal result, client using port
        return true;
    }

    if ( sockerrno == 0 ) {
        // Client went away, released port
        epicsSocketDestroy ( tmpSock );
    }
    else {
        char sockErrBuf[64];
        epicsSocketConvertErrorToString (
            sockErrBuf, sizeof ( sockErrBuf ), sockerrno );
        fprintf ( stderr, "CA Repeater: Bind test error \"%s\"\n",
            sockErrBuf );
    }
#ifdef AF_INET6
    sockerrno = makeSocket ( AF_INET6, this->port (), false, & tmpSock );
    if ( sockerrno == SOCK_EADDRINUSE ) {
        // Normal result, client using port
        return true;
    }
    if ( sockerrno == 0 ) {
        // Client went away, released port
        epicsSocketDestroy ( tmpSock );
    }
    else {
        char sockErrBuf[64];
        epicsSocketConvertErrorToString (
            sockErrBuf, sizeof ( sockErrBuf ), sockerrno );
        fprintf ( stderr, "CA Repeater: Bind6 test error \"%s\"\n",
            sockErrBuf );
    }
#endif
#ifdef NETDEBUG
    epicsBaseDebugLog ("repeaterClient::verify false port=%u\n",
                       this->port () );
#endif

    return false;
}


/*
 * verifyClients()
 */
static void verifyClients ( tsFreeList < repeaterClient, 0x20 > & freeList )
{
    static tsDLList < repeaterClient > theClients;
    repeaterClient *pclient;

    while ( ( pclient = client_list.get () ) ) {
        if ( pclient->verify () ) {
            theClients.add ( *pclient );
        }
        else {
            pclient->~repeaterClient ();
            freeList.release ( pclient );
        }
    }
    client_list.add ( theClients );
}

/*
 * fanOut()
 */
static void fanOut ( const osiSockAddr46 & from46, const void * pMsg,
    unsigned msgSize, tsFreeList < repeaterClient, 0x20 > & freeList )
{
    static tsDLList < repeaterClient > theClients;
    repeaterClient *pclient;

    while ( ( pclient = client_list.get () ) ) {
        theClients.add ( *pclient );
        /* Don't reflect back to sender */
        if ( pclient->identicalAddress ( from46 ) ) {
            continue;
        }
        if ( pclient->get_family() == AF_INET && from46.sa.sa_family != AF_INET ) {
#ifdef NETDEBUG
            epicsBaseDebugLog ("repeater: fanOut ignored: pclient=%p get_family=%d from46.sa.sa_family=%d\n",
                               pclient, pclient->get_family(), from46.sa.sa_family);
#endif

            continue;
        }
        if ( ! pclient->sendMessage ( pMsg, msgSize ) ) {
            if ( ! pclient->verify () ) {
                theClients.remove ( *pclient );
                pclient->~repeaterClient ();
                freeList.release ( pclient );
            }
        }
    }

    client_list.add ( theClients );
}

/*
 * register_new_client()
 */
static void register_new_client ( osiSockAddr46 & from46,
            tsFreeList < repeaterClient, 0x20 > & freeList )
{
    bool newClient = false;
    int status;

#ifdef NETDEBUG
    {
      char buf[64];
      sockAddrToDottedIP(&from46.sa, buf, sizeof(buf));
      epicsBaseDebugLog ("repeater: register_new_client='%s'\n", buf );
    }
#endif
    if ( ! epicsSocket46IsAF_INETorAF_INET6 ( from46.sa.sa_family ) ) {
        return;
    }

    /*
     * the repeater and its clients must be on the same host
     * If we receive a registration send to 1.2.3.4, that
     * may be OK, if, and only if, we have 1.2.3.4 as an interface
     * Try to bind to this interface to check this.
     * This is IPv4 only, check this in preparation for IPv6
     */
    if ( ( from46.sa.sa_family == AF_INET ) &&
         ( INADDR_LOOPBACK != ntohl ( from46.ia.sin_addr.s_addr ) ) ) {
        static SOCKET testSock = INVALID_SOCKET;
        static bool init = false;

        if ( ! init ) {
            SOCKET sock;
            if ( int sockerrno = makeSocket ( from46.sa.sa_family, PORT_ANY, true, & sock ) ) {
                char sockErrBuf[64];
                epicsSocketConvertErrorToString (
                    sockErrBuf, sizeof ( sockErrBuf ), sockerrno );
                fprintf ( stderr, "%s: Unable to create repeater bind test socket because \"%s\"\n",
                    __FILE__, sockErrBuf );
            }
            else {
                testSock = sock;
            }
            init = true;
        }

        /*
         * Unfortunately on 3.13 beta 11 and before the
         * repeater would not always allow the loopback address
         * as a local client address so current clients alternate
         * between the address of the first non-loopback interface
         * found and the loopback address when subscribing with
         * the CA repeater until all CA repeaters have been updated
         * to current code.
         */
        if ( testSock != INVALID_SOCKET ) {
            osiSockAddr46 addr46;

            addr46 = from46;
            addr46.ia.sin_port = PORT_ANY;

            /* we can only bind to a local address */
            status = bind ( testSock, &addr46.sa, sizeof ( addr46.ia ) );
            if ( status ) {
                return;
            }
        }
        else {
            return;
        }
    }

    tsDLIter < repeaterClient > pclient = client_list.firstIter ();
    while ( pclient.valid () ) {
        if ( pclient->identicalPort ( from46 ) ) {
            break;
        }
        pclient++;
    }

    repeaterClient *pNewClient;
    if ( pclient.valid () ) {
        pNewClient = pclient.pointer ();
    }
    else {
        pNewClient = new ( freeList ) repeaterClient ( from46 );
        if ( ! pNewClient ) {
            fprintf ( stderr, "%s: no memory for new client\n", __FILE__ );
            return;
        }
        if ( ! pNewClient->connect () ) {
            pNewClient->~repeaterClient ();
            freeList.release ( pNewClient );
            return;
        }
        client_list.add ( *pNewClient );
        newClient = true;
    }

#ifdef NETDEBUG
    epicsBaseDebugLog ("repeater: register_new_client pNewClient=%p\n", pNewClient );
#endif
    if ( ! pNewClient->sendConfirm () ) {
        client_list.remove ( *pNewClient );
        pNewClient->~repeaterClient ();
        freeList.release ( pNewClient );
#       ifdef DEBUG
            epicsUInt16 port = ntohs ( from.ia.sin_port );
            debugPrintf ( ( "Deleted repeater client=%u (error while sending ack)\n",
                        port ) );
#       endif
    }

    /*
     * send a noop message to all other clients so that we don't
     * accumulate sockets when there are no beacons
     */
    caHdr noop;
    memset ( (char *) &noop, '\0', sizeof ( noop ) );
    AlignedWireRef < epicsUInt16 > ( noop.m_cmmd ) = CA_PROTO_VERSION;
    fanOut ( from46, &noop, sizeof ( noop ), freeList );

    if ( newClient ) {
        /*
         * For HPUX and Solaris we need to verify that the clients
         * have not gone away - because an ICMP error return does not
         * get through to send(), which returns no error code.
         *
         * This is done each time that a new client is created.
         * See also the note in the file header.
         *
         * This is done here in order to avoid deleting a client
         * prior to sending its confirm message.
         */
        verifyClients ( freeList );
    }
}


/*
 *  ca_repeater ()
 */
void ca_repeater ()
{
    tsFreeList < repeaterClient, 0x20 > freeList;
    int size;
    SOCKET sock4;
    osiSockAddr46 from46;
    unsigned short port;
    char * pBuf;
#ifdef AF_INET6
    SOCKET sock6;
    struct osiSockPollfd *pPollFds = NULL;
    unsigned numPollFds = 0;
    unsigned searchDestList_count = 0;
#endif

    pBuf = new char [MAX_UDP_RECV];

    {
        bool success = osiSockAttach();
        assert ( success );
    }

    port = envGetInetPortConfigParam ( & EPICS_CA_REPEATER_PORT,
                                       static_cast <unsigned short> (CA_REPEATER_PORT) );

    if ( int sockerrno = makeSocket ( AF_INET, port, true, & sock4 ) ) {
        /*
         * test for server was already started
         */
        if ( sockerrno == SOCK_EADDRINUSE ) {
            osiSockRelease ();
            debugPrintf ( ( "CA Repeater: exiting because a repeater is already running\n" ) );
            delete [] pBuf;
            return;
        }
        char sockErrBuf[64];
        epicsSocketConvertErrorToString (
            sockErrBuf, sizeof ( sockErrBuf ), sockerrno );
        fprintf ( stderr, "%s: Unable to create repeater sock4 because \"%s\" - fatal\n",
            __FILE__, sockErrBuf );
        osiSockRelease ();
        delete [] pBuf;
        return;
    }
#ifdef AF_INET6
    /* Create a socket for registrations via [::1] */
    if ( int sockerrno = makeSocket ( AF_INET6, PORT_ANY, true, & sock6 ) ) {
      char sockErrBuf[64];
      epicsSocketConvertErrorToString ( sockErrBuf,
                                        sizeof ( sockErrBuf ),
                                        sockerrno );
      fprintf ( stderr, "%s: Unable to create sock6 because \"%s\" - fatal\n",
                __FILE__, sockErrBuf );
      return;
    } else {
        osiSockAddr46 addr46;
        memset ( (char *) &addr46, 0 , sizeof (addr46) );
        addr46.in6.sin6_family = AF_INET6;
        addr46.in6.sin6_addr = in6addr_loopback;
        addr46.in6.sin6_port = htons ( port );
        (void)epicsSocket46Bind(sock6, &addr46);
    }
#endif

    {
        ELLLIST casBeaconAddrList = ELLLIST_INIT;
        ELLLIST casMergeAddrList = ELLLIST_INIT;
#ifdef AF_INET6
        configureChannelAccessAddressList ( & casMergeAddrList, sock4, port);
#endif

        /*
         * collect user specified beacon address list;
         * check BEACON_ADDR_LIST list first; if no result, take CA_ADDR_LIST
        */
        if(!addAddrToChannelAccessAddressList(&casMergeAddrList,&EPICS_CAS_BEACON_ADDR_LIST,port,0)) {
            addAddrToChannelAccessAddressList(&casMergeAddrList,&EPICS_CA_ADDR_LIST,port,0);
        }

        /* First clean up */
        removeDuplicateAddresses(&casBeaconAddrList, &casMergeAddrList , 0);

#ifdef AF_INET6
        searchDestList_count = (unsigned)casBeaconAddrList.count;
        pPollFds = (osiSockPollfd*)callocMustSucceed(searchDestList_count + 2, /* sock4 sock6 */
                                              sizeof(struct osiSockPollfd),
                                              "ca_repeater");

        /* this.socket must be added to the polling list */
        pPollFds[numPollFds].fd = sock6;
        pPollFds[numPollFds].events = POLLIN;
        numPollFds++;
        pPollFds[numPollFds].fd = sock4;
        pPollFds[numPollFds].events = POLLIN;
        numPollFds++;
#endif
        osiSockAddrNode *pNode;
        for(pNode = (osiSockAddrNode*)ellFirst(&casBeaconAddrList);
            pNode;
            pNode = (osiSockAddrNode*)ellNext(&pNode->node))
        {

#ifdef NETDEBUG
            {
                char buf[64];
                sockAddrToDottedIP(&pNode->addr.sa, buf, sizeof(buf));
                epicsBaseDebugLog ("repeater: node='%s'\n", buf );
            }
#endif
#ifdef IP_ADD_MEMBERSHIP
            /*
             * join UDP socket to any multicast groups
             */
            if(pNode->addr.ia.sin_family==AF_INET) {
                epicsUInt32 top = ntohl(pNode->addr.ia.sin_addr.s_addr)>>24;
                if(top>=224 && top<=239) {

                    /* This is a multi-cast address */
                    struct ip_mreq mreq;

                    memset(&mreq, 0, sizeof(mreq));
                    mreq.imr_multiaddr = pNode->addr.ia.sin_addr;
                    mreq.imr_interface.s_addr = INADDR_ANY;

                    if (setsockopt(sock4, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        (char *) &mreq, sizeof(mreq)) != 0) {
                        char name[64];
                        char sockErrBuf[64];
                        epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
                        sockAddrToDottedIP (&pNode->addr.sa, name, sizeof(name));
                        errlogPrintf("caR: Socket mcast join to %s failed: %s\n", name, sockErrBuf );
                    }
                }
            }
#endif
#ifdef AF_INET6
            if (pNode->addr.sa.sa_family == AF_INET6) {
                osiSockAddr46 addr46 = pNode->addr; // struct copy
                unsigned int interfaceIndex = (unsigned int)addr46.in6.sin6_scope_id;
                /*
                 * The %en0 will become the scope id, which IPV6_MULTICAST_IF needs
                 * BSD/non-Linux system need a own socket per scope_id
                 */
                SOCKET socket46 = epicsSocket46Create ( AF_INET6, SOCK_DGRAM, IPPROTO_UDP );
                pPollFds[numPollFds].fd = socket46;
                pPollFds[numPollFds].events = POLLIN;
                numPollFds++;
                epicsSocket46optIPv6MultiCast(socket46, interfaceIndex);
                addr46.in6.sin6_port = htons ( port );
                (void)epicsSocket46Bind(socket46, &addr46);
            }
#endif
        }
    }

    debugPrintf ( ( "CA Repeater: Attached and initialized\n" ) );
#ifdef NETDEBUG
    epicsBaseDebugLog ("repeater pPollFds=%p searchDestList_count=%u numPollFds=%u\n",
                   pPollFds, searchDestList_count, numPollFds);
#endif

    while ( true ) {
        SOCKET sock = sock4;
#ifdef AF_INET6
        pollagain:
        if ( pPollFds && numPollFds >= 1 ) {
            int pollres = osiSockPoll ( pPollFds, numPollFds, -1 );
            if ( pollres < 0 ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString (sockErrBuf, sizeof ( sockErrBuf ) );
                epicsBaseDebugLog("repeater pollres =%d: %s\n",
                              pollres, sockErrBuf);
                goto pollagain;
            }
            for ( unsigned idx = 0; idx < numPollFds; idx++ ) {
#ifdef NETDEBUGXX
                epicsBaseDebugLog ("repeater idx=%u socket=%d revents=0x%x\n",
                               idx, (int)pPollFds[idx].fd, pPollFds[idx].revents);
#endif
                if (pPollFds[idx].revents) {
                    sock = pPollFds[idx].fd;
                }
            }
        }
#endif
        size = epicsSocket46Recvfrom ( sock, pBuf, MAX_UDP_RECV, 0,
                                       &from46 );
        if ( size < 0 ) {
            int errnoCpy = SOCKERRNO;
            // Avoid spurious ECONNREFUSED bug in linux
            if ( errnoCpy == SOCK_ECONNREFUSED ) {
                continue;
            }
            // Avoid ECONNRESET from connected socket in windows
            if ( errnoCpy == SOCK_ECONNRESET ) {
                continue;
            }
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (
                sockErrBuf, sizeof ( sockErrBuf ) );
            fprintf ( stderr, "CA Repeater: unexpected UDP recv err: %s\n",
                sockErrBuf );
            continue;
        }
        if ( ! epicsSocket46IsAF_INETorAF_INET6 ( from46.sa.sa_family ) ) {
#ifdef NETDEBUG
            {
                char buf[64];
                sockAddrToDottedIP(&from46.sa, buf, sizeof(buf));
                epicsPrintf ("%s:%d: CAC: CA Repeater recvfromAddr='%s' ignored\n",
                             __FILE__, __LINE__, buf);
             }
#endif
            continue;
        }

        caHdr * pMsg = ( caHdr * ) pBuf;

        /*
         * both zero length message and a registration message
         * will register a new client
         */
        if ( ( (size_t) size) >= sizeof (*pMsg) ) {
            if ( AlignedWireRef < epicsUInt16 > ( pMsg->m_cmmd ) == REPEATER_REGISTER ) {
                register_new_client ( from46, freeList );

                /*
                 * strip register client message
                 */
                pMsg++;
                size -= sizeof ( *pMsg );
                if ( size==0 ) {
                    continue;
                }
            }
            else if ( AlignedWireRef < epicsUInt16 > ( pMsg->m_cmmd ) == CA_PROTO_RSRV_IS_UP ) {
#ifdef AF_INET6
                if (from46.sa.sa_family == AF_INET6 ) {
                    size_t needed_size = sizeof(ca_msg_IPv6_RSRV_IS_UP_type);
                    if ( (size_t)size >= needed_size ) {
                        ca_ext_IPv6_RSRV_IS_UP_type * pMsgIPv6 = (ca_ext_IPv6_RSRV_IS_UP_type *)&pBuf[sizeof(caHdr)];
                        if (pMsgIPv6->m_typ_magic[0] == 'I' &&
                            pMsgIPv6->m_typ_magic[1] == 'P' &&
                            pMsgIPv6->m_typ_magic[2] == 'v' &&
                            pMsgIPv6->m_typ_magic[3] == '6' &&
                            ntohl(pMsgIPv6->m_size) == sizeof(*pMsgIPv6)) {
                            /* Copy the "from" address into the data, so that we can forward it */
                            memcpy(&pMsgIPv6->m_s6_addr,
                                   &from46.in6.sin6_addr.s6_addr,
                                   sizeof(pMsgIPv6->m_s6_addr));
                            pMsgIPv6->m_sin6_scope_id = from46.in6.sin6_scope_id;
                        } else {
#ifdef NETDEBUG
                            epicsBaseDebugLog("CA_PROTO_RSRV_IS_UP size=%u magic='%c%c%c%c'\n",
                                              (unsigned)ntohl(pMsgIPv6->m_size),
                                              isprint(pMsgIPv6->m_typ_magic[0]) ? pMsgIPv6->m_typ_magic[0] : '?',
                                              isprint(pMsgIPv6->m_typ_magic[1]) ? pMsgIPv6->m_typ_magic[1] : '?',
                                              isprint(pMsgIPv6->m_typ_magic[2]) ? pMsgIPv6->m_typ_magic[2] : '?',
                                              isprint(pMsgIPv6->m_typ_magic[3]) ? pMsgIPv6->m_typ_magic[3] : '?');
#endif
                        }
                    }
                } else
#endif
                if ( ( pMsg->m_available == 0u ) && ( from46.sa.sa_family == AF_INET ) ) {
                    /* Shorten the message to what clients expect for IPv4 beacons */
                    pMsg->m_postsize = 0;
                    pMsg->m_available = from46.ia.sin_addr.s_addr;
                    fanOut ( from46, pMsg, sizeof ( caHdr ) , freeList );
                    continue;
                }
            }
        }
        else if ( size == 0 ) {
            register_new_client ( from46, freeList );
            continue;
        }

        fanOut ( from46, pMsg, size, freeList );
    }
}

/*
 * caRepeaterThread ()
 */
extern "C" void caRepeaterThread ( void * /* pDummy */ )
{
    taskwdInsert ( epicsThreadGetIdSelf(), NULL, NULL );
    ca_repeater ();
}


