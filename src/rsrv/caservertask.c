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
#include "tsStamp.h"
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
 * create_base_client ()
 */
struct client *create_base_client ()
{
    struct client *client;
    
    client = freeListMalloc (rsrvClientFreeList);
    if (!client) {
        epicsPrintf ("CAS: no space in pool for a new client\n");
        return NULL;
    }
        
    /*
     * The following inits to zero done instead of a bfill since the send
     * and recv buffers are large and don't need initialization.
     * 
     * memset(client, 0, sizeof(*client));
     */
    
    client->blockSem = semBinaryCreate(semEmpty);
    if(!client->blockSem){
        freeListFree(rsrvClientFreeList, client);
        return NULL;
    }
    
    /*
     * user name initially unknown
     */
    client->pUserName = malloc(1); 
    if(!client->pUserName){
        semBinaryDestroy(client->blockSem);
        freeListFree(rsrvClientFreeList, client);
        return NULL;
    }
    client->pUserName[0] = '\0';
    
    /*
     * host name initially unknown
     */
    client->pHostName = malloc(1); 
    if(!client->pHostName){
        semBinaryDestroy(client->blockSem);
        free(client->pUserName);
        freeListFree(rsrvClientFreeList, client);
        return NULL;
    }
    client->pHostName[0] = '\0';
    
    ellInit(&client->addrq);
    ellInit(&client->putNotifyQue);
    memset((char *)&client->addr, 0, sizeof(client->addr));
    client->tid = 0;
    client->sock = INVALID_SOCKET;
    client->send.stk = 0ul;
    client->send.cnt = 0ul;
    client->recv.stk = 0ul;
    client->recv.cnt = 0ul;
    client->evuser = NULL;
    client->disconnect = FALSE; /* for TCP only */
    tsStampGetCurrent(&client->time_at_last_send);
    tsStampGetCurrent(&client->time_at_last_recv);
    client->proto = IPPROTO_UDP;
    client->minor_version_number = CA_UKN_MINOR_VERSION;
    
    client->send.maxstk = MAX_UDP_SEND;
    
    client->lock = semMutexMustCreate();
    client->putNotifyLock = semMutexMustCreate();
    client->addrqLock = semMutexMustCreate();
    client->eventqLock = semMutexMustCreate();
    
    client->recv.maxstk = MAX_UDP_RECV;
    return client;
}

/*
 *  create_client ()
 */
struct client *create_client (SOCKET sock)
{
    int                     status;
    struct client           *client;
    int                     true = TRUE;
    osiSocklen_t            addrSize;
    unsigned                priorityOfEvents;

    /*
     * see TCP(4P) this seems to make unsolicited single events much
     * faster. I take care of queue up as load increases.
     */
    status = setsockopt (sock, IPPROTO_TCP, TCP_NODELAY, 
                (char *)&true, sizeof(true));
    if (status < 0) {
        errlogPrintf ("CAS: TCP_NODELAY option set failed\n");
        socket_close (sock);
        return NULL;
    }
    
    /* 
     * turn on KEEPALIVE so if the client crashes
     * this task will find out and exit
     */
    status = setsockopt (sock, SOL_SOCKET, SO_KEEPALIVE, 
                    (char *)&true, sizeof(true));
    if (status < 0) {
        errlogPrintf ("CAS: SO_KEEPALIVE option set failed\n");
        socket_close (sock);
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
    status = setsockopt (sock, SOL_SOCKET, SO_SNDBUF, (char *)&i, sizeof(i));
    if (status < 0) {
        errlogPrintf ("CAS: SO_SNDBUF set failed\n");
        socket_close (sock);
        return NULL;
    }
    i = MAX_MSG_SIZE;
    status = setsockopt (sock, SOL_SOCKET, SO_RCVBUF, (char *)&i, sizeof(i));
    if (status < 0) {
        errlogPrintf ("CAS: SO_RCVBUF set failed\n");
        socket_close (sock);
        return NULL;
    }
#endif

    client = (struct client *) create_base_client ();
    if (!client) {
        errlogPrintf("CAS: client init failed\n");
        socket_close (sock);
        return NULL;
    }

    client->proto           = IPPROTO_TCP;
    client->send.maxstk     = MAX_TCP;
    client->recv.maxstk     = MAX_TCP;
    client->sock            = sock;

    addrSize = sizeof (client->addr);
    status = getpeername (sock, (struct sockaddr *)&client->addr,
                    &addrSize);
    if (status < 0) {
            epicsPrintf ("CAS: peer address fetch failed\n");
            destroy_client (client);
            return NULL;
    }

    client->evuser = (struct event_user *) db_init_events();
    if (!client->evuser) {
        errlogPrintf ("CAS: unable to init the event facility\n");
        destroy_client (client);
        return NULL;
    }

    status = db_add_extra_labor_event (client->evuser, write_notify_reply, client);
    if (status != DB_EVENT_OK) {
        errlogPrintf("CAS: unable to setup the event facility\n");
        destroy_client (client);
        return NULL;
    }

    {
        unsigned priorityOfSelf = threadGetPrioritySelf ();
        threadBoolStatus    tbs;

        tbs  = threadLowestPriorityLevelAbove ( priorityOfSelf, &priorityOfEvents );
        if ( tbs != tbsSuccess ) {
            priorityOfEvents = priorityOfSelf;
        }
    }

    status = db_start_events ( client->evuser, "CAS-event", 
                NULL, NULL, priorityOfEvents ); 
    if (status != DB_EVENT_OK) {
        errlogPrintf("CAS: unable to start the event facility\n");
        destroy_client (client);
        return NULL;
    }

    client->recv.cnt = 0ul;

    if (CASDEBUG>0) {
        char buf[64];
        ipAddrToDottedIP (&client->addr, buf, sizeof(buf));
        errlogPrintf ("CAS: conn req from %s\n", buf);
    }

    LOCK_CLIENTQ;
    ellAdd (&clientQ, &client->node);
    UNLOCK_CLIENTQ;

    return client;
}


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
    unsigned priorityOfSelf = threadGetPrioritySelf ();
    unsigned priorityOfUDP;
    threadBoolStatus tbs;
    struct sockaddr_in serverAddr;  /* server's address */
    osiSocklen_t addrSize;
    int status;
    SOCKET clientSock;
    threadId tid;
    int portChange;

    taskwdInsert ( threadGetIdSelf (), NULL, NULL );

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
        threadSuspendSelf ();
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
            threadSuspendSelf ();
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
        threadSuspendSelf ();
	}

    ca_server_port = ntohs (serverAddr.sin_port);

    if ( portChange ) {
        printf ( "cas warning: Configured TCP port was unavailable.\n");
        printf ( "cas warning: Using dynamically assigned TCP port %hu,\n", 
            ca_server_port );
        printf ( "cas warning: but now two or more severs share the same UDP port.\n");
        printf ( "cas warning: Depending on your IP kernel this server may not be\n" );
        printf ( "cas warning: reachable with UDP unicast (a host's IP in EPICS_CA_ADDR_LIST)\n" );
    }

    /* listen and accept new connections */
    if ( listen ( IOC_sock, 10 ) < 0 ) {
        errlogPrintf ("CAS: Listen error\n");
        socket_close (IOC_sock);
        threadSuspendSelf ();
    }

    tbs  = threadHighestPriorityLevelBelow ( priorityOfSelf, &priorityOfUDP );
    if ( tbs != tbsSuccess ) {
        priorityOfUDP = priorityOfSelf;
    }

    tid = threadCreate ( "CAS-UDP", priorityOfUDP,
        threadGetStackSize (threadStackMedium),
        (THREADFUNC) cast_server, 0 );
    if ( tid == 0 ) {
        epicsPrintf ( "CAS: unable to start connection request thread\n" );
    }

    while (TRUE) {
        struct sockaddr     sockAddr;
        osiSocklen_t        addLen = sizeof(sockAddr);

        if ( ( clientSock = accept ( IOC_sock, &sockAddr, &addLen ) ) == INVALID_SOCKET ) {
            errlogPrintf("CAS: Client accept error was \"%s\"\n",
                (int) SOCKERRSTR(SOCKERRNO));
            threadSleep(15.0);
            continue;
        } 
        else {
            unsigned priorityOfClient;
            threadId id;
            struct client *pClient;

            pClient = create_client (clientSock);
            if (!pClient) {
                errlogPrintf ( "CAS: unable to create new client because \"%s\"\n",
                    strerror (errno) );
                threadSleep(15.0);
                continue;
            }

            /* 
             * go up two levels in priority so that the event task is above the
             * task waiting in accept ()
             */
            tbs  = threadLowestPriorityLevelAbove ( priorityOfSelf, &priorityOfClient );
            if ( tbs != tbsSuccess ) {
                priorityOfClient = priorityOfSelf;
            }
            else {
                unsigned belowPriorityOfClient = priorityOfClient;
                tbs  = threadLowestPriorityLevelAbove ( belowPriorityOfClient, &priorityOfClient );
                if ( tbs != tbsSuccess ) {
                    priorityOfClient = belowPriorityOfClient;
                }
            }

            id = threadCreate ( "CAS-client", priorityOfClient,
                    threadGetStackSize (threadStackBig),
                    (THREADFUNC)camsgtask, (void *)pClient );
            if (id==0) {
                destroy_client ( pClient );
                errlogPrintf ( "CAS: task creation for new client failed\n" );
                threadSleep ( 15.0 );
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
    threadId tid;

    clientQlock = semMutexMustCreate();

    ellInit (&clientQ);
    freeListInitPvt (&rsrvClientFreeList, sizeof(struct client), 8);
    freeListInitPvt (&rsrvChanFreeList, sizeof(struct channel_in_use), 512);
    freeListInitPvt (&rsrvEventFreeList, sizeof(struct event_ext), 512);
    ellInit (&beaconAddrList);
    prsrv_cast_client = NULL;
    pCaBucket = NULL;

    tid = threadCreate ("CAS-TCP",
        threadPriorityChannelAccessServer,
        threadGetStackSize(threadStackMedium),
        (THREADFUNC)req_server,0);
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
    unsigned long           bytes_reserved;
    char                    *state[] = {"up", "down"};
    TS_STAMP                current;
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

    tsStampGetCurrent(&current);
    send_delay = tsStampDiffInSeconds(&current,&client->time_at_last_send);
    recv_delay = tsStampDiffInSeconds(&current,&client->time_at_last_recv);

    printf( "%s(%s): User=\"%s\", V%d.%u, Channel Count=%d\n", 
        clientHostName,
        client->pHostName,
        client->pUserName,
        CA_PROTOCOL_VERSION,
        client->minor_version_number,
        ellCount(&client->addrq));
    if (level>=1) {
        printf ("\tTask Id=%p, Protocol=%3s, Socket FD=%d\n", client->tid,
            pproto, client->sock); 
        printf( 
        "\tSecs since last send %6.2f, Secs since last receive %6.2f\n", 
            send_delay, recv_delay);
        printf( 
        "\tUnprocessed request bytes=%lu, Undelivered response bytes=%lu, State=%s\n", 
            client->send.stk,
            client->recv.cnt - client->recv.stk,
            state[client->disconnect?1:0]); 
    }

    if (level>=2u) {
        bytes_reserved = 0;
        bytes_reserved += sizeof(struct client);

        semMutexMustTake(client->addrqLock);
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
        semMutexGive(client->addrqLock);
        printf( "\t%ld bytes allocated\n", bytes_reserved);


        semMutexMustTake(client->addrqLock);
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
        semMutexGive(client->addrqLock);
        printf("\n");
    }

    if (level >= 3u) {
        printf( "\tSend Lock\n");
        semMutexShow(client->lock,1);
        printf( "\tPut Notify Lock\n");
        semMutexShow (client->putNotifyLock,1);
        printf( "\tAddress Queue Lock\n");
        semMutexShow (client->addrqLock,1);
        printf( "\tEvent Queue Lock\n");
        semMutexShow (client->eventqLock,1);
        printf( "\tBlock Semaphore\n");
        semBinaryShow (client->blockSem,1);
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

    printf ("Channel Access Server V%d.%d\n",
        CA_PROTOCOL_VERSION, CA_MINOR_VERSION);

    LOCK_CLIENTQ
    client = (struct client *) ellNext (&clientQ);
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

        printChannelAccessAddressList (&beaconAddrList);
    }
}

/* 
 * destroy_client ()
 */
void destroy_client (struct client *client)
{
    struct event_ext        *pevext;
    struct channel_in_use   *pciu;
    int                     status;

    if (!client) {
        return;
    }
    
    if (client->proto != IPPROTO_TCP) {
        errlogPrintf ("CAS: non TCP client delete ignored\n");
        return;
    }

    LOCK_CLIENTQ;
    ellDelete (&clientQ, &client->node);
    UNLOCK_CLIENTQ;

    if (CASDEBUG>0) {
        errlogPrintf ("CAS: Connection %d Terminated\n", client->sock);
    }

    /*
     * exit flow control so the event system will
     * shutdown correctly
     */
    db_event_flow_ctrl_mode_off (client->evuser);

    /*
     * Server task deleted first since close() is not reentrant
     */
    if ( client->tid != 0 ) {
        taskwdRemove (client->tid);
    }

    while(TRUE){
        semMutexMustTake (client->addrqLock);
        pciu = (struct channel_in_use *) ellGet(&client->addrq);
        semMutexGive (client->addrqLock);
        if (!pciu) {
            break;
        }

        /*
         * put notify in progress needs to be deleted
         */
        if (pciu->pPutNotify) {
            if (pciu->pPutNotify->busy) {
                dbNotifyCancel (&pciu->pPutNotify->dbPutNotify);
            }
        }

        while (TRUE){
            /*
             * AS state change could be using this list
             */
            semMutexMustTake (client->eventqLock);

            pevext = (struct event_ext *) ellGet(&pciu->eventq);
            semMutexGive (client->eventqLock);
            if(!pevext){
                break;
            }

            if (pevext->pdbev) {
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
        status = bucketRemoveItemUnsignedId (
                pCaBucket, 
                &pciu->sid);
        UNLOCK_CLIENTQ;
        if(status != S_bucket_success){
            errPrintf (
                status,
                __FILE__,
                __LINE__,
                "Bad id=%d at close",
                pciu->sid);
        }
        status = asRemoveClient(&pciu->asClientPVT);
        if(status!=0 && status != S_asLib_asNotActive){
            printf("And the status is %x \n", status);
            errPrintf(status, __FILE__, __LINE__, "asRemoveClient");
        }

        /*
         * place per channel block onto the
         * free list
         */
        freeListFree (rsrvChanFreeList, pciu);
    }

    if ( client->evuser ) {
        db_close_events (client->evuser);
    }
    
    if (client->sock!=INVALID_SOCKET) {
        if ( socket_close (client->sock) < 0) {
            errlogPrintf("CAS: Unable to close socket\n");
        }
    }

    semMutexDestroy (client->eventqLock);

    semMutexDestroy (client->addrqLock);

    semMutexDestroy (client->putNotifyLock);

    semMutexDestroy (client->lock);

    semBinaryDestroy (client->blockSem);

    if (client->pUserName) {
        free (client->pUserName);
    }

    if (client->pHostName) {
        free (client->pHostName);
    }

    client->minor_version_number = CA_UKN_MINOR_VERSION;

    freeListFree (rsrvClientFreeList, client);
}
