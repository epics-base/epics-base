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
 */

static char *sccsId = "@(#)caservertask.c	1.13\t7/28/92";

#include <vxWorks.h>
#include <ellLib.h>
#include <taskLib.h>
#include <types.h>
#include <sockLib.h>
#include <socket.h>
#include <in.h>
#include <unistd.h>
#include <logLib.h>
#include <string.h>
#include <usrLib.h>
#include <errnoLib.h>
#include <stdio.h>
#include <tickLib.h>
#include <sysLib.h>

#include <taskwd.h>
#include <db_access.h>
#include <task_params.h>
#include <server.h>

LOCAL int terminate_one_client(struct client *client);
LOCAL void log_one_client(struct client *client);


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
	struct sockaddr_in 	serverAddr;	/* server's address */
	int        		status;
	int			i;

	if (IOC_sock != 0 && IOC_sock != ERROR)
		if ((status = close(IOC_sock)) == ERROR)
			logMsg( "CAS: Unable to close open master socket\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);

	/*
	 * Open the socket. Use ARPA Internet address format and stream
	 * sockets. Format described in <sys/socket.h>.
	 */
	if ((IOC_sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		logMsg("CAS: Socket creation error\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		taskSuspend(0);
	}
	
        taskwdInsert((int)taskIdCurrent,NULL,NULL);

	/* Zero the sock_addr structure */
	bfill((char *)&serverAddr, sizeof(serverAddr), 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = CA_SERVER_PORT;

	/* get server's Internet address */
	if (bind(IOC_sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == ERROR) {
		logMsg("CAS: Bind error\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		close(IOC_sock);
		taskSuspend(0);
	}

	/* listen and accept new connections */
	if (listen(IOC_sock, 10) == ERROR) {
		logMsg("CAS: Listen error\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		close(IOC_sock);
		taskSuspend(0);
	}

	while (TRUE) {
		if ((i = accept(IOC_sock, NULL, 0)) == ERROR) {
			logMsg("CAS: Accept error\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			taskSuspend(0);
		} else {
			status = taskSpawn(CA_CLIENT_NAME,
					   CA_CLIENT_PRI,
					   CA_CLIENT_OPT,
					   CA_CLIENT_STACK,
					   (FUNCPTR) camsgtask,
					   i,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL,
					   NULL);
			if (status == ERROR) {
				logMsg("CAS: task creation failed\n",
					NULL,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
				logMsg("CAS: (client ignored)\n",
					NULL,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
				printErrno(errnoGet());
				close(i);
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
	if (client) {
		/* remove it from the list of clients */
		/* list delete returns no status */
		LOCK_CLIENTQ;
		ellDelete((ELLLIST *)&clientQ, (ELLNODE *)client);
		UNLOCK_CLIENTQ;
		terminate_one_client(client);
		LOCK_CLIENTQ;
		ellAdd((ELLLIST *)&rsrv_free_clientQ, (ELLNODE *)client);
		UNLOCK_CLIENTQ;
	} else {
		LOCK_CLIENTQ;
		while (client = (struct client *) ellGet(&clientQ))
			terminate_one_client(client);

		FASTLOCK(&rsrv_free_addrq_lck);
		ellFree(&rsrv_free_addrq);
		ellInit(&rsrv_free_addrq);
		FASTUNLOCK(&rsrv_free_addrq_lck);

		FASTLOCK(&rsrv_free_eventq_lck);
		ellFree(&rsrv_free_eventq);
		ellInit(&rsrv_free_eventq);
		FASTUNLOCK(&rsrv_free_eventq_lck);

		ellFree(&rsrv_free_clientQ);

		UNLOCK_CLIENTQ;
	}

	return OK;
}


/* 
 * TERMINATE_ONE_CLIENT
 */
LOCAL int terminate_one_client(struct client *client)
{
	FAST int        	servertid;
	FAST int        	tmpsock;
	FAST int        	status;
	FAST struct event_ext 	*pevext;
	FAST struct channel_in_use *pciu;

	if (client->proto != IPPROTO_TCP) {
		logMsg("CAS: non TCP client delete ignored\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		return ERROR;
	}

	tmpsock = client->sock;

	if(CASDEBUG>0){
		logMsg("CAS: Connection %d Terminated\n", 
			tmpsock,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	/*
	 * Server task deleted first since close() is not reentrant
	 */
	servertid = client->tid;
	if (servertid != taskIdSelf()){
		if (taskIdVerify(servertid) == OK){
			taskwdRemove(servertid);
			if (taskDelete(servertid) == ERROR) {
				printErrno(errnoGet());
			}
		}
	}

	pciu = (struct channel_in_use *) & client->addrq;
	while (pciu = (struct channel_in_use *) pciu->node.next){
		while (pevext = (struct event_ext *) ellGet((ELLLIST *)&pciu->eventq)) {

			status = db_cancel_event(
					(struct event_block *)(pevext + 1));
			if (status == ERROR)
				taskSuspend(0);
			FASTLOCK(&rsrv_free_eventq_lck);
			ellAdd((ELLLIST *)&rsrv_free_eventq, (ELLNODE *)pevext);
			FASTUNLOCK(&rsrv_free_eventq_lck);
		}
	}

	if (client->evuser) {
		status = db_close_events(client->evuser);
		if (status == ERROR)
			taskSuspend(0);
	}
	if (close(tmpsock) == ERROR)	/* close socket	 */
		logMsg("CAS: Unable to close socket\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);

	/* free dbaddr str */
	FASTLOCK(&rsrv_free_addrq_lck);
	ellConcat(
		&rsrv_free_addrq,
		&client->addrq);
	FASTUNLOCK(&rsrv_free_addrq_lck);

	if(FASTLOCKFREE(&client->lock)<0){
		logMsg("CAS: couldnt free sem\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	return OK;
}


/*
 *	client_stat()
 *
 */
int client_stat(void)
{
	int		bytes_reserved;
	struct client 	*client;


	LOCK_CLIENTQ;
	client = (struct client *) ellNext((ELLNODE *)&clientQ);
	while (client) {

		log_one_client(client);

		client = (struct client *) ellNext((ELLNODE *)client);
	}
	UNLOCK_CLIENTQ;

	if(prsrv_cast_client)
		log_one_client(prsrv_cast_client);
	
	bytes_reserved = 0;
	bytes_reserved += sizeof(struct client)*
				ellCount(&rsrv_free_clientQ);
	bytes_reserved += sizeof(struct channel_in_use)*
				ellCount(&rsrv_free_addrq);
	bytes_reserved += (sizeof(struct event_ext)+db_sizeof_event_block())*
				ellCount(&rsrv_free_eventq);
	printf(	"There are currently %d bytes on the server's free list\n",
		bytes_reserved);
	printf(	"{%d client(s), %d channel(s), and %d event(s) (monitors)}\n",
		ellCount(&rsrv_free_clientQ),
		ellCount(&rsrv_free_addrq),
		ellCount(&rsrv_free_eventq));

	return ellCount(&clientQ);
}



/*
 *	log_one_client()
 *
 */
LOCAL void log_one_client(struct client *client)
{
	struct channel_in_use	*pciu;
	struct sockaddr_in 	*psaddr;
	char			*pproto;
	unsigned long 		current;
	unsigned long 		delay;
	unsigned long		bytes_reserved;
	char			*state[] = {"up", "down"};

	if(client->proto == IPPROTO_UDP){
		pproto = "UDP";
	}
	else if(client->proto == IPPROTO_TCP){
		pproto = "TCP";
	}
	else{
		pproto = "UKN";
	}

	current = tickGet();
	if (current >= client->ticks_at_last_io) {
		delay = current - client->ticks_at_last_io;
	} 
	else {
		delay = current + (~0L - client->ticks_at_last_io);
	}

	printf(	"Socket=%d, Protocol=%s, tid=%x, secs since last interaction %d\n", 
		client->sock, 
		pproto, 
		client->tid,
		delay/sysClkRateGet());

	bytes_reserved = 0;
	bytes_reserved += sizeof(struct client);
	pciu = (struct channel_in_use *) client->addrq.node.next;
	while (pciu){
		bytes_reserved += sizeof(struct channel_in_use);
		bytes_reserved += 
			(sizeof(struct event_ext)+db_sizeof_event_block())*
				ellCount((ELLLIST *)&pciu->eventq);
		pciu = (struct channel_in_use *) ellNext((ELLNODE *)pciu);
	}



	psaddr = &client->addr;
	printf("\tRemote address %u.%u.%u.%u Remote port %d state=%s\n",
	       	(psaddr->sin_addr.s_addr & 0xff000000) >> 24,
	       	(psaddr->sin_addr.s_addr & 0x00ff0000) >> 16,
	       	(psaddr->sin_addr.s_addr & 0x0000ff00) >> 8,
	       	(psaddr->sin_addr.s_addr & 0x000000ff),
	       	psaddr->sin_port,
		state[client->disconnect?1:0]);
	printf(	"\tChannel count %d\n", ellCount(&client->addrq));
	printf(	"\t%d bytes allocated\n", bytes_reserved);

	pciu = (struct channel_in_use *) client->addrq.node.next;
	while (pciu){
		printf(	"\t%s(%d) ", 
			pciu->addr.precord,
			pciu->eventq.count);
		pciu = (struct channel_in_use *) ellNext((ELLNODE *)pciu);
	}

	printf("\n");
}
