/*
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
 *	.05 joh	110691	print nil recv disconnect message only
 *			if debug is on
 *	.06 joh 021192  better diagnostics
 *	.07 joh 031692  disconnect on bad message
 *	.08 joh 111892	set TCP buffer size to be synergistic
 *			with CA buffer size
 *	.09 joh	111992	moved the event tasks prioity down
 *			(added new arg to db_start_events())
 */

static char *sccsId = "@(#) $Id$";

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "osiSock.h"
#include "tsStamp.h"
#include "os_depen.h"
#include "osiThread.h"
#include "errlog.h"
#include "ellLib.h"
#include "taskwd.h"
#include "db_access.h"
#include "server.h"
#include "bsdSocketResource.h"


/*
 *
 *	camsgtask()
 *
 *	CA server TCP client task (one spawned for each client)
 */
int camsgtask(sock)
SOCKET 		sock;
{
  	int 		nchars;
  	int		status;
	struct client 	*client;
    	int 		true = TRUE;

	client = NULL;

	/*
	 * see TCP(4P) this seems to make unsollicited single events much
	 * faster. I take care of queue up as load increases.
	 */
    	status = setsockopt(
				sock,
				IPPROTO_TCP,
				TCP_NODELAY,
				(char *)&true,
				sizeof(true));
    	if(status == ERROR){
      		errlogPrintf("CAS: TCP_NODELAY option set failed\n");
      		socket_close(sock);
      		return ERROR;
    	}


	/* 
	 * turn on KEEPALIVE so if the client crashes
  	 * this task will find out and exit
	 */
	status = setsockopt(
			sock, 
			SOL_SOCKET, 
			SO_KEEPALIVE,
		    	(char *)&true, 
			sizeof(true));
    	if(status == ERROR){
      		errlogPrintf("CAS: SO_KEEPALIVE option set failed\n");
      		socket_close(sock);
      		return ERROR;
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
	status = setsockopt(
			sock,
			SOL_SOCKET,
			SO_SNDBUF,
			(char *)&i,
			sizeof(i));
	if(status < 0){
		errlogPrintf("CAS: SO_SNDBUF set failed\n");
		socket_close(sock);
		return ERROR;
	}
	i = MAX_MSG_SIZE;
	status = setsockopt(
			sock,
			SOL_SOCKET,
			SO_RCVBUF,
			(char *)&i,
			sizeof(i));
	if(status < 0){
		errlogPrintf("CAS: SO_RCVBUF set failed\n");
		socket_close(sock);
		return ERROR;
	}
#endif

	/*
 	 * performed in two steps purely for 
	 * historical reasons
	 */
	client = (struct client *) create_udp_client(NULL);
	if (!client) {
		errlogPrintf("CAS: client init failed\n");
		socket_close(sock);
		return ERROR;
	}

	taskwdInsert( 	threadGetIdSelf(),
			NULL,
			NULL);

	status = udp_to_tcp(client, sock);
	if(status<0){
		errlogPrintf("CAS: TCP convert failed\n");
		free_client(client);
      		return ERROR;
	}

  	if(CASDEBUG>0){
		char buf[64];
 		ipAddrToA (&client->addr, buf, sizeof(buf));
    	errlogPrintf(	"CAS: conn req from %s\n",
			(int) /* sic */ buf);
  	}

	LOCK_CLIENTQ;
	ellAdd(&clientQ, &client->node);
	UNLOCK_CLIENTQ;

	client->evuser = (struct event_user *) db_init_events();
	if (!client->evuser) {
		errlogPrintf("CAS: unable to init the event facility\n");
		free_client(client);
		return ERROR;
	}
	status = db_add_extra_labor_event(
			client->evuser,
			write_notify_reply,
			client);
	if(status == ERROR){
		errlogPrintf("CAS: unable to setup the event facility\n");
		free_client(client);
		return ERROR;
	}
	status = db_start_events(
			client->evuser, 
			"CAevent",
			NULL, 
			NULL,
			1);	/* one priority notch lower */
	if (status == ERROR) {
		errlogPrintf("CAS: unable to start the event facility\n");
		free_client(client);
		return ERROR;
	}

	client->recv.cnt = 0ul;
	while (TRUE) {
		client->recv.stk = 0;
			
		nchars = recv(	
				sock, 
				&client->recv.buf[client->recv.cnt], 
				(int)(sizeof(client->recv.buf)-client->recv.cnt), 
				0);
		if (nchars==0){
  			if(CASDEBUG>0){
				errlogPrintf("CAS: nill message disconnect\n");
			}
			break;
		}
		else if(nchars<0){
			long	anerrno;

			anerrno = SOCKERRNO;

			/*
			 * normal conn lost conditions
			 */
                        if(     (anerrno!=ECONNABORTED&&
                                anerrno!=ECONNRESET&&
                                anerrno!=ETIMEDOUT)||
                                CASDEBUG>2){

                                errlogPrintf(
                                	"CAS: client disconnect(errno=%d)\n",
                                	anerrno);
                        }

			break;
		}

		tsStampGetCurrent(&client->time_at_last_recv);
		client->recv.cnt += (unsigned long) nchars;

		status = camessage(client, &client->recv);
		if(status == OK){
			/*
			 * if there is a partial message
			 * align it with the start of the buffer
			 */
			if (client->recv.cnt >= client->recv.stk) {
				unsigned bytes_left;
				char *pbuf;

				bytes_left = client->recv.cnt - client->recv.stk;

				pbuf = client->recv.buf;

				/*
				 * overlapping regions handled
				 * by memmove 
				 */
				memmove(pbuf,
					pbuf + client->recv.stk,
					bytes_left);
				client->recv.cnt = bytes_left;
			}
			else{
				client->recv.cnt = 0ul;
			}
		}else{
            char buf[64];
 
			client->recv.cnt = 0ul;

			/*
			 * disconnect when there are severe message errors
			 */
            ipAddrToA (&client->addr, buf, sizeof(buf));
            errlogPrintf ("CAS: forcing disconnect from %s\n",
	            /* sic */ (int) buf);
			break;
		}

		/*
		 * allow message to batch up if more are comming
		 */
		status = socket_ioctl(sock, FIONREAD, (int) &nchars);
		if (status < 0) {
			errlogPrintf("CAS: io ctl err %d\n",
				SOCKERRNO);
			cas_send_msg(client, TRUE);
		}
		else if (nchars == 0){
			cas_send_msg(client, TRUE);
		}
	}
	
	free_client(client);

	return OK;
}
