/*
 * $Id$
 *
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   5-88
 *
 *  Experimental Physics and Industrial Control System (EPICS)
 *
 *  Copyright 1991, the Regents of the University of California,
 *  and the University of Chicago Board of Governors.
 *
 *  This software was produced under  U.S. Government contracts:
 *  (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *  and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *  Initial development by:
 *      The Controls and Automation Group (AT-8)
 *      Ground Test Accelerator
 *      Accelerator Technology Division
 *      Los Alamos National Laboratory
 *
 *  Co-developed with
 *      The Controls and Computing Group
 *      Accelerator Systems Division
 *      Advanced Photon Source
 *      Argonne National Laboratory
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "osiSock.h"
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

#define GLBLSOURCE
#include "server.h"
#include "rsrv.h"

#define DELETE_TASK(NAME)\
if(threadNameToId(NAME)!=0)threadDestroy(threadNameToId(NAME));

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
LOCAL int req_server (void)
{
    unsigned priorityOfSelf = epicsThreadGetPrioritySelf ();
    unsigned priorityOfUDP;
    epicsThreadBooleanStatus tbs;
    struct sockaddr_in serverAddr;  /* server's address */
    osiSocklen_t addrSize;
    int status;
    SOCKET clientSock;
    epicsThreadId tid;
    int portChange;

    taskwdInsert ( epicsThreadGetIdSelf (), NULL, NULL );

    ca_server_port = envGetInetPortConfigParam (&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

    if (IOC_sock != 0 && IOC_sock != INVALID_SOCKET)
        if ((status = socket_close(IOC_sock)) < 0)
            errlogPrintf( "CAS: Unable to close open master socket\n");

    /*
     * Open the socket. Use ARPA Internet address format and stream
     * sockets. Format described in <sys/socket.h>.
     */
    if ( ( IOC_sock = socket(AF_INET, SOCK_STREAM, 0) ) == INVALID_SOCKET ) {
        errlogPrintf ("CAS: Socket creation error\n");
        epicsThreadSuspendSelf ();
    }

    /*
     * Note: WINSOCK appears to assign a different functionality for 
     * SO_REUSEADDR compared to other OS. With WINSOCK SO_REUSEADDR indicates
     * that simultaneously servers can bind to the same TCP port on the same host!
     * Also, servers are always enabled to reuse a port immediately after 
     * they exit ( even if SO_REUSEADDR isnt set ).
     */
#   ifndef SO_REUSEADDR_ALLOWS_SIMULTANEOUS_TCP_SERVERS_TO_USE_SAME_PORT
    {
        int flag = 1;
        status = setsockopt ( IOC_sock,  SOL_SOCKET, SO_REUSEADDR,
                    (char *) &flag, sizeof (flag) );
        if ( status < 0 ) {
            int errnoCpy = SOCKERRNO;
            errlogPrintf (
        "%s: set socket option SO_REUSEADDR failed because \"%s\"\n", 
                    __FILE__, SOCKERRSTR (errnoCpy) );
        }
    }
#   endif

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
            errlogPrintf ( "CAS: Socket bind error was \"%s\"\n",
                SOCKERRSTR (SOCKERRNO) );
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
		errlogPrintf ( "CAS: getsockname() error %s\n", 
			SOCKERRSTR(SOCKERRNO) );
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
    if ( listen ( IOC_sock, 10 ) < 0 ) {
        errlogPrintf ("CAS: Listen error\n");
        socket_close (IOC_sock);
        epicsThreadSuspendSelf ();
    }

    tbs  = epicsThreadHighestPriorityLevelBelow ( priorityOfSelf, &priorityOfUDP );
    if ( tbs != epicsThreadBooleanStatusSuccess ) {
        priorityOfUDP = priorityOfSelf;
    }

    tid = epicsThreadCreate ( "CAS-UDP", priorityOfUDP,
        epicsThreadGetStackSize (epicsThreadStackMedium),
        (EPICSTHREADFUNC) cast_server, 0 );
    if ( tid == 0 ) {
        epicsPrintf ( "CAS: unable to start connection request thread\n" );
    }

    while (TRUE) {
        struct sockaddr     sockAddr;
        osiSocklen_t        addLen = sizeof(sockAddr);

        if ( ( clientSock = accept ( IOC_sock, &sockAddr, &addLen ) ) == INVALID_SOCKET ) {
            errlogPrintf("CAS: Client accept error was \"%s\"\n",
                (int) SOCKERRSTR(SOCKERRNO));
            epicsThreadSleep(15.0);
            continue;
        } 
        else {
            epicsThreadId id;
            struct client *pClient;

            pClient = create_tcp_client ( clientSock );
            if ( ! pClient ) {
                errlogPrintf ( "CAS: unable to create new client because \"%s\"\n",
                    strerror ( errno ) );
                epicsThreadSleep ( 15.0 );
                continue;
            }

            LOCK_CLIENTQ;
            ellAdd ( &clientQ, &pClient->node );
            UNLOCK_CLIENTQ;

            id = epicsThreadCreate ( "CAS-client", epicsThreadPriorityCAServerLow,
                    epicsThreadGetStackSize ( epicsThreadStackBig ),
                    ( EPICSTHREADFUNC ) camsgtask, pClient );
            if ( id == 0 ) {
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
epicsShareFunc int epicsShareAPI rsrv_init (void)
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

    status =  envGetLongConfigParam ( &EPICS_CA_MAX_ARRAY_BYTES, &maxBytesAsALong );
    if ( status || maxBytesAsALong < 0 ) {
        errlogPrintf ( "cas: EPICS_CA_MAX_ARRAY_BYTES was not a positive integer\n" );
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
            errlogPrintf ( "cas: EPICS_CA_MAX_ARRAY_BYTES was rounded up to %u\n", MAX_TCP );
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
        (EPICSTHREADFUNC)req_server, 0);
    if ( tid == 0 ) {
        epicsPrintf ( "CAS: unable to start connection request thread\n" );
    }

    return RSRV_OK;
}

/*
 *  log_one_client ()
 */
LOCAL void log_one_client (struct client *client, unsigned level)
{
    int                     i;
    struct channel_in_use   *pciu;
    char                    *pproto;
    double                  send_delay;
    double                  recv_delay;
    unsigned                bytes_reserved;
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

    printf( "%s(%s): User=\"%s\", V%u.%u, Channel Count=%d\n", 
        clientHostName,
        client->pHostName ? client->pHostName : "",
        client->pUserName ? client->pUserName : "",
        CA_MAJOR_PROTOCOL_REVISION,
        client->minor_version_number,
        ellCount(&client->addrq));
    if (level>=1) {
        printf ("\tTask Id=%p, Protocol=%3s, Socket FD=%d\n", client->tid,
            pproto, client->sock); 
        printf( 
        "\tSecs since last send %6.2f, Secs since last receive %6.2f\n", 
            send_delay, recv_delay);
        printf( 
        "\tUnprocessed request bytes=%u, Undelivered response bytes=%u, State=%s\n", 
            client->send.stk,
            client->recv.cnt - client->recv.stk,
            state[client->disconnect?1:0]); 
    }

    if (level>=2u) {
        bytes_reserved = 0;
        bytes_reserved += sizeof(struct client);

        epicsMutexMustLock(client->addrqLock);
        pciu = (struct channel_in_use *) client->addrq.node.next;
        while (pciu){
            bytes_reserved += sizeof(struct channel_in_use);
            bytes_reserved += sizeof(struct event_ext)*ellCount(&pciu->eventq);
            if(pciu->pPutNotify){
                bytes_reserved += sizeof(*pciu->pPutNotify);
                bytes_reserved += pciu->pPutNotify->valueSize;
            }
            pciu = (struct channel_in_use *) ellNext(&pciu->node);
        }
        epicsMutexUnlock(client->addrqLock);
        printf( "\t%d bytes allocated\n", bytes_reserved);


        epicsMutexMustLock(client->addrqLock);
        pciu = (struct channel_in_use *) client->addrq.node.next;
        i=0;
        while (pciu){
            printf( "\t%s(%d%c%c)", 
                pciu->addr.precord->name,
                ellCount(&pciu->eventq),
                asCheckGet(pciu->asClientPVT)?'r':'-',
                rsrvCheckPut(pciu)?'w':'-');
            pciu = (struct channel_in_use *) ellNext(&pciu->node);
            if( ++i % 3 == 0){
                printf("\n");
            }
        }
        epicsMutexUnlock(client->addrqLock);
        printf("\n");
    }

    if (level >= 3u) {
        printf( "\tSend Lock\n");
        epicsMutexShow(client->lock,1);
        printf( "\tPut Notify Lock\n");
        epicsMutexShow (client->putNotifyLock,1);
        printf( "\tAddress Queue Lock\n");
        epicsMutexShow (client->addrqLock,1);
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
    client = (struct client *) ellNext ( &clientQ );
    if (!client) {
        printf("No clients connected.\n");
    }
    while (client) {

        log_one_client(client, level);

        client = (struct client *) ellNext(&client->node);
    }
    UNLOCK_CLIENTQ

    if (level>=2 && prsrv_cast_client) {
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
        printf( "There are currently %u bytes on the server's free list\n",
            bytes_reserved);
        printf( "%u client(s), %u channel(s), and %u event(s) (monitors)\n",
            freeListItemsAvail (rsrvClientFreeList),
            freeListItemsAvail (rsrvChanFreeList),
            freeListItemsAvail (rsrvEventFreeList));

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
        if ( socket_close (client->sock) < 0) {
            errlogPrintf( "CAS: Unable to close socket\n" );
        }
    }

    if ( client->proto == IPPROTO_TCP ) {
        if ( client->send.type == mbtSmallTCP ) {
            if ( client->send.buf ) {
                freeListFree ( rsrvSmallBufFreeListTCP,  client->send.buf );
            }
            if ( client->recv.buf ) {
                freeListFree ( rsrvSmallBufFreeListTCP,  client->recv.buf );
            }
        }
        else if ( client->send.type == mbtLargeTCP ) {
            if ( client->send.buf ) {
                freeListFree ( rsrvLargeBufFreeListTCP,  client->send.buf );
            }
            if ( client->recv.buf ) {
                freeListFree ( rsrvLargeBufFreeListTCP,  client->recv.buf );
            }
        }
        else {
            errlogPrintf ( "Currupt buffer type code during cleanup?\n" );
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

    epicsMutexDestroy ( client->eventqLock );

    epicsMutexDestroy ( client->addrqLock );

    epicsMutexDestroy ( client->putNotifyLock );

    epicsMutexDestroy ( client->lock );

    epicsEventDestroy ( client->blockSem );

    if ( client->pUserName ) {
        free ( client->pUserName );
    }

    if ( client->pHostName ) {
        free ( client->pHostName );
    } 

    freeListFree ( rsrvClientFreeList, client );
}

void destroy_tcp_client ( struct client *client )
{
    struct event_ext        *pevext;
    struct channel_in_use   *pciu;
    int                     status;

    if ( CASDEBUG > 0 ) {
        errlogPrintf ( "CAS: Connection %d Terminated\n", client->sock );
    }

    if ( client->evuser ) {
        db_event_flow_ctrl_mode_off ( client->evuser );
    }

    while ( TRUE ){
        epicsMutexMustLock ( client->addrqLock );
        pciu = (struct channel_in_use *) ellGet ( &client->addrq );
        epicsMutexUnlock ( client->addrqLock );
        if ( ! pciu ) {
            break;
        }

        /*
         * put notify in progress needs to be deleted
         */
        if ( pciu->pPutNotify ) {
            if ( pciu->pPutNotify->busy ) {
                dbNotifyCancel ( &pciu->pPutNotify->dbPutNotify );
            }
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
        status = db_flush_extra_labor_event (client->evuser);
        assert (status==DB_EVENT_OK);
        if (pciu->pPutNotify) {
            free(pciu->pPutNotify);
        }
        LOCK_CLIENTQ;
        status = bucketRemoveItemUnsignedId ( pCaBucket, &pciu->sid);
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
    
    client = freeListCalloc ( rsrvClientFreeList );
    if ( ! client ) {
        epicsPrintf ("CAS: no space in pool for a new client\n");
        return NULL;
    }    

    client->sock = sock;
    client->proto = proto;

    client->blockSem = epicsEventCreate ( epicsEventEmpty );
    client->lock = epicsMutexCreate();
    client->putNotifyLock = epicsMutexCreate();
    client->addrqLock = epicsMutexCreate();
    client->eventqLock = epicsMutexCreate();
    if ( ! client->blockSem || ! client->lock || ! client->putNotifyLock ||
        ! client->addrqLock || ! client->eventqLock ) {
        destroy_client ( client );
        return NULL;
    }

    client->pUserName = NULL; 
    client->pHostName = NULL;     
    ellInit ( &client->addrq );
    ellInit ( &client->putNotifyQue );
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
    pClient->tid = epicsThreadGetIdSelf ();
    taskwdInsert ( pClient->tid, NULL, NULL );
}

void casExpandSendBuffer ( struct client *pClient, ca_uint32_t size )
{
    if ( pClient->send.type == mbtSmallTCP && rsrvSizeofLargeBufTCP > MAX_TCP ) {
        char *pNewBuf = ( char * ) freeListCalloc ( rsrvLargeBufFreeListTCP );
        memcpy ( pNewBuf, pClient->send.buf, pClient->send.stk );
        pClient->send.buf = pNewBuf;
        pClient->send.maxstk = rsrvSizeofLargeBufTCP;
        pClient->send.type = mbtLargeTCP;
    }
}

void casExpandRecvBuffer ( struct client *pClient, ca_uint32_t size )
{
    if ( pClient->recv.type == mbtSmallTCP && rsrvSizeofLargeBufTCP > MAX_TCP ) {
        char *pNewBuf = ( char * ) freeListCalloc ( rsrvLargeBufFreeListTCP );
        assert ( pClient->recv.cnt >= pClient->recv.stk );
        memcpy ( pNewBuf, &pClient->recv.buf[pClient->recv.stk], pClient->recv.cnt - pClient->recv.stk );
        pClient->recv.buf = pNewBuf;
        pClient->recv.cnt = pClient->recv.cnt - pClient->recv.stk;
        pClient->recv.stk = 0u;
        pClient->recv.maxstk = rsrvSizeofLargeBufTCP;
        pClient->recv.type = mbtLargeTCP;
    }
}

/*
 *  create_tcp_client ()
 */
struct client *create_tcp_client ( SOCKET sock )
{
    int                     status;
    struct client           *client;
    int                     true = TRUE;
    osiSocklen_t            addrSize;
    unsigned                priorityOfEvents;

    client = create_client ( sock, IPPROTO_TCP );
    if ( ! client ) {
        errlogPrintf ("CAS: no space in pool for a new TCP client\n");
        return NULL;
    }

    /*
     * see TCP(4P) this seems to make unsolicited single events much
     * faster. I take care of queue up as load increases.
     */
    status = setsockopt ( sock, IPPROTO_TCP, TCP_NODELAY, 
                (char *) &true, sizeof (true) );
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
                    (char *) &true, sizeof (true) );
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

    status = db_add_extra_labor_event (client->evuser, write_notify_reply, client);
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

    if ( CASDEBUG > 0 ) {
        char buf[64];
        ipAddrToDottedIP ( &client->addr, buf, sizeof(buf) );
        errlogPrintf ( "CAS: conn req from %s\n", buf );
    }

    return client;
}

