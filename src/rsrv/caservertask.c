/*
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	5-88
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * 	Modification Log:
 * 	-----------------
 *	.01 joh	030891	now saves old client structure for later reuse
 *	.02 joh	071591	print the delay from the last interaction in
 *			client_stat().
 *	.03 joh 080991	close the socket if task create fails
 *	.04 joh	090591	updated for v5 vxWorks
 *	.05 joh	103091	print task id and disconnect state in client_stat()
 *	.06 joh	112691	dont print client disconnect message unless
 *			debug is on.
 *	.07 joh 020592	substitute lstConcat() for lstExtract() to avoid
 *			replacing the destination list
 *	.08 joh 021492	cleaned up terminate_one_client()
 *	.09 joh 022092	print free list statistics in client_stat()
 *	.10 joh 022592	print more statistics in client_stat()
 *	.11 joh 073093	added args to taskSpawn for v5.1 vxWorks	
 *	.12 joh 020494	identifies the client in client_stat
 */

static char *sccsId = "@(#) $Id$";

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include <sys/types.h>
#include <errno.h>

#include "osiSock.h"
#include "tsStamp.h"
#include "errlog.h"
#include "ellLib.h"
#include "taskwd.h"
#include "db_access.h"
#include "envDefs.h"
#include "freeList.h"
#include "errlog.h"
#include "bsdSocketResource.h"

#include "server.h"

LOCAL int terminate_one_client(struct client *client);
LOCAL void log_one_client(struct client *client, unsigned level);


/*
 *
 *	req_server()
 *
 *	CA server task
 *
 *	Waits for connections at the CA port and spawns a task to
 *	handle each of them
 *
 */
int req_server(void)
{
	struct sockaddr_in serverAddr;	/* server's address */
	int status;
	int i;

	taskwdInsert(threadGetIdSelf(),NULL,NULL);

	ca_server_port = caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

	if (IOC_sock != 0 && IOC_sock != ERROR)
		if ((status = socket_close(IOC_sock)) == ERROR)
			errlogPrintf( "CAS: Unable to close open master socket\n");

	/*
	 * Open the socket. Use ARPA Internet address format and stream
	 * sockets. Format described in <sys/socket.h>.
	 */
	if ((IOC_sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		errlogPrintf("CAS: Socket creation error\n");
		threadSuspend(threadGetIdSelf());
	}

	/* Zero the sock_addr structure */
	memset((void *)&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(ca_server_port);

	/* get server's Internet address */
	if (bind(IOC_sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == ERROR) {
		errlogPrintf("CAS: Bind error\n");
		socket_close(IOC_sock);
		threadSuspend(threadGetIdSelf());
	}

	/* listen and accept new connections */
	if (listen(IOC_sock, 10) == ERROR) {
		errlogPrintf("CAS: Listen error\n");
		socket_close(IOC_sock);
		threadSuspend(threadGetIdSelf());
	}

	while (TRUE) {
		struct sockaddr	sockAddr;
		int		addLen = sizeof(sockAddr);

		if ((i = accept(IOC_sock, &sockAddr, &addLen)) == ERROR) {
			errlogPrintf("CAS: Client accept error was \"%s\"\n",
				(int) SOCKERRSTR(SOCKERRNO));
			threadSleep(15.0);
			continue;
		} else {
			threadId id;
			id = threadCreate("CAclient",
				threadPriorityChannelAccessClient,
				threadGetStackSize(threadStackBig),
				(THREADFUNC)camsgtask,(void *)i);
			if (id==0) {
				errlogPrintf("CAS: task creation for new client failed because \"%s\"\n",
					(int) strerror(errno));
				threadSleep(15.0);
				continue;
			}
		}
	}
}


/*
 *
 *	free_client()
 *
 */
int free_client(struct client *client)
{
	if (!client) {
		return ERROR;
	}

	/* remove it from the list of clients */
	/* list delete returns no status */
	LOCK_CLIENTQ;
	ellDelete(&clientQ, &client->node);
	UNLOCK_CLIENTQ;

	terminate_one_client(client);

	freeListFree(rsrvClientFreeList, client);

	return OK;
}


/* 
 * TERMINATE_ONE_CLIENT
 */
LOCAL int terminate_one_client(struct client *client)
{
	threadId       	servertid;
	SOCKET        	tmpsock;
	int        	status;
	struct event_ext 	*pevext;
	struct channel_in_use *pciu;

	if (client->proto != IPPROTO_TCP) {
		errlogPrintf("CAS: non TCP client delete ignored\n");
		return ERROR;
	}

	tmpsock = client->sock;

	if(CASDEBUG>0){
		errlogPrintf("CAS: Connection %d Terminated\n", 
			tmpsock);
	}

	/*
	 * exit flow control so the event system will
	 * shutdown correctly
	 */
	db_event_flow_ctrl_mode_off(client->evuser);

	/*
	 * Server task deleted first since close() is not reentrant
	 */
	servertid = client->tid;
	taskwdRemove(servertid);
	if (servertid != threadGetIdSelf()){
		if(servertid != 0) {
			threadDestroy(servertid);
		}
		servertid = 0;
	}

	while(TRUE){
		semMutexMustTake(client->addrqLock);
		pciu = (struct channel_in_use *) ellGet(&client->addrq);
		semMutexGive(client->addrqLock);
		if(!pciu){
			break;
		}

		/*
		 * put notify in progress needs to be deleted
		 */
		if(pciu->pPutNotify){
			if(pciu->pPutNotify->busy){
                		dbNotifyCancel(&pciu->pPutNotify->dbPutNotify);
			}
		}

		while (TRUE){
			/*
			 * AS state change could be using this list
			 */
			semMutexMustTake(client->eventqLock);
			pevext = (struct event_ext *) ellGet(&pciu->eventq);
			semMutexGive(client->eventqLock);
			if(!pevext){
				break;
			}

			if(pevext->pdbev){
				status = db_cancel_event(pevext->pdbev);
				assert(status == OK);
			}
			freeListFree(rsrvEventFreeList, pevext);
		}
		status = db_flush_extra_labor_event(client->evuser);
		assert(status==OK);
		if(pciu->pPutNotify){
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

	if (client->evuser) {
		status = db_close_events(client->evuser);
		if (status == ERROR)
			threadSuspend(threadGetIdSelf());
	}
	if (socket_close(tmpsock) == ERROR)	/* close socket	 */
		errlogPrintf("CAS: Unable to close socket\n");

	semMutexDestroy(client->eventqLock);

	semMutexDestroy(client->addrqLock);

	semMutexDestroy(client->putNotifyLock);

	semMutexDestroy(client->lock);

	semBinaryDestroy(client->blockSem);

	if(client->pUserName){
		free(client->pUserName);
	}

	if(client->pHostName){
		free(client->pHostName);
	}

	client->minor_version_number = CA_UKN_MINOR_VERSION;

	return OK;
}


/*
 *	client_stat()
 */
int client_stat(unsigned level)
{
	printf ("\"client_stat\" has been replaced by \"casr\"\n");
	return ellCount(&clientQ);
}

/*
 *	casr()
 */
void casr (unsigned level)
{
	size_t bytes_reserved;
	struct client 	*client;

	printf( "Channel Access Server V%d.%d\n",
		CA_PROTOCOL_VERSION,
		CA_MINOR_VERSION);

	LOCK_CLIENTQ;
	client = (struct client *) ellNext(&clientQ);
	if (!client) {
		printf("No clients connected.\n");
	}
	while (client) {

		log_one_client(client, level);

		client = (struct client *) ellNext(&client->node);
	}
	UNLOCK_CLIENTQ;

	if (level>=2 && prsrv_cast_client) {
		log_one_client(prsrv_cast_client, level);
	}
	
	if (level>=2u) {
		bytes_reserved = 0u;
		bytes_reserved += sizeof (struct client) * 
					freeListItemsAvail (rsrvClientFreeList);
		bytes_reserved += sizeof (struct channel_in_use) *
					freeListItemsAvail (rsrvChanFreeList);
		bytes_reserved += (sizeof(struct event_ext)+db_sizeof_event_block()) *
					freeListItemsAvail (rsrvEventFreeList);
		printf(	"There are currently %u bytes on the server's free list\n",
			bytes_reserved);
		printf(	"%u client(s), %u channel(s), and %u event(s) (monitors)\n",
			freeListItemsAvail (rsrvClientFreeList),
			freeListItemsAvail (rsrvChanFreeList),
			freeListItemsAvail (rsrvEventFreeList));

		if(pCaBucket){
			printf(	"The server's resource id conversion table:\n");
			LOCK_CLIENTQ;
			bucketShow (pCaBucket);
			UNLOCK_CLIENTQ;
		}

		caPrintAddrList (&beaconAddrList);
	}
}



/*
 *	log_one_client()
 *
 */
LOCAL void log_one_client(struct client *client, unsigned level)
{
	int			i;
	struct channel_in_use	*pciu;
	char			*pproto;
	double			send_delay;
	double			recv_delay;
	unsigned long		bytes_reserved;
	char			*state[] = {"up", "down"};
	TS_STAMP 		current;
	char            clientHostName[256];

	ipAddrToA (&client->addr, clientHostName, sizeof(clientHostName));

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

	printf(	"%s(%s): User=\"%s\", V%d.%u, Channel Count=%d\n", 
        clientHostName,
		client->pHostName,
		client->pUserName,
		CA_PROTOCOL_VERSION,
		client->minor_version_number,
		ellCount(&client->addrq));
	if (level>=1) {
		printf( "\tTId=0X%lX, Protocol=%3s, Socket FD=%d\n", 
			(unsigned long) client->tid,
			pproto, 
			client->sock); 
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
			bytes_reserved += 
				(sizeof(struct event_ext)+db_sizeof_event_block())*
					ellCount(&pciu->eventq);
			if(pciu->pPutNotify){
				bytes_reserved += sizeof(*pciu->pPutNotify);
				bytes_reserved += pciu->pPutNotify->valueSize;
			}
			pciu = (struct channel_in_use *) ellNext(&pciu->node);
		}
		semMutexGive(client->addrqLock);
		printf(	"\t%ld bytes allocated\n", bytes_reserved);


		semMutexMustTake(client->addrqLock);
		pciu = (struct channel_in_use *) client->addrq.node.next;
		i=0;
		while (pciu){
			printf(	"\t%s(%d%c%c)", 
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
		printf(	"\tSend Lock\n");
		semMutexShow(client->lock);
		printf(	"\tPut Notify Lock\n");
		semMutexShow (client->putNotifyLock);
		printf(	"\tAddress Queue Lock\n");
		semMutexShow (client->addrqLock);
		printf(	"\tEvent Queue Lock\n");
		semMutexShow (client->eventqLock);
		printf(	"\tBlock Semaphore\n");
		semBinaryShow (client->blockSem);
	}
}
