/*************************************************************************\
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

#include "osiSock.h"
#include "osiPoolStatus.h"
#include "epicsSignal.h"
#include "epicsEvent.h"
#include "epicsMutex.h"
#include "epicsTime.h"
#include "errlog.h"
#include "taskwd.h"
#include "addrList.h"
#include "freeList.h"
#include "errlog.h"
#include "db_field_log.h"
#include "dbAddr.h"
#include "dbEvent.h"
#include "dbCommon.h"
#include "epicsStdioRedirect.h"

#define epicsExportSharedSymbols
#include "rsrv.h"
#define GLBLSOURCE
#include "server.h"

#define DELETE_TASK(NAME)\
if(threadNameToId(NAME)!=0)threadDestroy(threadNameToId(NAME));

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
    unsigned priorityOfSelf = epicsThreadGetPrioritySelf ();
    unsigned priorityOfBeacons;
    epicsThreadBooleanStatus tbs;
    struct sockaddr_in serverAddr;  /* server's address */
    osiSocklen_t addrSize;
    int status;
    SOCKET clientSock;
    epicsThreadId tid;
    int portChange;

    epicsSignalInstallSigPipeIgnore ();

    taskwdInsert ( epicsThreadGetIdSelf (), NULL, NULL );
    
    rsrvCurrentClient = epicsThreadPrivateCreate ();

    if ( envGetConfigParamPtr ( &EPICS_CAS_SERVER_PORT ) ) {
        ca_server_port = envGetInetPortConfigParam ( &EPICS_CAS_SERVER_PORT, 
            (unsigned short) CA_SERVER_PORT );
    }
    else {
        ca_server_port = envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT, 
            (unsigned short) CA_SERVER_PORT );
    }

    if (IOC_sock != 0 && IOC_sock != INVALID_SOCKET) {
        epicsSocketDestroy ( IOC_sock );
    }
    
    /*
     * Open the socket. Use ARPA Internet address format and stream
     * sockets. Format described in <sys/socket.h>.
     */
    if ( ( IOC_sock = epicsSocketCreate (AF_INET, SOCK_STREAM, 0) ) == INVALID_SOCKET ) {
        errlogPrintf ("CAS: Socket creation error\n");
        epicsThreadSuspendSelf ();
    }

    epicsSocketEnableAddressReuseDuringTimeWaitState ( IOC_sock );

    /* Zero the sock_addr structure */
    memset ( (void *) &serverAddr, 0, sizeof ( serverAddr ) );
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl (INADDR_ANY); 
    serverAddr.sin_port = htons ( ca_server_port );

    /* get server's Internet address */
    status = bind ( IOC_sock, (struct sockaddr *) &serverAddr, sizeof ( serverAddr ) );
	if ( status < 0 ) {
		if ( SOCKERRNO == SOCK_EADDRINUSE ) {
			/*
			 * enable assignment of a default port
			 * (so the getsockname() call below will
			 * work correctly)
			 */
			serverAddr.sin_port = ntohs (0);
			status = bind ( IOC_sock, 
                (struct sockaddr *) &serverAddr, sizeof ( serverAddr ) );
		}
		if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAS: Socket bind error was \"%s\"\n",
                sockErrBuf );
            epicsThreadSuspendSelf ();
		}
        portChange = 1;
	}
    else {
        portChange = 0;
    }

	addrSize = ( osiSocklen_t ) sizeof ( serverAddr );
	status = getsockname ( IOC_sock, 
			(struct sockaddr *)&serverAddr, &addrSize);
	if ( status ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
		errlogPrintf ( "CAS: getsockname() error %s\n", 
			sockErrBuf );
        epicsThreadSuspendSelf ();
	}

    ca_server_port = ntohs (serverAddr.sin_port);

    if ( portChange ) {
        errlogPrintf ( "cas warning: Configured TCP port was unavailable.\n");
        errlogPrintf ( "cas warning: Using dynamically assigned TCP port %hu,\n", 
            ca_server_port );
        errlogPrintf ( "cas warning: but now two or more servers share the same UDP port.\n");
        errlogPrintf ( "cas warning: Depending on your IP kernel this server may not be\n" );
        errlogPrintf ( "cas warning: reachable with UDP unicast (a host's IP in EPICS_CA_ADDR_LIST)\n" );
    }

    /* listen and accept new connections */
    if ( listen ( IOC_sock, 20 ) < 0 ) {
        errlogPrintf ("CAS: Listen error\n");
        epicsSocketDestroy (IOC_sock);
        epicsThreadSuspendSelf ();
    }

    tbs  = epicsThreadHighestPriorityLevelBelow ( priorityOfSelf, &priorityOfBeacons );
    if ( tbs != epicsThreadBooleanStatusSuccess ) {
        priorityOfBeacons = priorityOfSelf;
    }

    beacon_startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    beacon_ctl = ctlPause;

    tid = epicsThreadCreate ( "CAS-beacon", priorityOfBeacons,
        epicsThreadGetStackSize (epicsThreadStackSmall),
        rsrv_online_notify_task, 0 );
    if ( tid == 0 ) {
        epicsPrintf ( "CAS: unable to start beacon thread\n" );
    }

    epicsEventMustWait(beacon_startStopEvent);
    epicsEventSignal(castcp_startStopEvent);

    while (TRUE) {
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

/*
 * rsrv_init ()
 */
int rsrv_init (void)
{
    epicsThreadBooleanStatus tbs;
    unsigned priorityOfConnectDaemon;
    epicsThreadId tid;
    long maxBytesAsALong;
    long status;

    clientQlock = epicsMutexMustCreate();

    ellInit ( &clientQ );
    freeListInitPvt ( &rsrvClientFreeList, sizeof(struct client), 8 );
    freeListInitPvt ( &rsrvChanFreeList, sizeof(struct channel_in_use), 512 );
    freeListInitPvt ( &rsrvEventFreeList, sizeof(struct event_ext), 512 );
    freeListInitPvt ( &rsrvSmallBufFreeListTCP, MAX_TCP, 16 );
    initializePutNotifyFreeList ();

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
    freeListInitPvt ( &rsrvLargeBufFreeListTCP, rsrvSizeofLargeBufTCP, 1 );
    ellInit ( &beaconAddrList );
    prsrv_cast_client = NULL;
    pCaBucket = NULL;

    castcp_startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    castcp_ctl = ctlPause;

    /*
     * go down two levels so that we are below 
     * the TCP and event threads started on behalf
     * of individual clients
     */
    tbs  = epicsThreadHighestPriorityLevelBelow ( 
        epicsThreadPriorityCAServerLow, &priorityOfConnectDaemon );
    if ( tbs == epicsThreadBooleanStatusSuccess ) {
        tbs  = epicsThreadHighestPriorityLevelBelow ( 
            priorityOfConnectDaemon, &priorityOfConnectDaemon );
        if ( tbs != epicsThreadBooleanStatusSuccess ) {
            priorityOfConnectDaemon = epicsThreadPriorityCAServerLow;
        }
    }
    else {
        priorityOfConnectDaemon = epicsThreadPriorityCAServerLow;
    }

    tid = epicsThreadCreate ( "CAS-TCP",
        priorityOfConnectDaemon,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        req_server, 0);
    if ( tid == 0 ) {
        epicsPrintf ( "CAS: unable to start connection request thread\n" );
    }

    epicsEventMustWait(castcp_startStopEvent);

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
    struct client * client, ELLLIST * pList )
{
    unsigned i = 0u;
    struct channel_in_use * pciu;
    epicsMutexMustLock ( client->chanListLock );
    pciu = (struct channel_in_use *) pList->node.next;
    while ( pciu ){
        printf( "\t%s(%d%c%c)", 
            pciu->addr.precord->name,
            ellCount ( &pciu->eventq ),
            asCheckGet ( pciu->asClientPVT ) ? 'r': '-',
            rsrvCheckPut ( pciu ) ? 'w': '-' );
        pciu = ( struct channel_in_use * ) ellNext ( &pciu->node );
        if(  ++i % 3u == 0u ) {
            printf ( "\n" );
        }
    }
    epicsMutexUnlock ( client->chanListLock );
}

/*
 *  log_one_client ()
 */
static void log_one_client (struct client *client, unsigned level)
{
    char                    *pproto;
    double                  send_delay;
    double                  recv_delay;
    char                    *state[] = {"up", "down"};
    epicsTimeStamp          current;
    char                    clientHostName[256];

    ipAddrToDottedIP (&client->addr, clientHostName, sizeof(clientHostName));

    if(client->proto == IPPROTO_UDP){
        pproto = "UDP";
    }
    else if(client->proto == IPPROTO_TCP){
        pproto = "TCP";
    }
    else{
        pproto = "UKN";
    }

    epicsTimeGetCurrent(&current);
    send_delay = epicsTimeDiffInSeconds(&current,&client->time_at_last_send);
    recv_delay = epicsTimeDiffInSeconds(&current,&client->time_at_last_recv);

    printf ( "%s %s(%s): User=\"%s\", V%u.%u, %d Channels, Priority=%u\n", 
        pproto,
        clientHostName,
        client->pHostName ? client->pHostName : "",
        client->pUserName ? client->pUserName : "",
        CA_MAJOR_PROTOCOL_REVISION,
        client->minor_version_number,
        ellCount(&client->chanList) + 
            ellCount(&client->chanPendingUpdateARList),
        client->priority );
    if ( level >= 1 ) {
        printf ("\tTask Id=%p, Socket FD=%d\n", 
            (void *) client->tid, client->sock); 
        printf( 
        "\tSecs since last send %6.2f, Secs since last receive %6.2f\n", 
            send_delay, recv_delay);
        printf( 
        "\tUnprocessed request bytes=%u, Undelivered response bytes=%u\n", 
            client->recv.cnt - client->recv.stk,
            client->send.stk ); 
        printf( 
        "\tState=%s%s%s\n", 
            state[client->disconnect?1:0],
            client->send.type == mbtLargeTCP ? " jumbo-send-buf" : "",
            client->recv.type == mbtLargeTCP ? " jumbo-recv-buf" : "");
    }

    if ( level >= 2u ) {
        unsigned bytes_reserved = 0;
        bytes_reserved += sizeof(struct client);
        bytes_reserved += countChanListBytes ( 
                            client, & client->chanList );
        bytes_reserved += countChanListBytes ( 
                        client, & client->chanPendingUpdateARList );
        printf( "\t%d bytes allocated\n", bytes_reserved);
        showChanList ( client, & client->chanList );
        showChanList ( client, & client->chanPendingUpdateARList );
        printf("\n");
    }

    if ( level >= 3u ) {
        printf( "\tSend Lock\n");
        epicsMutexShow(client->lock,1);
        printf( "\tPut Notify Lock\n");
        epicsMutexShow (client->putNotifyLock,1);
        printf( "\tAddress Queue Lock\n");
        epicsMutexShow (client->chanListLock,1);
        printf( "\tEvent Queue Lock\n");
        epicsMutexShow (client->eventqLock,1);
        printf( "\tBlock Semaphore\n");
        epicsEventShow (client->blockSem,1);
    }
}

/*
 *  casr()
 */
void epicsShareAPI casr (unsigned level)
{
    size_t bytes_reserved;
    struct client *client;

    if ( ! clientQlock ) {
        return;
    }

    printf ("Channel Access Server V%s\n",
        CA_VERSION_STRING ( CA_MINOR_PROTOCOL_REVISION ) );

    LOCK_CLIENTQ
    client = (struct client *) ellNext ( &clientQ.node );
    if (client) {
        printf("Connected circuits:\n");
    }
    else {
        printf("No clients connected.\n");
    }
    while (client) {
        log_one_client(client, level);
        client = (struct client *) ellNext(&client->node);
    }
    UNLOCK_CLIENTQ

    if (level>=2 && prsrv_cast_client) {
        printf( "UDP Server:\n" );
        log_one_client(prsrv_cast_client, level);
    }
    
    if (level>=2u) {
        bytes_reserved = 0u;
        bytes_reserved += sizeof (struct client) * 
                    freeListItemsAvail (rsrvClientFreeList);
        bytes_reserved += sizeof (struct channel_in_use) *
                    freeListItemsAvail (rsrvChanFreeList);
        bytes_reserved += sizeof(struct event_ext) *
                    freeListItemsAvail (rsrvEventFreeList);
        bytes_reserved += MAX_TCP * 
                    freeListItemsAvail ( rsrvSmallBufFreeListTCP );
        bytes_reserved += rsrvSizeofLargeBufTCP * 
                    freeListItemsAvail ( rsrvLargeBufFreeListTCP );
        bytes_reserved += rsrvSizeOfPutNotify ( 0 ) * 
                    freeListItemsAvail ( rsrvPutNotifyFreeList );
        printf( "There are currently %u bytes on the server's free list\n",
            (unsigned int) bytes_reserved);
        printf( "%u client(s), %u channel(s), %u event(s) (monitors) %u putNotify(s)\n",
            (unsigned int) freeListItemsAvail ( rsrvClientFreeList ),
            (unsigned int) freeListItemsAvail ( rsrvChanFreeList ),
            (unsigned int) freeListItemsAvail ( rsrvEventFreeList ),
            (unsigned int) freeListItemsAvail ( rsrvPutNotifyFreeList ));
        printf( "%u small buffers (%u bytes ea), and %u jumbo buffers (%u bytes ea)\n",
            (unsigned int) freeListItemsAvail ( rsrvSmallBufFreeListTCP ),
            MAX_TCP,
            (unsigned int) freeListItemsAvail ( rsrvLargeBufFreeListTCP ),
            rsrvSizeofLargeBufTCP );
        if(pCaBucket){
            printf( "The server's resource id conversion table:\n");
            LOCK_CLIENTQ;
            bucketShow (pCaBucket);
            UNLOCK_CLIENTQ;
        }
        printf ( "The server's array size limit is %u bytes max\n",
            rsrvSizeofLargeBufTCP );

        printChannelAccessAddressList (&beaconAddrList);
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
                freeListFree ( rsrvLargeBufFreeListTCP,  client->send.buf );
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
                freeListFree ( rsrvLargeBufFreeListTCP,  client->recv.buf );
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

void casExpandSendBuffer ( struct client *pClient, ca_uint32_t size )
{
    if ( pClient->send.type == mbtSmallTCP && rsrvSizeofLargeBufTCP > MAX_TCP 
            && size <= rsrvSizeofLargeBufTCP ) {
        int spaceAvailOnFreeList = freeListItemsAvail ( rsrvLargeBufFreeListTCP ) > 0;
        if ( osiSufficentSpaceInPool(rsrvSizeofLargeBufTCP) || spaceAvailOnFreeList ) { 
            char *pNewBuf = ( char * ) freeListCalloc ( rsrvLargeBufFreeListTCP );
            if ( pNewBuf ) {
                memcpy ( pNewBuf, pClient->send.buf, pClient->send.stk );
                freeListFree ( rsrvSmallBufFreeListTCP,  pClient->send.buf );
                pClient->send.buf = pNewBuf;
                pClient->send.maxstk = rsrvSizeofLargeBufTCP;
                pClient->send.type = mbtLargeTCP;
            }
        }
    }
}

void casExpandRecvBuffer ( struct client *pClient, ca_uint32_t size )
{
    if ( pClient->recv.type == mbtSmallTCP && rsrvSizeofLargeBufTCP > MAX_TCP
            && size <= rsrvSizeofLargeBufTCP) {
        int spaceAvailOnFreeList = freeListItemsAvail ( rsrvLargeBufFreeListTCP ) > 0;
        if ( osiSufficentSpaceInPool(rsrvSizeofLargeBufTCP) || spaceAvailOnFreeList ) { 
            char *pNewBuf = ( char * ) freeListCalloc ( rsrvLargeBufFreeListTCP );
            if ( pNewBuf ) {
                assert ( pClient->recv.cnt >= pClient->recv.stk );
                memcpy ( pNewBuf, &pClient->recv.buf[pClient->recv.stk], pClient->recv.cnt - pClient->recv.stk );
                freeListFree ( rsrvSmallBufFreeListTCP,  pClient->recv.buf );
                pClient->recv.buf = pNewBuf;
                pClient->recv.cnt = pClient->recv.cnt - pClient->recv.stk;
                pClient->recv.stk = 0u;
                pClient->recv.maxstk = rsrvSizeofLargeBufTCP;
                pClient->recv.type = mbtLargeTCP;
            }
        }
    }
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
