/*	@(#)caservertask.c
 *   @(#)caservertask.c	1.2	6/27/91
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
 */

#include <vxWorks.h>
#include <lstLib.h>
#include <taskLib.h>
#include <types.h>
#include <socket.h>
#include <in.h>
#include <db_access.h>
#include <task_params.h>
#include <server.h>


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
void 
req_server()
{
	struct sockaddr_in 	serverAddr;	/* server's address */
	FAST struct client 	*client;
	FAST int        	status;
	FAST int        	i;

	if (IOC_sock != 0 && IOC_sock != ERROR)
		if ((status = close(IOC_sock)) == ERROR)
			logMsg("CAS: Unable to close open master socket\n");

	/*
	 * Open the socket. Use ARPA Internet address format and stream
	 * sockets. Format described in <sys/socket.h>.
	 */
	if ((IOC_sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		logMsg("CAS: Socket creation error\n");
		taskSuspend(0);
	}
	
	/* Zero the sock_addr structure */
	bfill(&serverAddr, sizeof(serverAddr), 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = CA_SERVER_PORT;

	/* get server's Internet address */
	if (bind(IOC_sock, &serverAddr, sizeof(serverAddr)) == ERROR) {
		logMsg("CAS: Bind error\n");
		close(IOC_sock);
		taskSuspend(0);
	}

	/* listen and accept new connections */
	if (listen(IOC_sock, 10) == ERROR) {
		logMsg("CAS: Listen error\n");
		close(IOC_sock);
		taskSuspend(0);
	}

	while (TRUE) {
		if ((i = accept(IOC_sock, NULL, 0)) == ERROR) {
			logMsg("CAS: Accept error\n");
			taskSuspend(0);
		} else {
			status = taskSpawn(CA_CLIENT_NAME,
					   CA_CLIENT_PRI,
					   CA_CLIENT_OPT,
					   CA_CLIENT_STACK,
					   camsgtask,
					   i);
			if (status == ERROR) {
				logMsg("CAS: task creation failed\n");
				logMsg("CAS: (client ignored)\n");
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
STATUS
free_client(client)
register struct client *client;
{
	if (client) {
		/* remove it from the list of clients */
		/* list delete returns no status */
		LOCK_CLIENTQ;
		lstDelete(&clientQ, client);
		UNLOCK_CLIENTQ;
		terminate_one_client(client);
		LOCK_CLIENTQ;
		lstAdd(&rsrv_free_clientQ, client);
		UNLOCK_CLIENTQ;
	} else {
		LOCK_CLIENTQ;
		while (client = (struct client *) lstGet(&clientQ))
			terminate_one_client(client);

		FASTLOCK(&rsrv_free_addrq_lck);
		lstFree(&rsrv_free_addrq);
		lstInit(&rsrv_free_addrq);
		FASTUNLOCK(&rsrv_free_addrq_lck);

		FASTLOCK(&rsrv_free_eventq_lck);
		lstFree(&rsrv_free_eventq);
		lstInit(&rsrv_free_eventq);
		FASTUNLOCK(&rsrv_free_eventq_lck);

		lstFree(&rsrv_free_clientQ);

		UNLOCK_CLIENTQ;
	}
}


/* 
 * TERMINATE_ONE_CLIENT
 */
int 
terminate_one_client(client)
register struct client *client;
{
	FAST int        servertid = client->tid;
	FAST int        tmpsock = client->sock;
	FAST int        status;
	FAST struct event_ext *pevext;
	FAST struct channel_in_use *pciu;

	if (client->proto == IPPROTO_TCP) {

		if(CASDEBUG>0){
			logMsg("CAS: Connection %d Terminated\n", tmpsock);
		}

		/*
		 * Server task deleted first since close() is not reentrant
		 */
		if (servertid != taskIdSelf() && servertid){
			if (taskIdVerify(servertid) == OK){
				if (taskDelete(servertid) == ERROR) {
					printErrno(errnoGet());
				}
			}
		}

		pciu = (struct channel_in_use *) & client->addrq;
		while (pciu = (struct channel_in_use *) pciu->node.next)
			while (pevext = (struct event_ext *) lstGet(&pciu->eventq)) {

				status = db_cancel_event(pevext + 1);
				if (status == ERROR)
					taskSuspend(0);
				FASTLOCK(&rsrv_free_eventq_lck);
				lstAdd(&rsrv_free_eventq, pevext);
				FASTUNLOCK(&rsrv_free_eventq_lck);
			}

		if (client->evuser) {
			status = db_close_events(client->evuser);
			if (status == ERROR)
				taskSuspend(0);
		}
		if (tmpsock != NONE)
			if ((status = close(tmpsock)) == ERROR)	/* close socket	 */
				logMsg("CAS: Unable to close socket\n");
	}

	/* free dbaddr str */
	FASTLOCK(&rsrv_free_addrq_lck);
	lstConcat(
		&rsrv_free_addrq,
		&client->addrq);
	FASTUNLOCK(&rsrv_free_addrq_lck);

	if(FASTLOCKFREE(&client->lock)<0){
		logMsg("CAS: couldnt free sem\n");
	}

	return OK;
}


/*
 *	client_stat()
 *
 */
STATUS
client_stat()
{
	struct client *client;


	LOCK_CLIENTQ;
	client = (struct client *) lstNext(&clientQ);
	while (client) {

		log_one_client(client);

		client = (struct client *) lstNext(client);
	}
	UNLOCK_CLIENTQ;

	if(prsrv_cast_client)
		log_one_client(prsrv_cast_client);
	
	return lstCount(&clientQ);
}



/*
 *	log_one_client()
 *
 */
static void 
log_one_client(client)
struct client *client;
{
	NODE          		*addr;
	struct sockaddr_in 	*psaddr;
	char			*pproto;
	unsigned long 		current;
	unsigned long 		delay;
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

	psaddr = &client->addr;
	printf("\tRemote address %u.%u.%u.%u Remote port %d state=%s\n",
	       	(psaddr->sin_addr.s_addr & 0xff000000) >> 24,
	       	(psaddr->sin_addr.s_addr & 0x00ff0000) >> 16,
	       	(psaddr->sin_addr.s_addr & 0x0000ff00) >> 8,
	       	(psaddr->sin_addr.s_addr & 0x000000ff),
	       	psaddr->sin_port,
		state[client->disconnect?1:0]);
	printf("\tChannel count %d\n", lstCount(&client->addrq));
	addr = (NODE *) & client->addrq;
	while (addr = lstNext(addr))
		printf("\t%s ", ((struct db_addr *) (addr + 1))->precord);
	printf("\n");
}
