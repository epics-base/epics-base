/*	@(#)camsgtask.c
 *   @(#)camsgtask.c	1.2	6/27/91
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
 *	.04 joh	071191	changes to recover from UDP port reuse
 *			by rebooted clients
 */

#include <vxWorks.h>
#include <lstLib.h>
#include <types.h>
#include <socket.h>
#include <ioLib.h>
#include <in.h>
#include <tcp.h>
#include <errno.h>
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


	/*
 	 * performed in two steps purely for 
	 * historical reasons
	 */
	client = (struct client *) create_udp_client(NULL);
	udp_to_tcp(client, sock);
	if (!client) {
		logMsg("camsgtask: client init failed\n");
		close(sock);
		return;
	}

	i = sizeof(client->addr);
	status = getpeername(
			sock,
 			&client->addr, 
			&i); 
    	if(status == ERROR){
      		logMsg("camsgtask: peer address fetch failed\n");
      		close(sock);
      		return;
    	}
				
  	if(CASDEBUG>0){
    		logMsg(	"camsgtask: Recieved connection request\n");
   		logMsg("from addr %x, port %x \n", 
			client->addr.sin_addr, 
			client->addr.sin_port);
  	}

	LOCK_CLIENTQ;
	lstAdd(&clientQ, client);
	UNLOCK_CLIENTQ;

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
		if (nchars==0){
			logMsg("camsgtask: nill message disconnect\n");
			break;
		}
		else if(nchars<=0){
			long	anerrno;

			anerrno = errnoGet(taskIdSelf());

			/*
			 * normal conn lost conditions
			 */
			if(CASDEBUG==0){
				if(anerrno==ECONNABORTED||anerrno==ECONNRESET){
					break;
				}
			}

			logMsg("camsgtask: Exiting after msg recv error\n");
			printErrno(anerrno);

			break;
		}

		client->ticks_at_last_io = tickGet();
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
			cas_send_msg(client, TRUE);
		}
		if (nchars == 0){
			cas_send_msg(client, TRUE);
		}
	
	}
	
	free_client(client);
}

