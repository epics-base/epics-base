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
 *	.00 joh	030191	Fixed cast server to not block on TCP
 *	.01 joh	030891	now reuses old client structure
 *	.02 joh	032091	allways flushes if the client changes and the old
 *			client has a TCP connection.(bug introduced by .00)
 *	.03 joh 071291	changes to avoid confusion when a rebooted
 *			client uses the same port number
 *	.04 joh 080591	changed printf() to a logMsg()
 *	.05 joh 082091	tick stamp init in create_udp_client()
 *	.06 joh 112291	dont change the address until after the flush
 *	.07 joh 112291	fixed the comments
 *      .08 joh 021192  better diagnostics
 *
 *	Improvements
 *	------------
 *	.01
 *	Dont send channel found message unless there is memory, a task slot,
 *	and a TCP socket available. Send a diagnostic instead. 
 *	Or ... make the timeout shorter? This is only a problem if
 *	they persist in trying to make a connection after getting no
 *	response.
 *
 *	Notes:
 *	------
 *	.01
 * 	Replies to broadcasts are not returned over
 * 	an existing TCP connection to avoid a TCP
 * 	pend which could lock up the cast server.
 */

static char	*sccsId = "$Id$\t$Date$";

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

	
void 	clean_addrq();



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
	struct sockaddr_in		new_recv_addr;
  	int  				recv_addr_size;
  	unsigned			nchars;

  	recv_addr_size = sizeof(new_recv_addr);

  	if( IOC_cast_sock!=0 && IOC_cast_sock!=ERROR )
    		if( (status = close(IOC_cast_sock)) == ERROR )
      			logMsg("CAS: Unable to close master cast socket\n");

  	/* 
  	 *  Open the socket.
 	 *  Use ARPA Internet address format and datagram socket.
 	 *  Format described in <sys/socket.h>.
 	 */

  	if((IOC_cast_sock = socket (AF_INET, SOCK_DGRAM, 0)) == ERROR){
    		logMsg("CAS: casts socket creation error\n");
    		taskSuspend(0);
  	}


  	/*  Zero the sock_addr structure */
  	bfill(&sin, sizeof(sin), 0);
  	sin.sin_family = AF_INET;
  	sin.sin_addr.s_addr = INADDR_ANY;
  	sin.sin_port = CA_SERVER_PORT;

	
  	/* get server's Internet address */
  	if( bind(IOC_cast_sock, &sin, sizeof (sin)) == ERROR){
    		logMsg("CAS: cast bind error\n");
    		close (IOC_cast_sock);
    		taskSuspend(0);
  	}

  	/* tell clients we are on line again */
  	status = taskSpawn(
		CA_ONLINE_NAME,
		CA_ONLINE_PRI,
		CA_ONLINE_OPT,
		CA_ONLINE_STACK,
		rsrv_online_notify_task);
  	if(status<0){
    		logMsg("CAS: couldnt start up online notify task\n");
    		printErrno(errnoGet ());
  	}


	/*
	 * setup new client structure but reuse old structure if
	 * possible
	 *
	 */
	while(TRUE){
       	 	prsrv_cast_client = create_udp_client(IOC_cast_sock);
      		if(prsrv_cast_client){
       	 		break;
      		}
		taskDelay(sysClkRateGet()*60*5);
    	}


  	while(TRUE){

    		status = recvfrom(
			IOC_cast_sock,
			prsrv_cast_client->recv.buf,
			sizeof(prsrv_cast_client->recv.buf),
			NULL,
			&new_recv_addr, 
			&recv_addr_size);
    		if(status<0){
      			logMsg("CAS: UDP recv error (errno=%d)\n",
				errnoGet(taskIdSelf()));
     	 		taskSuspend(0);
    		}

		prsrv_cast_client->recv.cnt = status;
		prsrv_cast_client->recv.stk = 0;
		prsrv_cast_client->ticks_at_last_io = tickGet();
/*
 *
 *	keeping an eye on the socket library
 *
 */
if(sizeof(prsrv_cast_client->addr) != recv_addr_size){
	printf("cast server: addr size has changed?\n");
}

		/*
		 * If we are talking to a new client flush the old one 
		 * in case it is holding UDP messages waiting to 
		 * see if the next message is for this same client.
		 */
		status = bcmp(
				&prsrv_cast_client->addr, 
				&new_recv_addr, 
				recv_addr_size);
		if(status){ 	
			/* 
			 * if the address is different 
			 */
			cas_send_msg(prsrv_cast_client, TRUE);
  			prsrv_cast_client->addr = new_recv_addr;
		}

    		if(CASDEBUG>1){
       			logMsg(	"CAS: cast server msg of %d bytes\n", 
				prsrv_cast_client->recv.cnt);
       			logMsg(	"CAS: from addr %x, port %x \n", 
				prsrv_cast_client->addr.sin_addr, 
				prsrv_cast_client->addr.sin_port);
    		}

    		if(CASDEBUG>2)
      			count = prsrv_cast_client->addrq.count;

    		status = camessage(
				prsrv_cast_client, 
				&prsrv_cast_client->recv);
		if(status == OK){
			if(prsrv_cast_client->recv.cnt != 
				prsrv_cast_client->recv.stk){

				logMsg(	"CAS: partial UDP msg of %d bytes ?\n",
					prsrv_cast_client->recv.cnt-
						prsrv_cast_client->recv.stk);
			}
		}

		if(prsrv_cast_client->addrq.count){
      			if(CASDEBUG>2){
        			logMsg(	"CAS: Fnd %d name matches (%d tot)\n",
					prsrv_cast_client->addrq.count-count,
					prsrv_cast_client->addrq.count);
			}
		}

		/*
		 * allow message to batch up if more are comming
		 */
 	   	status = ioctl(IOC_cast_sock, FIONREAD, &nchars);
 	   	if(status == ERROR){
  	    		taskSuspend(0);
 	   	}

    		if(nchars == 0){
			cas_send_msg(prsrv_cast_client, TRUE);
	      		clean_addrq(prsrv_cast_client);
    		}	
  	}
}


/*
 * clean_addrq
 *
 * 
 */
#define		TIMEOUT	60 /* sec */

static void 
clean_addrq(pclient)
struct client *pclient;
{
	struct channel_in_use	*pciu;
	struct channel_in_use	*pnextciu;
	FAST unsigned long 	current = tickGet();
	unsigned long   	delay;
	unsigned long   	maxdelay = 0;
	unsigned		ndelete=0;
  	unsigned long		timeout = TIMEOUT*sysClkRateGet();

	current = tickGet();

	pnextciu = (struct channel_in_use *) 
			pclient->addrq.node.next;

	while(pciu = pnextciu){
		pnextciu = (struct channel_in_use *)pciu->node.next;

		if (current >= pciu->ticks_at_creation) {
			delay = current - pciu->ticks_at_creation;
		} 
		else {
			delay = current + (~0L - pciu->ticks_at_creation);
		}

		if (delay > timeout) {
			lstDelete(&pclient->addrq, pciu);
        		FASTLOCK(&rsrv_free_addrq_lck);
			lstAdd(&rsrv_free_addrq, pciu);
       			FASTUNLOCK(&rsrv_free_addrq_lck);
			ndelete++;
			maxdelay = max(delay, maxdelay);
		}
	}

	if(ndelete){
#ifdef DEBUG
		logMsg(	"CAS: %d CA channels have expired after %d sec\n",
			ndelete,
			maxdelay / sysClkRateGet());
#endif
	}
}


/*
 * CREATE_UDP_CLIENT
 *
 * 
 */
struct client 
*create_udp_client(sock)
unsigned sock;
{
  	struct client *client;

    	LOCK_CLIENTQ;
	client = (struct client *)lstGet(&rsrv_free_clientQ);
    	UNLOCK_CLIENTQ;

	if(!client){
      		client = (struct client *)malloc(sizeof(struct client));
      		if(!client){
			logMsg("CAS: no mem for new client\n");
        		return NULL;
      		}
	}

      	if(CASDEBUG>2)
        	logMsg(	"CAS: Creating new udp client\n");

 	/*
	 * The following inits to zero done instead of a bfill since the send
	 * and recv buffers are large and don't need initialization.
	 * 
	 * bfill(client, sizeof(*client), NULL);
	 */
   
      	lstInit(&client->addrq);
  	bfill(&client->addr, sizeof(client->addr), 0);
      	client->tid = taskIdSelf();
      	client->send.stk = 0;
      	client->send.cnt = 0;
      	client->recv.stk = 0;
      	client->recv.cnt = 0;
      	client->evuser = NULL;
      	client->eventsoff = FALSE;
	client->disconnect = FALSE;	/* for TCP only */
	client->ticks_at_last_io = tickGet();
      	client->proto = IPPROTO_UDP;
      	client->sock = sock;

      	client->send.maxstk = MAX_UDP-sizeof(client->recv.cnt);

      	FASTLOCKINIT(&client->lock);

      	client->recv.maxstk = MAX_UDP;

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

  	if(CASDEBUG>2)
    		logMsg("CAS: converting udp client to tcp\n");

  	client->proto 		= IPPROTO_TCP;
  	client->send.maxstk 	= MAX_TCP;
  	client->recv.maxstk 	= MAX_TCP;
  	client->sock 		= sock;
      	client->tid = 		taskIdSelf();

  	return OK;
}


