/*	@(#)camsgtask.c
 *   $Id$
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	6-88
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
 *	.01 joh 08--88 	added broadcast switchover to TCP/IP
 *	.02 joh 041089 	added new event passing
 *	.03 joh 060791	camsgtask() now returns info about 
 *			partial messages
 */

#include <vxWorks.h>
#include <lstLib.h>
#include <types.h>
#include <socket.h>
#include <ioLib.h>
#include <in.h>
#include <tcp.h>
#include <task_params.h>
#include <db_access.h>
#include <server.h>


/*
 *
 *	camsgtask()
 *
 *	CA server TCP client task (one spawned for each client)
 */
void camsgtask(sock)
FAST int 		sock;
{
  	int 			nchars;
  	FAST int		status;
  	FAST struct client 	*client = NULL;
  	struct sockaddr_in	addr;
  	int			i;
    	int 			true = TRUE;


	/*
	 * see TCP(4P) this seems to make unsollicited single events much
	 * faster. I take care of queue up as load increases.
	 */
    	status = setsockopt(
				sock,
				IPPROTO_TCP,
				TCP_NODELAY,
				&true,
				sizeof true);
    	if(status == ERROR){
      		logMsg("camsgtask: TCP_NODELAY option set failed\n");
      		close(sock);
      		return;
    	}


	/* 
	 * turn on KEEPALIVE so if the client crashes
  	 * this task will find out and exit
	 */
	status = setsockopt(
			sock, 
			SOL_SOCKET, 
			SO_KEEPALIVE,
		    	&true, 
			sizeof true);
    	if(status == ERROR){
      		logMsg("camsgtask: SO_KEEPALIVE option set failed\n");
      		close(sock);
      		return;
    	}

  	nchars = recv(	sock,
 			&addr, 
			sizeof(addr), 
			0);
  	if ( nchars <  sizeof(addr) ){
    		logMsg("camsgtask: Protocol error\n");
    		close(sock);
    		return;
  	}

  	if(MPDEBUG==2){
    		logMsg(	"camsgtask: Recieved connection request\n");
   		logMsg("from addr %x, udp port %x \n", 
			addr.sin_addr, 
			addr.sin_port);
  	}

	/*
	 * NOTE: The client structure used here does not have to be locked
	 * since this thread does not traverse the addr que or the client
	 * que. This thread is deleted prior to deletion of this client by
	 * terminate_one_client().
	 * 
	 * wait a reasonable length of time in case the cast server is busy
	 */
	for (i = 0; i < 10; i++) {
		LOCK_CLIENTQ;
		client = existing_client(&addr);
		UNLOCK_CLIENTQ;
	
		if (client)
			break;
	
		taskDelay(sysClkRateGet() * 5);	/* 5 sec */
	}

	if (!client) {
		logMsg("camsgtask: Unknown client (protocol error)\n");
		close(sock);
		return;
	}

	/*
	 * convert connection to TCP
	 */
	LOCK_SEND(client);
	udp_to_tcp(client, sock);
	UNLOCK_SEND(client);
	
	client->tid = taskIdSelf();
	
	client->evuser = (struct event_user *) db_init_events();
	if (!client->evuser) {
		logMsg("camsgtask: unable to init the event facility\n");
		free_client(client);
		return;
	}
	status = db_start_events(
			client->evuser, 
			CA_EVENT_NAME, 
			NULL, 
			NULL);
	if (status == ERROR) {
		logMsg("camsgtask: unable to start the event facility\n");
		free_client(client);
		return;
	}

	client->recv.cnt = 0;
	while (TRUE) {
		client->recv.stk = 0;
			
		nchars = recv(	
				sock, 
				&client->recv.buf[client->recv.cnt], 
				sizeof(client->recv.buf)-client->recv.cnt, 
				0);
		if(nchars<=0){
			if(MPDEBUG>0){
				logMsg("CA server: msg recv error\n");
				printErrno(errnoGet(taskIdSelf()));
			}
			break;
		}

		client->recv.cnt += nchars;
		status = camessage(client, &client->recv);
		if(status == OK){
			unsigned bytes_left;

			bytes_left = client->recv.cnt - client->recv.stk;

			/*
			 * if there is a partial message
			 * align it with the start of the buffer
			 */
			if(bytes_left>0){
				char *pbuf;

				pbuf = client->recv.buf;

				/*
				 * overlapping regions handled
				 * by bcopy
				 */
				bcopy(	pbuf + client->recv.stk,
					pbuf,
					bytes_left);
				client->recv.cnt = bytes_left;
			}
			else{
				client->recv.cnt = 0;
			}
		}else{
			client->recv.cnt = 0;
		}

	
		/*
		 * allow message to batch up if more are comming
		 */
		status = ioctl(sock, FIONREAD, &nchars);
		if (status == ERROR) {
			printErrno(errnoGet(taskIdSelf()));
			taskSuspend(0);
		}
		if (nchars == 0)
			cas_send_msg(client, TRUE);
	
		/*
		 * dont hang around if there are no 
		 * connections to process variables
		 */
		if (client->addrq.count == 0)
			break;
	}
	
	free_client(client);
}


/*
 *
 *	read_entire_msg()
 *
 *
 */
static int read_entire_msg(sockfd, mp, tot)
FAST sockfd;			/* socket file descriptor */
FAST unsigned char *mp;		/* where to read pointer */
FAST unsigned int tot;		/* size of entire message */
{
	FAST unsigned int 	nchars;
	FAST unsigned int 	total = 0;
	int			status;

	while(TRUE) {
		nchars = recv(	sockfd, 
				mp + total,
				tot - total, 
				0);
		if(nchars <= 0){
			if(MPDEBUG == 2) {
				logMsg("rsrv: msg recv error\n");
				printErrno(errnoGet(taskIdSelf()));
			}
			return ERROR;
		}

		total += nchars;
		if(total == tot)
			return total;
		else if(total > tot)
			return ERROR;
	}
}
