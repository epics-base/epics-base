/*************************************************************************\
* Copyright (c) 2016 Michael Davidsaver
* Copyright (c) 2015 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Jeffrey O. Hill
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "addrList.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsSignal.h"
#include "epicsStdio.h"
#include "epicsTime.h"
#include "errlog.h"
#include "freeList.h"
#include "osiPoolStatus.h"
#include "osiSock.h"
#include "taskwd.h"
#include "cantProceed.h"

#include "epicsExport.h"

#define epicsExportSharedSymbols
#include "dbChannel.h"
#include "dbCommon.h"
#include "dbEvent.h"
#include "db_field_log.h"
#include "dbServer.h"
#include "rsrv.h"

#define GLBLSOURCE
#include "server.h"

epicsThreadPrivateId rsrvCurrentClient;

/*
 *
 *  req_server()
 *
 *  CA server task
 *
 *  Waits for connections at the CA port and spawns a task to
 *  handle each of them
 *
 */
static void req_server (void *pParm)
{
    rsrv_iface_config *conf = pParm;
    SOCKET IOC_sock;

    taskwdInsert ( epicsThreadGetIdSelf (), NULL, NULL );

    IOC_sock = conf->tcp;

    /* listen and accept new connections */
    if ( listen ( IOC_sock, 20 ) < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString (
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "CAS: Listen error %s\n",
            sockErrBuf );
        epicsSocketDestroy (IOC_sock);
        epicsThreadSuspendSelf ();
    }

    epicsEventSignal(castcp_startStopEvent);

    while (TRUE) {
        SOCKET clientSock;
        struct sockaddr     sockAddr;
        osiSocklen_t        addLen = sizeof(sockAddr);

        while (castcp_ctl == ctlPause) {
            epicsThreadSleep(0.1);
        }

        clientSock = epicsSocketAccept ( IOC_sock, &sockAddr, &addLen );
        if ( clientSock == INVALID_SOCKET ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf("CAS: Client accept error was \"%s\"\n",
                sockErrBuf );
            epicsThreadSleep(15.0);
            continue;
        }
        else {
            epicsThreadId id;
            struct client *pClient;

            /* socket passed in is closed if unsuccessful here */
            pClient = create_tcp_client ( clientSock );
            if ( ! pClient ) {
                epicsThreadSleep ( 15.0 );
                continue;
            }

            LOCK_CLIENTQ;
            ellAdd ( &clientQ, &pClient->node );
            UNLOCK_CLIENTQ;

            id = epicsThreadCreate ( "CAS-client", epicsThreadPriorityCAServerLow,
                    epicsThreadGetStackSize ( epicsThreadStackBig ),
                    camsgtask, pClient );
            if ( id == 0 ) {
                LOCK_CLIENTQ;
                ellDelete ( &clientQ, &pClient->node );
                UNLOCK_CLIENTQ;
                destroy_tcp_client ( pClient );
                errlogPrintf ( "CAS: task creation for new client failed\n" );
                epicsThreadSleep ( 15.0 );
                continue;
            }
        }
    }
}

static
int tryBind(SOCKET sock, const osiSockAddr* addr, const char *name)
{
    if(bind(sock, (struct sockaddr *) &addr->sa, sizeof(*addr))<0) {
        char sockErrBuf[64];
        if(SOCKERRNO!=SOCK_EADDRINUSE)
        {
            epicsSocketConvertErrnoToString (
                        sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAS: %s bind error: \"%s\"\n",
                           name, sockErrBuf );
            epicsThreadSuspendSelf ();
        }
        return -1;
    } else
        return 0;
}

/* need to collect a set of TCP sockets, one for each interface,
 * which are bound to the same TCP port number.
 * Needed to avoid the complications and confusion of different TCP
 * ports for each interface (name server and beacon sender would need
 * to know this).
 */
static
SOCKET* rsrv_grab_tcp(unsigned short *port)
{
    SOCKET *socks;
    osiSockAddr scratch;
    unsigned i;

    socks = mallocMustSucceed(ellCount(&casIntfAddrList)*sizeof(*socks), "rsrv_grab_tcp");
    for(i=0; i<ellCount(&casIntfAddrList); i++)
        socks[i] = INVALID_SOCKET;

    /* start with preferred port */
    memset(&scratch, 0, sizeof(scratch));
    scratch.ia.sin_family = AF_INET;
    scratch.ia.sin_port = htons(*port);

    while(ellCount(&casIntfAddrList)>0) {
        ELLNODE *cur, *next;
        unsigned ok = 1;

        for(i=0; i<ellCount(&casIntfAddrList); i++) {
            if(socks[i] != INVALID_SOCKET)
                epicsSocketDestroy(socks[i]);
            socks[i] = INVALID_SOCKET;
        }

        for (i=0, cur=ellFirst(&casIntfAddrList), next = cur ? ellNext(cur) : NULL;
             cur;
             i++, cur=next, next=next ? ellNext(next) : NULL)
        {
            SOCKET tcpsock;
            osiSockAddr ifaceAddr = ((osiSockAddrNode *)cur)->addr;

            scratch.ia.sin_addr = ifaceAddr.ia.sin_addr;

            tcpsock = socks[i] = epicsSocketCreate (AF_INET, SOCK_STREAM, 0);
            if(tcpsock==INVALID_SOCKET)
                cantProceed("rsrv ran out of sockets during initialization");

            epicsSocketEnableAddressReuseDuringTimeWaitState ( tcpsock );

            if(bind(tcpsock, &scratch.sa, sizeof(scratch))==0) {
                if(scratch.ia.sin_port==0) {
                    /* use first socket to pick a random port */
                    osiSocklen_t alen = sizeof(ifaceAddr);
                    assert(i==0);
                    if(getsockname(tcpsock, &ifaceAddr.sa, &alen)) {
                        char sockErrBuf[64];
                        epicsSocketConvertErrnoToString (
                            sockErrBuf, sizeof ( sockErrBuf ) );
                        errlogPrintf ( "CAS: getsockname error was \"%s\"\n",
                            sockErrBuf );
                        epicsThreadSuspendSelf ();
                        ok = 0;
                        break;
                    }
                    scratch.ia.sin_port = ifaceAddr.ia.sin_port;
                    assert(scratch.ia.sin_port!=0);
                }
            } else {
                int errcode = SOCKERRNO;
                /* bind fails.  React harshly to unexpected errors to avoid an infinite loop */
                if(errcode==SOCK_EADDRNOTAVAIL) {
                    /* this is not a bind()able address. */
                    int j;
                    char name[40];
                    ipAddrToDottedIP(&scratch.ia, name, sizeof(name));
                    printf("Skipping %s which is not an interface address\n", name);

                    for(j=0; j<=i; j++) {
                        epicsSocketDestroy(socks[j]);
                        socks[j] = INVALID_SOCKET;
                    }

                    ellDelete(&casIntfAddrList, cur);
                    free(cur);
                    ok = 0;
                    break;
                }
                /* if SOCK_EADDRINUSE or SOCK_EACCES try again with a different
                 * port number, otherwise fail hard.
                 */
                if (errcode != SOCK_EADDRINUSE &&
                    errcode != SOCK_EACCES) {
                    char name[40];
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString (
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    ipAddrToDottedIP(&scratch.ia, name, sizeof(name));
                    cantProceed( "CAS: Socket bind %s error was %s\n",
                        name, sockErrBuf );
                }
                ok = 0;
                break;
            }
        }

        if (ok) {
            assert(scratch.ia.sin_port!=0);
            *port = ntohs(scratch.ia.sin_port);

            break;
        } else {

            for(i=0; i<ellCount(&casIntfAddrList); i++) {
                /* cleanup any ports actually bound */
                if(socks[i]!=INVALID_SOCKET) {
                    epicsSocketDestroy(socks[i]);
                    socks[i] = INVALID_SOCKET;
                }
            }

            scratch.ia.sin_port=0; /* next iteration starts with a random port */
        }
    }

    if(ellCount(&casIntfAddrList)==0)
        cantProceed("Error: RSRV has empty interface list\n"
                    "The CA server can't function without binding to"
                    " at least one network interface.\n");

    return socks;
}

static
void rsrv_build_addr_lists(void)
{
    int autobeaconlist = 1;

    /* the UDP ports are known at this point, but the TCP port is not */
    assert(ca_beacon_port!=0);
    assert(ca_udp_port!=0);

    envGetBoolConfigParam(&EPICS_CAS_AUTO_BEACON_ADDR_LIST, &autobeaconlist);

    ellInit ( &casIntfAddrList );
    ellInit ( &beaconAddrList );
    ellInit ( &casMCastAddrList );

    /* Setup socket for sending server beacons.
     * Also used for NIC introspection
     */

    beaconSocket = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    if (beaconSocket==INVALID_SOCKET)
        cantProceed("socket allocation failed during address list expansion");

    {
        int intTrue = 1;
        if (setsockopt (beaconSocket, SOL_SOCKET, SO_BROADCAST,
                        (char *)&intTrue, sizeof(intTrue))<0) {
            cantProceed("CAS: online socket set up error\n");
        }
#ifdef IP_ADD_MEMBERSHIP
        {
            int flag = 1;
            if (setsockopt(beaconSocket, IPPROTO_IP, IP_MULTICAST_LOOP,
                           (char *)&flag, sizeof(flag))<0) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString (
                            sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf("rsrv: failed to set mcast loopback\n");
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
        if ( setsockopt(beaconSocket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl))) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString (
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf("rsrv: failed to set mcast ttl %d\n", ttl);
        }
    }
#endif
    }

    /* populate the interface address list (default is empty) */
    {
        ELLLIST temp = ELLLIST_INIT;
        /* use the first parameter which is set. */
        addAddrToChannelAccessAddressList ( &temp, &EPICS_CAS_INTF_ADDR_LIST, ca_udp_port, 0 );

        removeDuplicateAddresses(&casIntfAddrList, &temp, 0);
    }

    /* Process the interface address list
     * Move multicast addresses to casMCastAddrList
     * Populate beacon address list (if autobeaconlist and iface list not-empty).
     */
    {
        int foundWildcard = 0, doautobeacon = autobeaconlist;

        osiSockAddrNode *pNode, *pNext;
        for(pNode = (osiSockAddrNode*)ellFirst(&casIntfAddrList),
            pNext = pNode ? (osiSockAddrNode*)ellNext(&pNode->node) : NULL;
            pNode;
            pNode = pNext,
            pNext = pNext ? (osiSockAddrNode*)ellNext(&pNext->node) : NULL)
        {
            osiSockAddr match;
            epicsUInt32 top = ntohl(pNode->addr.ia.sin_addr.s_addr)>>24;

            if(pNode->addr.ia.sin_family==AF_INET && pNode->addr.ia.sin_addr.s_addr==htonl(INADDR_ANY))
            {
                foundWildcard = 1;

            } else if(pNode->addr.ia.sin_family==AF_INET && top>=224 && top<=239) {
                /* This is a multi-cast address */
                ellDelete(&casIntfAddrList, &pNode->node);
                ellAdd(&casMCastAddrList, &pNode->node);
                continue;
            }

            if(!doautobeacon)
                continue;
            /* when given a specific interface address, auto populate with the
             * corresponding broadcast address.
             */

            autobeaconlist = 0; /* prevent later population from wildcard */

            memset(&match, 0, sizeof(match));
            match.ia.sin_family = AF_INET;
            match.ia.sin_addr.s_addr = pNode->addr.ia.sin_addr.s_addr;
            match.ia.sin_port = htons(ca_beacon_port);

            osiSockDiscoverBroadcastAddresses(&beaconAddrList, beaconSocket, &match);
        }

        if (foundWildcard && ellCount(&casIntfAddrList) != 1) {
            cantProceed("CAS interface address list can not contain 0.0.0.0 and other interface addresses.\n");
        }
    }

    if (ellCount(&casIntfAddrList) == 0) {
        /* default to wildcard 0.0.0.0 when interface address list is empty */
        osiSockAddrNode *pNode = (osiSockAddrNode *) callocMustSucceed( 1, sizeof(*pNode), "rsrv_init" );
        pNode->addr.ia.sin_family = AF_INET;
        pNode->addr.ia.sin_addr.s_addr = htonl ( INADDR_ANY );
        pNode->addr.ia.sin_port = 0;
        ellAdd ( &casIntfAddrList, &pNode->node );
    }

    {
        ELLLIST temp = ELLLIST_INIT;
        osiSockAddrNode *pNode;

        ellConcat(&temp, &beaconAddrList);

        /* collect user specified beacon address list
         * prefer EPICS_CAS_BEACON_ADDR_LIST, fallback to EPICS_CA_ADDR_LIST
         */
        addAddrToChannelAccessAddressList ( &temp, &EPICS_CAS_BEACON_ADDR_LIST, ca_beacon_port, 0 );

        if (autobeaconlist) {
            /* auto populate with all broadcast addresses.
             * Note that autobeaconlist is zeroed above if an interface
             * address list is provided.
             */
            osiSockAddr match;
            memset(&match, 0, sizeof(match));
            match.ia.sin_family = AF_INET;
            match.ia.sin_addr.s_addr = htonl(INADDR_ANY);
            match.ia.sin_port = htons(ca_beacon_port);

            osiSockDiscoverBroadcastAddresses(&temp, beaconSocket, &match);
        }

        /* set the port for any automatically discovered destinations. */
        for(pNode = (osiSockAddrNode*)ellFirst(&temp);
            pNode;
            pNode = (osiSockAddrNode*)ellNext(&pNode->node))
        {
            if(pNode->addr.ia.sin_port==0)
                pNode->addr.ia.sin_port = htons(ca_beacon_port);
        }

        removeDuplicateAddresses(&beaconAddrList, &temp, 0);
    }

    if (ellCount(&beaconAddrList)==0)
        fprintf(stderr, "Warning: RSRV has empty beacon address list\n");

    {
        osiSockAddrNode *node;
        ELLLIST temp = ELLLIST_INIT,
                temp2= ELLLIST_INIT;
        size_t idx = 0;

        addAddrToChannelAccessAddressList ( &temp, &EPICS_CAS_IGNORE_ADDR_LIST, 0, 0 );
        removeDuplicateAddresses(&temp2, &temp, 0);

        /* Keep the list of addresses to ignore in an array on the assumption that
         * it is short enough that using a hash table would be slower.
         * 0.0.0.0 indicates end of list
         */
        casIgnoreAddrs = callocMustSucceed(1+ellCount(&temp2), sizeof(casIgnoreAddrs[0]), "casIgnoreAddrs");

        while((node=(osiSockAddrNode*)ellGet(&temp2))!=NULL)
        {
            casIgnoreAddrs[idx++] = node->addr.ia.sin_addr.s_addr;
            free(node);
        }
        casIgnoreAddrs[idx] = 0;
    }
}

static dbServer rsrv_server = {
    ELLNODE_INIT,
    "rsrv",
    casr,
    casStatsFetch,
    casClientInitiatingCurrentThread
};

/*
 * rsrv_init ()
 */
int rsrv_init (void)
{
    long maxBytesAsALong;
    long status;
    SOCKET *socks;
    int autoMaxBytes;

    clientQlock = epicsMutexMustCreate();

    freeListInitPvt ( &rsrvClientFreeList, sizeof(struct client), 8 );
    freeListInitPvt ( &rsrvChanFreeList, sizeof(struct channel_in_use), 512 );
    freeListInitPvt ( &rsrvEventFreeList, sizeof(struct event_ext), 512 );
    freeListInitPvt ( &rsrvSmallBufFreeListTCP, MAX_TCP, 16 );
    initializePutNotifyFreeList ();

    epicsSignalInstallSigPipeIgnore ();

    rsrvCurrentClient = epicsThreadPrivateCreate ();

    dbRegisterServer(&rsrv_server);

    if ( envGetConfigParamPtr ( &EPICS_CAS_SERVER_PORT ) ) {
        ca_server_port = envGetInetPortConfigParam ( &EPICS_CAS_SERVER_PORT,
            (unsigned short) CA_SERVER_PORT );
    }
    else {
        ca_server_port = envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT,
            (unsigned short) CA_SERVER_PORT );
    }
    ca_udp_port = ca_server_port;

    if (envGetConfigParamPtr(&EPICS_CAS_BEACON_PORT)) {
        ca_beacon_port = envGetInetPortConfigParam (&EPICS_CAS_BEACON_PORT,
            (unsigned short) CA_REPEATER_PORT );
    }
    else {
        ca_beacon_port = envGetInetPortConfigParam (&EPICS_CA_REPEATER_PORT,
            (unsigned short) CA_REPEATER_PORT );
    }

    status =  envGetLongConfigParam ( &EPICS_CA_MAX_ARRAY_BYTES, &maxBytesAsALong );
    if ( status || maxBytesAsALong < 0 ) {
        errlogPrintf ( "CAS: EPICS_CA_MAX_ARRAY_BYTES was not a positive integer\n" );
        rsrvSizeofLargeBufTCP = MAX_TCP;
    }
    else {
        /* allow room for the protocol header so that they get the array size they requested */
        static const unsigned headerSize = sizeof ( caHdr ) + 2 * sizeof ( ca_uint32_t );
        ca_uint32_t maxBytes = ( unsigned ) maxBytesAsALong;
        if ( maxBytes < 0xffffffff - headerSize ) {
            maxBytes += headerSize;
        }
        else {
            maxBytes = 0xffffffff;
        }
        if ( maxBytes < MAX_TCP ) {
            errlogPrintf ( "CAS: EPICS_CA_MAX_ARRAY_BYTES was rounded up to %u\n", MAX_TCP );
            rsrvSizeofLargeBufTCP = MAX_TCP;
        }
        else {
            rsrvSizeofLargeBufTCP = maxBytes;
        }
    }

    if(envGetBoolConfigParam(&EPICS_CA_AUTO_ARRAY_BYTES, &autoMaxBytes))
        autoMaxBytes = 1;

    if (!autoMaxBytes)
        freeListInitPvt ( &rsrvLargeBufFreeListTCP, rsrvSizeofLargeBufTCP, 1 );
    else
        rsrvLargeBufFreeListTCP = NULL;
    pCaBucket = bucketCreate(CAS_HASH_TABLE_SIZE);
    if (!pCaBucket)
        cantProceed("RSRV failed to allocate ID lookup table\n");

    rsrv_build_addr_lists();

    castcp_startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    casudp_startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    beacon_startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    castcp_ctl = ctlPause;

    /* Thread priorites
     * Now starting per interface
     *  TCP Listener: epicsThreadPriorityCAServerLow-2
     *  Name receiver: epicsThreadPriorityCAServerLow-4
     * Now starting global
     *  Beacon sender: epicsThreadPriorityCAServerLow-3
     * Started later per TCP client
     *  TCP receiver: epicsThreadPriorityCAServerLow
     *  TCP sender : epicsThreadPriorityCAServerLow-1
     */
    {
        unsigned i;
        threadPrios[0] = epicsThreadPriorityCAServerLow;

        for(i=1; i<NELEMENTS(threadPrios); i++)
        {
            if(epicsThreadBooleanStatusSuccess!=epicsThreadHighestPriorityLevelBelow(
                        threadPrios[i-1], &threadPrios[i]))
            {
                /* on failure use the lowest known */
                threadPrios[i] = threadPrios[i-1];
            }
        }
    }

    {
        unsigned short sport = ca_server_port;
        socks = rsrv_grab_tcp(&sport);

        if ( sport != ca_server_port ) {
            ca_server_port = sport;
            errlogPrintf ( "cas warning: Configured TCP port was unavailable.\n");
            errlogPrintf ( "cas warning: Using dynamically assigned TCP port %hu,\n",
                ca_server_port );
            errlogPrintf ( "cas warning: but now two or more servers share the same UDP port.\n");
            errlogPrintf ( "cas warning: Depending on your IP kernel this server may not be\n" );
            errlogPrintf ( "cas warning: reachable with UDP unicast (a host's IP in EPICS_CA_ADDR_LIST)\n" );
        }
    }

    /* start servers (TCP and UDP(s) for each interface.
     */
    {
        int havesometcp = 0;
        ELLNODE *cur;
        int i;

        for (i=0, cur=ellFirst(&casIntfAddrList); cur; i++, cur=ellNext(cur))
        {
            char ifaceName[40];
            rsrv_iface_config *conf;

            conf = callocMustSucceed(1, sizeof(*conf), "rsrv_init");

            conf->tcpAddr = ((osiSockAddrNode *)cur)->addr;
            conf->tcpAddr.ia.sin_port = htons(ca_server_port);
            conf->tcp = socks[i];
            socks[i] = INVALID_SOCKET;

            ipAddrToDottedIP (&conf->tcpAddr.ia, ifaceName, sizeof(ifaceName));

            conf->udp = conf->udpbcast = INVALID_SOCKET;

            /* create and bind UDP name receiver socket(s) */

            conf->udp = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
            if(conf->udp==INVALID_SOCKET)
                cantProceed("rsrv_init ran out of udp sockets");

            conf->udpAddr = conf->tcpAddr;
            conf->udpAddr.ia.sin_port = htons(ca_udp_port);

            epicsSocketEnableAddressUseForDatagramFanout ( conf->udp );

            if(tryBind(conf->udp, &conf->udpAddr, "UDP unicast socket"))
                goto cleanup;

#ifdef IP_ADD_MEMBERSHIP
            /* join UDP socket to any multicast groups */
            {
                osiSockAddrNode *pNode;

                for(pNode = (osiSockAddrNode*)ellFirst(&casMCastAddrList);
                    pNode;
                    pNode = (osiSockAddrNode*)ellNext(&pNode->node))
                {
                    struct ip_mreq mreq;

                    memset(&mreq, 0, sizeof(mreq));
                    mreq.imr_multiaddr = pNode->addr.ia.sin_addr;
                    mreq.imr_interface.s_addr = conf->udpAddr.ia.sin_addr.s_addr;

                    if (setsockopt(conf->udp, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        (char *) &mreq, sizeof(mreq))!=0) {
                        struct sockaddr_in temp;
                        char name[40];
                        char sockErrBuf[64];
                        temp.sin_family = AF_INET;
                        temp.sin_addr = mreq.imr_multiaddr;
                        temp.sin_port = conf->udpAddr.ia.sin_port;
                        epicsSocketConvertErrnoToString (
                            sockErrBuf, sizeof ( sockErrBuf ) );
                        ipAddrToDottedIP (&temp, name, sizeof(name));
                        fprintf(stderr, "CAS: Socket mcast join %s to %s failed with \"%s\"\n",
                            ifaceName, name, sockErrBuf );
                    }
                }
            }
#else
            if(ellCount(&casMCastAddrList)){
                fprintf(stderr, "IPv4 Multicast name lookup not supported by this target\n");
            }
#endif

#if !(defined(_WIN32) || defined(__CYGWIN__))
            /* An oddness of BSD sockets (not winsock) is that binding to
             * INADDR_ANY will receive unicast and broadcast, but binding to
             * a specific interface address receives only unicast.  The trick
             * is to bind a second socket to the interface broadcast address,
             * which will then receive only broadcasts.
             */
            if(conf->udpAddr.ia.sin_addr.s_addr!=htonl(INADDR_ANY)) {
                /* find interface broadcast address */
                ELLLIST bcastList = ELLLIST_INIT;
                osiSockAddrNode *pNode;

                osiSockDiscoverBroadcastAddresses (&bcastList,
                                                   conf->udp, &conf->udpAddr); // match addr

                if(ellCount(&bcastList)==0) {
                    fprintf(stderr, "Warning: Can't find broadcast address of interface %s\n"
                                    "         Name lookup may not work on this interface\n", ifaceName);
                } else {
                    if(ellCount(&bcastList)>1 && conf->udpAddr.ia.sin_addr.s_addr!=htonl(INADDR_ANY))
                        printf("Interface %s has more than one broadcast address?\n", ifaceName);

                    pNode = (osiSockAddrNode*)ellFirst(&bcastList);

                    conf->udpbcast = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
                    if(conf->udpbcast==INVALID_SOCKET)
                        cantProceed("rsrv_init ran out of udp sockets for bcast");

                    epicsSocketEnableAddressUseForDatagramFanout ( conf->udpbcast );

                    conf->udpbcastAddr = conf->udpAddr;
                    conf->udpbcastAddr.ia.sin_addr.s_addr = pNode->addr.ia.sin_addr.s_addr;

                    if(tryBind(conf->udpbcast, &conf->udpbcastAddr, "UDP Socket bcast"))
                        goto cleanup;
                }

                ellFree(&bcastList);
            }

#endif /* !(defined(_WIN32) || defined(__CYGWIN__)) */

            ellAdd(&servers, &conf->node);

            /* have all sockets, time to start some threads */

            epicsThreadMustCreate("CAS-TCP", threadPrios[2],
                    epicsThreadGetStackSize(epicsThreadStackMedium),
                    &req_server, conf);

            epicsEventMustWait(castcp_startStopEvent);

            epicsThreadMustCreate("CAS-UDP", threadPrios[4],
                    epicsThreadGetStackSize(epicsThreadStackMedium),
                    &cast_server, conf);

            epicsEventMustWait(casudp_startStopEvent);

#if !(defined(_WIN32) || defined(__CYGWIN__))
            if(conf->udpbcast != INVALID_SOCKET) {
                conf->startbcast = 1;

                epicsThreadMustCreate("CAS-UDP2", threadPrios[4],
                        epicsThreadGetStackSize(epicsThreadStackMedium),
                        &cast_server, conf);

                epicsEventMustWait(casudp_startStopEvent);

                conf->startbcast = 0;
            }
#endif /* !(defined(_WIN32) || defined(__CYGWIN__)) */

            havesometcp = 1;
            continue;
        cleanup:
            epicsSocketDestroy(conf->tcp);
            if(conf->udp!=INVALID_SOCKET) epicsSocketDestroy(conf->udp);
            if(conf->udpbcast!=INVALID_SOCKET) epicsSocketDestroy(conf->udpbcast);
            free(conf);
        }

        if(!havesometcp)
            cantProceed("CAS: No TCP server started\n");
    }

    /* servers list is considered read-only from this point */

    epicsThreadMustCreate("CAS-beacon", threadPrios[3],
            epicsThreadGetStackSize(epicsThreadStackSmall),
            &rsrv_online_notify_task, NULL);

    epicsEventMustWait(beacon_startStopEvent);

    return RSRV_OK;
}

int rsrv_run (void)
{
    castcp_ctl = ctlRun;
    casudp_ctl = ctlRun;
    beacon_ctl = ctlRun;

    return RSRV_OK;
}

int rsrv_pause (void)
{
    beacon_ctl = ctlPause;
    casudp_ctl = ctlPause;
    castcp_ctl = ctlPause;

    return RSRV_OK;
}

static unsigned countChanListBytes (
    struct client *client, ELLLIST * pList )
{
    struct channel_in_use   * pciu;
    unsigned                bytes_reserved = 0;

    epicsMutexMustLock ( client->chanListLock );
    pciu = ( struct channel_in_use * ) pList->node.next;
    while ( pciu ) {
        bytes_reserved += sizeof(struct channel_in_use);
        bytes_reserved += sizeof(struct event_ext)*ellCount( &pciu->eventq );
        bytes_reserved += rsrvSizeOfPutNotify ( pciu->pPutNotify );
        pciu = ( struct channel_in_use * ) ellNext( &pciu->node );
    }
    epicsMutexUnlock ( client->chanListLock );

    return bytes_reserved;
}

static void showChanList (
    struct client * client, unsigned level, ELLLIST * pList )
{
    struct channel_in_use * pciu;
    epicsMutexMustLock ( client->chanListLock );
    pciu = (struct channel_in_use *) pList->node.next;
    while ( pciu ){
        dbChannelShow ( pciu->dbch, level, 8 );
        if ( level >= 1u )
            printf( "%12s# on eventq=%d, access=%c%c\n", "",
                ellCount ( &pciu->eventq ),
                asCheckGet ( pciu->asClientPVT ) ? 'r': '-',
                rsrvCheckPut ( pciu ) ? 'w': '-' );
        pciu = ( struct channel_in_use * ) ellNext ( &pciu->node );
    }
    epicsMutexUnlock ( client->chanListLock );
}

/*
 *  log_one_client ()
 */
static void log_one_client (struct client *client, unsigned level)
{
    char clientIP[40];
    int n;

    ipAddrToDottedIP (&client->addr, clientIP, sizeof(clientIP));

    if ( client->proto == IPPROTO_UDP ) {
        printf ( "\tLast name requested by %s:\n",
            clientIP );
    }
    else if ( client->proto == IPPROTO_TCP ) {
        printf ( "    TCP client at %s '%s':\n",
            clientIP,
            client->pHostName ? client->pHostName : "" );
    }
    else {
        printf ( "    Unknown client at %s '%s':\n",
            clientIP,
            client->pHostName ? client->pHostName : "" );
    }

    n = ellCount(&client->chanList) + ellCount(&client->chanPendingUpdateARList);
    printf ( "\tUser '%s', V%u.%u, Priority = %u, %d Channel%s\n",
        client->pUserName ? client->pUserName : "",
        CA_MAJOR_PROTOCOL_REVISION,
        client->minor_version_number,
        client->priority,
        n, n == 1 ? "" : "s" );

    if ( level >= 3u ) {
        double         send_delay;
        double         recv_delay;
        char           *state[] = {"up", "down"};
        epicsTimeStamp current;

        epicsTimeGetCurrent(&current);
        send_delay = epicsTimeDiffInSeconds(&current,&client->time_at_last_send);
        recv_delay = epicsTimeDiffInSeconds(&current,&client->time_at_last_recv);

        printf ("\tTask Id = %p, Socket FD = %d\n",
            (void *) client->tid, client->sock);
        printf(
        "\t%.2f secs since last send, %.2f secs since last receive\n",
            send_delay, recv_delay);
        printf(
        "\tUnprocessed request bytes = %u, Undelivered response bytes = %u\n",
            client->recv.cnt - client->recv.stk,
            client->send.stk );
        printf(
        "\tState = %s%s%s\n",
            state[client->disconnect?1:0],
            client->send.type == mbtLargeTCP ? " jumbo-send-buf" : "",
            client->recv.type == mbtLargeTCP ? " jumbo-recv-buf" : "");
    }

    if ( level >= 1u ) {
        showChanList ( client, level - 1u, & client->chanList );
        showChanList ( client, level - 1u, & client->chanPendingUpdateARList );
    }

    if ( level >= 4u ) {
        unsigned bytes_reserved = sizeof(struct client);

        bytes_reserved += countChanListBytes (
                            client, & client->chanList );
        bytes_reserved += countChanListBytes (
                        client, & client->chanPendingUpdateARList );
        printf( "\t%d bytes allocated\n", bytes_reserved);
        printf( "\tSend Lock:\n\t    ");
        epicsMutexShow(client->lock,1);
        printf( "\tPut Notify Lock:\n\t    ");
        epicsMutexShow (client->putNotifyLock,1);
        printf( "\tAddress Queue Lock:\n\t    ");
        epicsMutexShow (client->chanListLock,1);
        printf( "\tEvent Queue Lock:\n\t    ");
        epicsMutexShow (client->eventqLock,1);
        printf( "\tBlock Semaphore:\n\t    ");
        epicsEventShow (client->blockSem,1);
    }
}

/*
 *  casr()
 */
void casr (unsigned level)
{
    size_t bytes_reserved;
    int n;

    if ( ! clientQlock ) {
        return;
    }

    printf ("Channel Access Server V%s\n",
        CA_VERSION_STRING ( CA_MINOR_PROTOCOL_REVISION ) );

    LOCK_CLIENTQ
    n = ellCount ( &clientQ );
    if (n == 0) {
        printf("No clients connected.\n");
    }
    else if (level == 0) {
        printf("%d client%s connected.\n",
            n, n == 1 ? "" : "s" );
    }
    else {
        struct client *client = (struct client *) ellFirst ( &clientQ );

        printf("%d client%s connected:\n",
            n, n == 1 ? "" : "s" );
        while (client) {
            log_one_client(client, level - 1);
            client = (struct client *) ellNext(&client->node);
        }
    }
    UNLOCK_CLIENTQ

    if (level>=1) {
        rsrv_iface_config *iface = (rsrv_iface_config *) ellFirst ( &servers );
        while (iface) {
            char    buf[40];

            ipAddrToDottedIP (&iface->tcpAddr.ia, buf, sizeof(buf));
            printf("CAS-TCP server on %s with\n", buf);

            ipAddrToDottedIP (&iface->udpAddr.ia, buf, sizeof(buf));
#if defined(_WIN32)
            printf("    CAS-UDP name server on %s\n", buf);
            if (level >= 2)
                log_one_client(iface->client, level - 2);
#else
            if (iface->udpbcast==INVALID_SOCKET) {
                printf("    CAS-UDP name server on %s\n", buf);
                if (level >= 2)
                    log_one_client(iface->client, level - 2);
            }
            else {
                printf("    CAS-UDP unicast name server on %s\n", buf);
                if (level >= 2)
                    log_one_client(iface->client, level - 2);
                ipAddrToDottedIP (&iface->udpbcastAddr.ia, buf, sizeof(buf));
                printf("    CAS-UDP broadcast name server on %s\n", buf);
                if (level >= 2)
                    log_one_client(iface->bclient, level - 2);
            }
#endif

            iface = (rsrv_iface_config *) ellNext(&iface->node);
        }
    }

    if (level>=1) {
        osiSockAddrNode * pAddr;
        char buf[40];
        int n = ellCount(&casMCastAddrList);

        if (n) {
            printf("Monitoring %d multicast address%s:\n",
                n, n == 1 ? "" : "es");
            for(pAddr = (osiSockAddrNode*)ellFirst(&casMCastAddrList);
                pAddr;
                pAddr = (osiSockAddrNode*)ellNext(&pAddr->node))
            {
                ipAddrToDottedIP (&pAddr->addr.ia, buf, sizeof(buf));
                printf("    %s\n", buf);
            }
        }

        n = ellCount(&beaconAddrList);
        printf("Sending CAS-beacons to %d address%s:\n",
            n, n == 1 ? "" : "es");
        for(pAddr = (osiSockAddrNode*)ellFirst(&beaconAddrList);
            pAddr;
            pAddr = (osiSockAddrNode*)ellNext(&pAddr->node))
        {
            ipAddrToDottedIP (&pAddr->addr.ia, buf, sizeof(buf));
            printf("    %s\n", buf);
        }

        if (casIgnoreAddrs[0]) { /* 0 indicates end of array */
            size_t i;
            printf("Ignoring UDP messages from address%s\n",
                   n == 1 ? "" : "es");
            for(i=0; casIgnoreAddrs[i]; i++)
            {
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = casIgnoreAddrs[i];
                addr.sin_port = 0;
                ipAddrToDottedIP(&addr, buf, sizeof(buf));
                printf("    %s\n", buf);
            }
        }
    }

    if (level>=4u) {
        bytes_reserved = 0u;
        bytes_reserved += sizeof (struct client) *
                    freeListItemsAvail (rsrvClientFreeList);
        bytes_reserved += sizeof (struct channel_in_use) *
                    freeListItemsAvail (rsrvChanFreeList);
        bytes_reserved += sizeof(struct event_ext) *
                    freeListItemsAvail (rsrvEventFreeList);
        bytes_reserved += MAX_TCP *
                    freeListItemsAvail ( rsrvSmallBufFreeListTCP );
        if(rsrvLargeBufFreeListTCP) {
            bytes_reserved += rsrvSizeofLargeBufTCP *
                        freeListItemsAvail ( rsrvLargeBufFreeListTCP );
        }
        bytes_reserved += rsrvSizeOfPutNotify ( 0 ) *
                    freeListItemsAvail ( rsrvPutNotifyFreeList );
        printf( "Free-lists total %u bytes, comprising\n",
            (unsigned int) bytes_reserved);
        printf( "    %u client(s), %u channel(s), %u monitor event(s), %u putNotify(s)\n",
            (unsigned int) freeListItemsAvail ( rsrvClientFreeList ),
            (unsigned int) freeListItemsAvail ( rsrvChanFreeList ),
            (unsigned int) freeListItemsAvail ( rsrvEventFreeList ),
            (unsigned int) freeListItemsAvail ( rsrvPutNotifyFreeList ));
        printf( "    %u small (%u byte) buffers, %u jumbo (%u byte) buffers\n",
            (unsigned int) freeListItemsAvail ( rsrvSmallBufFreeListTCP ),
            MAX_TCP,
            (unsigned int)(rsrvLargeBufFreeListTCP ? freeListItemsAvail ( rsrvLargeBufFreeListTCP ) : -1),
            rsrvSizeofLargeBufTCP );
        printf( "Server resource id table:\n");
        LOCK_CLIENTQ;
        bucketShow (pCaBucket);
        UNLOCK_CLIENTQ;
    }
}

/*
 * destroy_client ()
 */
void destroy_client ( struct client *client )
{
    if ( ! client ) {
        return;
    }

    if ( client->tid != 0 ) {
        taskwdRemove ( client->tid );
    }

    if ( client->sock != INVALID_SOCKET ) {
        epicsSocketDestroy ( client->sock );
    }

    if ( client->proto == IPPROTO_TCP ) {
        if ( client->send.buf ) {
            if ( client->send.type == mbtSmallTCP ) {
                freeListFree ( rsrvSmallBufFreeListTCP,  client->send.buf );
            }
            else if ( client->send.type == mbtLargeTCP ) {
                if(rsrvLargeBufFreeListTCP)
                    freeListFree ( rsrvLargeBufFreeListTCP,  client->send.buf );
                else
                    free(client->send.buf);
            }
            else {
                errlogPrintf ( "CAS: Corrupt send buffer free list type code=%u during client cleanup?\n",
                    client->send.type );
            }
        }
        if ( client->recv.buf ) {
            if ( client->recv.type == mbtSmallTCP ) {
                freeListFree ( rsrvSmallBufFreeListTCP,  client->recv.buf );
            }
            else if ( client->recv.type == mbtLargeTCP ) {
                if(rsrvLargeBufFreeListTCP)
                    freeListFree ( rsrvLargeBufFreeListTCP,  client->recv.buf );
                else
                    free(client->recv.buf);
            }
            else {
                errlogPrintf ( "CAS: Corrupt recv buffer free list type code=%u during client cleanup?\n",
                    client->send.type );
            }
        }
    }
    else if ( client->proto == IPPROTO_UDP ) {
        if ( client->send.buf ) {
            free ( client->send.buf );
        }
        if ( client->recv.buf ) {
            free ( client->recv.buf );
        }
    }

    if ( client->eventqLock ) {
        epicsMutexDestroy ( client->eventqLock );
    }

    if ( client->chanListLock ) {
        epicsMutexDestroy ( client->chanListLock );
    }

    if ( client->putNotifyLock ) {
        epicsMutexDestroy ( client->putNotifyLock );
    }

    if ( client->lock ) {
        epicsMutexDestroy ( client->lock );
    }

    if ( client->blockSem ) {
        epicsEventDestroy ( client->blockSem );
    }

    if ( client->pUserName ) {
        free ( client->pUserName );
    }

    if ( client->pHostName ) {
        free ( client->pHostName );
    }

    freeListFree ( rsrvClientFreeList, client );
}

static void destroyAllChannels (
    struct client * client, ELLLIST * pList )
{
    if ( !client->chanListLock || !client->eventqLock ) {
        return;
    }

    while ( TRUE ) {
        struct event_ext        *pevext;
        int                     status;
        struct channel_in_use   *pciu;

        epicsMutexMustLock ( client->chanListLock );
        pciu = (struct channel_in_use *) ellGet ( pList );
        if(pciu) pciu->state = rsrvCS_shutdown;
        epicsMutexUnlock ( client->chanListLock );

        if ( ! pciu ) {
            break;
        }

        while ( TRUE ) {
            /*
            * AS state change could be using this list
            */
            epicsMutexMustLock ( client->eventqLock );
            pevext = (struct event_ext *) ellGet ( &pciu->eventq );
            epicsMutexUnlock ( client->eventqLock );

            if ( ! pevext ) {
                break;
            }

            if ( pevext->pdbev ) {
                db_cancel_event (pevext->pdbev);
            }
            freeListFree (rsrvEventFreeList, pevext);
        }
        rsrvFreePutNotify ( client, pciu->pPutNotify );
        LOCK_CLIENTQ;
        status = bucketRemoveItemUnsignedId ( pCaBucket, &pciu->sid);
        rsrvChannelCount--;
        UNLOCK_CLIENTQ;
        if ( status != S_bucket_success ) {
            errPrintf ( status, __FILE__, __LINE__,
                "Bad id=%d at close", pciu->sid);
        }
        status = asRemoveClient(&pciu->asClientPVT);
        if ( status && status != S_asLib_asNotActive ) {
            printf ( "bad asRemoveClient() status was %x \n", status );
            errPrintf ( status, __FILE__, __LINE__, "asRemoveClient" );
        }

        dbChannelDelete(pciu->dbch);
        freeListFree ( rsrvChanFreeList, pciu );
    }
}

void destroy_tcp_client ( struct client *client )
{
    int                     status;

    if ( CASDEBUG > 0 ) {
        errlogPrintf ( "CAS: Connection %d Terminated\n", client->sock );
    }

    if ( client->evuser ) {
        /*
         * turn off extra labor callbacks from the event thread
         */
        status = db_add_extra_labor_event ( client->evuser, NULL, NULL );
        assert ( ! status );

        /*
         * wait for extra labor in progress to comple
         */
        db_flush_extra_labor_event ( client->evuser );
    }

    destroyAllChannels ( client, & client->chanList );
    destroyAllChannels ( client, & client->chanPendingUpdateARList );

    if ( client->evuser ) {
        db_close_events (client->evuser);
    }

    destroy_client ( client );
}

/*
 * create_client ()
 */
struct client * create_client ( SOCKET sock, int proto )
{
    struct client *client;
    int           spaceAvailOnFreeList;
    size_t        spaceNeeded;

    /*
     * stop further use of server if memory becomes scarse
     */
    spaceAvailOnFreeList =     freeListItemsAvail ( rsrvClientFreeList ) > 0
                            && freeListItemsAvail ( rsrvSmallBufFreeListTCP ) > 0;
    spaceNeeded = sizeof (struct client) + MAX_TCP;
    if ( ! ( osiSufficentSpaceInPool(spaceNeeded) || spaceAvailOnFreeList ) ) {
        epicsSocketDestroy ( sock );
        epicsPrintf ("CAS: no space in pool for a new client (below max block thresh)\n");
        return NULL;
    }

    client = freeListCalloc ( rsrvClientFreeList );
    if ( ! client ) {
        epicsSocketDestroy ( sock );
        epicsPrintf ("CAS: no space in pool for a new client (alloc failed)\n");
        return NULL;
    }

    client->sock = sock;
    client->proto = proto;

    client->blockSem = epicsEventCreate ( epicsEventEmpty );
    client->lock = epicsMutexCreate();
    client->putNotifyLock = epicsMutexCreate();
    client->chanListLock = epicsMutexCreate();
    client->eventqLock = epicsMutexCreate();
    if ( ! client->blockSem || ! client->lock || ! client->putNotifyLock ||
        ! client->chanListLock || ! client->eventqLock ) {
        destroy_client ( client );
        return NULL;
    }

    client->pUserName = NULL;
    client->pHostName = NULL;
    ellInit ( & client->chanList );
    ellInit ( & client->chanPendingUpdateARList );
    ellInit ( & client->putNotifyQue );
    memset ( (char *)&client->addr, 0, sizeof (client->addr) );
    client->tid = 0;

    if ( proto == IPPROTO_TCP ) {
        client->send.buf = (char *) freeListCalloc ( rsrvSmallBufFreeListTCP );
        client->send.maxstk = MAX_TCP;
        client->send.type = mbtSmallTCP;
        client->recv.buf =  (char *) freeListCalloc ( rsrvSmallBufFreeListTCP );
        client->recv.maxstk = MAX_TCP;
        client->recv.type = mbtSmallTCP;
    }
    else if ( proto == IPPROTO_UDP ) {
        client->send.buf = malloc ( MAX_UDP_SEND );
        client->send.maxstk = MAX_UDP_SEND;
        client->send.type = mbtUDP;
        client->recv.buf = malloc ( MAX_UDP_RECV );
        client->recv.maxstk = MAX_UDP_RECV;
        client->recv.type = mbtUDP;
    }
    if ( ! client->send.buf || ! client->recv.buf ) {
        destroy_client ( client );
        return NULL;
    }
    client->send.stk = 0u;
    client->send.cnt = 0u;
    client->recv.stk = 0u;
    client->recv.cnt = 0u;
    client->evuser = NULL;
    client->priority = CA_PROTO_PRIORITY_MIN;
    client->disconnect = FALSE;
    epicsTimeGetCurrent ( &client->time_at_last_send );
    epicsTimeGetCurrent ( &client->time_at_last_recv );
    client->minor_version_number = CA_UKN_MINOR_VERSION;
    client->recvBytesToDrain = 0u;

    return client;
}

void casAttachThreadToClient ( struct client *pClient )
{
    epicsSignalInstallSigAlarmIgnore ();
    epicsSignalInstallSigPipeIgnore ();
    pClient->tid = epicsThreadGetIdSelf ();
    epicsThreadPrivateSet ( rsrvCurrentClient, pClient );
    taskwdInsert ( pClient->tid, NULL, NULL );
}

static
void casExpandBuffer ( struct message_buffer *buf, ca_uint32_t size, int sendbuf )
{
    char *newbuf = NULL;
    unsigned newsize;
    enum messageBufferType newtype;

    assert (size > MAX_TCP);

    if ( size <= buf->maxstk || buf->type == mbtUDP ) return;

    /* try to alloc new buffer */
    if (size <= MAX_TCP) {
        return; /* shouldn't happen */

    } else if(!rsrvLargeBufFreeListTCP) {
        // round up to multiple of 4K
        size = ((size-1)|0xfff)+1;

        if (buf->type==mbtLargeTCP)
            newbuf = realloc (buf->buf, size);
        else
            newbuf = malloc (size);
        newtype = mbtLargeTCP;
        newsize = size;

    } else if (size <= rsrvSizeofLargeBufTCP) {
        newbuf = freeListCalloc ( rsrvLargeBufFreeListTCP );
        newsize = rsrvSizeofLargeBufTCP;
        newtype = mbtLargeTCP;
    }

    if (newbuf) {
        /* copy existing buffer */
        if (sendbuf) {
            /* send buffer uses [0, stk) */
            if (!rsrvLargeBufFreeListTCP && buf->type==mbtLargeTCP) {
                /* realloc already copied */
            } else {
                memcpy ( newbuf, buf->buf, buf->stk );
            }
        } else {
            /* recv buffer uses [stk, cnt) */
            unsigned used;
            assert ( buf->cnt >= buf->stk );
            used = buf->cnt - buf->stk;

            /* buf->buf may be the same as newbuf if realloc() used */
            memmove ( newbuf, &buf->buf[buf->stk], used );

            buf->cnt = used;
            buf->stk = 0;

        }

        /* free existing buffer */
        if(buf->type==mbtSmallTCP) {
            freeListFree ( rsrvSmallBufFreeListTCP,  buf->buf );
        } else if(buf->type==mbtLargeTCP) {
            freeListFree ( rsrvLargeBufFreeListTCP,  buf->buf );
        } else {
            /* realloc() already free()'d if necessary */
        }

        buf->buf = newbuf;
        buf->type = newtype;
        buf->maxstk = newsize;
    }
}

void casExpandSendBuffer ( struct client *pClient, ca_uint32_t size )
{
    casExpandBuffer (&pClient->send, size, 1);
}

void casExpandRecvBuffer ( struct client *pClient, ca_uint32_t size )
{
    casExpandBuffer (&pClient->recv, size, 0);
}

/*
 *  create_tcp_client ()
 */
struct client *create_tcp_client ( SOCKET sock )
{
    int                     status;
    struct client           *client;
    int                     intTrue = TRUE;
    osiSocklen_t            addrSize;
    unsigned                priorityOfEvents;

    /* socket passed in is destroyed here if unsuccessful */
    client = create_client ( sock, IPPROTO_TCP );
    if ( ! client ) {
        return NULL;
    }

    /*
     * see TCP(4P) this seems to make unsolicited single events much
     * faster. I take care of queue up as load increases.
     */
    status = setsockopt ( sock, IPPROTO_TCP, TCP_NODELAY,
                (char *) &intTrue, sizeof (intTrue) );
    if (status < 0) {
        errlogPrintf ( "CAS: TCP_NODELAY option set failed\n" );
        destroy_client ( client );
        return NULL;
    }

    /*
     * turn on KEEPALIVE so if the client crashes
     * this task will find out and exit
     */
    status = setsockopt ( sock, SOL_SOCKET, SO_KEEPALIVE,
                    (char *) &intTrue, sizeof (intTrue) );
    if ( status < 0 ) {
        errlogPrintf ( "CAS: SO_KEEPALIVE option set failed\n" );
        destroy_client ( client );
        return NULL;
    }

    /*
     * some concern that vxWorks will run out of mBuf's
     * if this change is made
     *
     * joh 11-10-98
     */
#if 0
    /*
     * set TCP buffer sizes to be synergistic
     * with CA internal buffering
     */
    i = MAX_MSG_SIZE;
    status = setsockopt ( sock, SOL_SOCKET, SO_SNDBUF, (char *) &i, sizeof (i) );
    if (status < 0) {
        errlogPrintf ( "CAS: SO_SNDBUF set failed\n" );
        destroy_client ( client );
        return NULL;
    }
    i = MAX_MSG_SIZE;
    status = setsockopt ( sock, SOL_SOCKET, SO_RCVBUF, (char *) &i, sizeof (i) );
    if (status < 0) {
        errlogPrintf ( "CAS: SO_RCVBUF set failed\n" );
        destroy_client ( client );
        return NULL;
    }
#endif

    addrSize = sizeof ( client->addr );
    status = getpeername ( sock, (struct sockaddr *)&client->addr,
                    &addrSize );
    if ( status < 0 ) {
        epicsPrintf ("CAS: peer address fetch failed\n");
        destroy_tcp_client (client);
        return NULL;
    }

    client->evuser = (struct event_user *) db_init_events ();
    if ( ! client->evuser ) {
        errlogPrintf ("CAS: unable to init the event facility\n");
        destroy_tcp_client (client);
        return NULL;
    }

    status = db_add_extra_labor_event ( client->evuser, rsrv_extra_labor, client );
    if (status != DB_EVENT_OK) {
        errlogPrintf("CAS: unable to setup the event facility\n");
        destroy_tcp_client (client);
        return NULL;
    }

    {
        epicsThreadBooleanStatus    tbs;

        tbs  = epicsThreadHighestPriorityLevelBelow ( epicsThreadPriorityCAServerLow, &priorityOfEvents );
        if ( tbs != epicsThreadBooleanStatusSuccess ) {
            priorityOfEvents = epicsThreadPriorityCAServerLow;
        }
    }

    status = db_start_events ( client->evuser, "CAS-event",
                NULL, NULL, priorityOfEvents );
    if ( status != DB_EVENT_OK ) {
        errlogPrintf ( "CAS: unable to start the event facility\n" );
        destroy_tcp_client ( client );
        return NULL;
    }

    /*
     * add first version message should it be needed
     */
    rsrv_version_reply ( client );

    if ( CASDEBUG > 0 ) {
        char buf[64];
        ipAddrToDottedIP ( &client->addr, buf, sizeof(buf) );
        errlogPrintf ( "CAS: conn req from %s\n", buf );
    }

    return client;
}

void casStatsFetch ( unsigned *pChanCount, unsigned *pCircuitCount )
{
    LOCK_CLIENTQ;
    {
        int circuitCount = ellCount ( &clientQ );
        if ( circuitCount < 0 ) {
            *pCircuitCount = 0;
        }
        else {
            *pCircuitCount = (unsigned) circuitCount;
        }
        *pChanCount = rsrvChannelCount;
    }
    UNLOCK_CLIENTQ;
}
