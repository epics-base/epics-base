/*	@(#)cast_server.c
 *   $Id$
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
 *	.00 joh	030191	Fixed cast server to not block on TCP
 *	.01 joh	030891	now reuses old client structure
 *	.02 joh	032091	allways flushes if the client changes and the old
 *			client has a TCP connection.(bug introduced by .00)
 *
 *	Improvements
 *	------------
 *	Dont send channel found message unless there is memory, a task slot,
 *	and a TCP socket available. Send a diagnostic instead. 
 *	Or ... make the timeout shorter? This is only a problem if
 *	they persist in trying to make a connection after getting no
 *	response.
 */

#include <vxWorks.h>
#include <lstLib.h>
#include <taskLib.h>
#include <types.h>
#include <socket.h>
#include <ioLib.h>
#include <in.h>
#include <db_access.h>
#include <task_params.h>
#include <server.h>

	

/*
 * CAST_SERVER
 *
 * service UDP messages
 * 
 */
void 
cast_server()
{
  	struct sockaddr_in		sin;	
  	FAST int			status;
  	int				count;
  	FAST struct client		*client = NULL;
  	FAST struct client		*true_client = NULL;
  	FAST struct client		*temp_client;
  	struct sockaddr_in		recv_addr;
  	int  				recv_addr_size = sizeof(recv_addr);
  	unsigned			nchars;
# 	define				TIMEOUT	60 /* sec */
  	unsigned long			timeout = TIMEOUT*sysClkRateGet();
 	static struct message_buffer  	udp_msg;

  	struct client 			*existing_client();
  	struct client 			*create_udp_client();
  	void		 		rsrv_online_notify_task();

  	if( IOC_cast_sock!=0 && IOC_cast_sock!=ERROR )
    		if( (status = close(IOC_cast_sock)) == ERROR )
      			logMsg("Unable to close open master socket\n");

  	/* 
  	 *  Open the socket.
 	 *  Use ARPA Internet address format and datagram socket.
 	 *  Format described in <sys/socket.h>.
 	 */

  	if((IOC_cast_sock = socket (AF_INET, SOCK_DGRAM, 0)) == ERROR){
    		logMsg("Socket creation error\n");
    		printErrno (errnoGet ());
    		taskSuspend(0);
  	}


  	/*  Zero the sock_addr structure */
  	bfill(&sin, sizeof(sin), 0);
  	sin.sin_family = AF_INET;
  	sin.sin_addr.s_addr = INADDR_ANY;
  	sin.sin_port = CA_SERVER_PORT;

	
  	/* get server's Internet address */
  	if( bind(IOC_cast_sock, &sin, sizeof (sin)) == ERROR){
    		logMsg("Bind error\n");
    		printErrno (errnoGet ());
    		close (IOC_cast_sock);
    		taskSuspend(0);
  	}


  	bfill(&udp_msg, sizeof(udp_msg), NULL);

  	/* tell clients we are on line again */
  	status = taskSpawn(
		CA_ONLINE_NAME,
		CA_ONLINE_PRI,
		CA_ONLINE_OPT,
		CA_ONLINE_STACK,
		rsrv_online_notify_task);
  	if(status<0){
    		logMsg("Cast_server: couldnt start up online notify task\n");
    		printErrno(errnoGet ());
  	}


  	while(TRUE){

		/*
		 * setup new client structure but reuse old structure if
		 * possible
		 *
		 */
		if(!client){
       	 		client = create_udp_client(IOC_cast_sock);
      			if(!client){
				taskDelay(sysClkRateGet()*60*5);
       	 			continue;
      			}
    		}

    		status = recvfrom(
			IOC_cast_sock,
			udp_msg.buf,
			sizeof(udp_msg.buf),
			NULL,
			&recv_addr, 
			&recv_addr_size);
    		if(status<0){
      			logMsg("Cast_server: UDP recv error\n");
      			printErrno(errnoGet ());
     	 		taskSuspend(0);
    		}

		udp_msg.cnt = status;
		udp_msg.stk = 0;

		/*
		 * If we are talking to a new client flush the old one 
		 * in case it is holding UDP messages for a 
		 * TCP connected client - waiting to see if the
		 * next message is for this same client.
		 * (replies to broadcasts are not returned over
		 * an existing TCP connection to avoid a TCP
		 * pend which could lock up the cast server).
		 */
		if(	client->valid_addr &&
			(client->addr.sin_addr.s_addr != 
				recv_addr.sin_addr.s_addr ||
			client->addr.sin_port != recv_addr.sin_port)){
			cas_send_msg(client, TRUE);
		}
      		client->addr = recv_addr;
		client->valid_addr = TRUE;

    		if(MPDEBUG==2){
       			logMsg(	"cast_server(): msg of %d bytes\n", 
				status);
       			logMsg(	"from addr %x, port %x \n", 
				recv_addr.sin_addr, 
				recv_addr.sin_port);
    		}

    		if(MPDEBUG==3)
      			count = client->addrq.count;

		/*
		 * check for existing client occurs after
		 * message process so that this thread never
		 * blocks sending TCP
		 */
    		status = camessage(client, &udp_msg);
		if(status == OK){
			if(udp_msg.cnt != udp_msg.stk){
				printf(	"CA UDP Message OF %d\n",
					udp_msg.cnt-udp_msg.stk);
			}
		}

		if(client->addrq.count){
    			LOCK_CLIENTQ;

    			true_client = existing_client(&client->addr);
			if(true_client == NULL){
      				client->ticks_at_creation = tickGet();
      				lstAdd(&clientQ, client);
				client = NULL;
			}else{
				lstConcat(
					&true_client->addrq,
					&client->addrq);
			}

    			UNLOCK_CLIENTQ;

      			if(MPDEBUG==3){
        			logMsg(	"Fnd %d name matches (%d tot)\n",
					client->addrq.count-count,
					client->addrq.count);
			}
		}

		/*
		 * allow message to batch up if more are comming
		 */
 	   	status = ioctl(IOC_cast_sock, FIONREAD, &nchars);
 	   	if(status == ERROR){
  	    		printErrno(errnoGet(0));
  	    		taskSuspend(0);
 	   	}

    		if(nchars == 0){
			if(client)
				cas_send_msg(client, TRUE);

			/*
			 * catch any that have not been sent yet
			 */
    			LOCK_CLIENTQ;
      			temp_client = (struct client *) &clientQ;
      			while(temp_client = (struct client *) 
					temp_client->node.next){
				if(temp_client->proto == IPPROTO_UDP)
          				cas_send_msg(temp_client, TRUE);
      			}

      			clean_clientq(timeout);
    			UNLOCK_CLIENTQ;
    		}	
  	}
}




/*
 * CREATE_UDP_CLIENT
 *
 * 
 */
struct client *create_udp_client(sock)
unsigned sock;
{
  	struct client *client;

    	LOCK_CLIENTQ;
	client = (struct client *)lstGet(&rsrv_free_clientQ);
    	UNLOCK_CLIENTQ;

	if(!client){
      		client = (struct client *)malloc(sizeof(struct client));
      		if(!client){
			logMsg("CA: no mem for another client\n");
        		printErrno(errnoGet ());
        		return NULL;
      		}
	}

      	if(MPDEBUG==3)
        	logMsg(	"cast_server(): Creating new udp client\n");

	/*    
      	The following inits to zero done instead of a bfill since
      	the send and recv buffers are large and don't need initialization.

      	bfill(client, sizeof(*client), NULL);
	*/     
      	lstInit(&client->addrq);
      	client->tid = 0;
      	client->send.stk = 0;
      	client->send.cnt = 0;
      	client->recv.stk = 0;
      	client->recv.cnt = 0;
      	client->evuser = NULL;
      	client->eventsoff = FALSE;
	client->valid_addr = FALSE;
	client->disconnect = FALSE;	/* for TCP only */

      	client->proto = IPPROTO_UDP;
      	client->send.maxstk = MAX_UDP-sizeof(client->recv.cnt);
      	FASTLOCKINIT(&client->send.lock);

      	client->recv.maxstk = MAX_UDP;
      	FASTLOCKINIT(&client->recv.lock);

      	client->sock = sock;

      	return client;
}



/*
 * UDP_TO_TCP
 *
 * send lock must be applied
 * 
 */
int udp_to_tcp(client,sock)
struct client 	*client;
unsigned	sock;
{

  	if(MPDEBUG==3)
    		logMsg("cast_server(): converting udp client to tcp\n");

  	client->proto 		= IPPROTO_TCP;
  	client->send.maxstk 	= MAX_TCP;
  	client->recv.maxstk 	= MAX_TCP;
  	client->sock 		= sock;

  	return OK;
}


