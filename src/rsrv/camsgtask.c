/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	6-88
 *
 */

static char *sccsId = "@(#) $Id$";

#include <vxWorks.h>
#include <types.h>
#include <socket.h>
#include <sockLib.h>
#include <ioLib.h>
#include <in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <logLib.h>
#include <errnoLib.h>
#include <tickLib.h>
#include <string.h>
#include <taskLib.h>

#include "ellLib.h"
#include "taskwd.h"
#include "task_params.h"
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
FAST int 		sock;
{
  	int 			nchars;
  	FAST int		status;
	FAST struct client 	*client;
    	int 			true = TRUE;

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
      		logMsg("CAS: TCP_NODELAY option set failed\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
      		close(sock);
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
      		logMsg("CAS: SO_KEEPALIVE option set failed\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
      		close(sock);
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
		logMsg("CAS: SO_SNDBUF set failed\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		close(sock);
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
		logMsg("CAS: SO_RCVBUF set failed\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		close(sock);
		return ERROR;
	}
#endif

	/*
 	 * performed in two steps purely for 
	 * historical reasons
	 */
	client = (struct client *) create_udp_client(NULL);
	if (!client) {
		logMsg("CAS: client init failed\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		close(sock);
		return ERROR;
	}

	taskwdInsert( 	(int)taskIdCurrent,
			NULL,
			NULL);

	status = udp_to_tcp(client, sock);
	if(status<0){
		logMsg("CAS: TCP convert failed\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		free_client(client);
      		return ERROR;
	}

  	if(CASDEBUG>0){
		char buf[64];
 		ipAddrToA (&client->addr, buf, sizeof(buf));
    	logMsg(	"CAS: conn req from %s\n",
			(int) /* sic */ buf,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
  	}

	client->evuser = (struct event_user *) db_init_events();
	if (!client->evuser) {
		logMsg("CAS: unable to init the event facility\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		free_client(client);
		return ERROR;
	}
	status = db_add_extra_labor_event(
			client->evuser,
			write_notify_reply,
			client);
	if(status == ERROR){
		logMsg("CAS: unable to setup the event facility\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		free_client(client);
		return ERROR;
	}
	status = db_start_events(
			client->evuser, 
			CA_EVENT_NAME, 
			NULL, 
			NULL,
			1);	/* one priority notch lower */
	if (status == ERROR) {
		logMsg("CAS: unable to start the event facility\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		free_client(client);
		return ERROR;
	}
	
	LOCK_CLIENTQ;
	ellAdd(&clientQ, &client->node);
	UNLOCK_CLIENTQ;

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
				logMsg("CAS: nill message disconnect\n",
					NULL,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
			}
			break;
		}
		else if(nchars<0){
			long	anerrno;

			anerrno = errnoGet();

			/*
			 * normal conn lost conditions
			 */
                        if(     (anerrno!=ECONNABORTED&&
                                anerrno!=ECONNRESET&&
                                anerrno!=ETIMEDOUT)||
                                CASDEBUG>2){

                                logMsg(
                                	"CAS: client disconnect(errno=%d)\n",
                                	anerrno,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
                        }

			break;
		}

		client->ticks_at_last_recv = tickGet();
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
            logMsg ("CAS: forcing disconnect from %s\n",
	            /* sic */ (int) buf, NULL, NULL,
	            NULL, NULL, NULL);

			break;
		}

		/*
		 * allow message to batch up if more are comming
		 */
		status = ioctl(sock, FIONREAD, (int) &nchars);
		if (status < 0) {
			logMsg("CAS: io ctl err %d\n",
				errnoGet(),
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			cas_send_msg(client, TRUE);
		}
		else if (nchars == 0){
			cas_send_msg(client, TRUE);
		}
	}
	
	LOCK_CLIENTQ;
	ellDelete(&clientQ, &client->node);
	UNLOCK_CLIENTQ;
	
	free_client(client);

	return OK;
}
