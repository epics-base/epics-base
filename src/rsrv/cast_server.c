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

static char	*sccsId = "@(#) $Id$";

#include <string.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <types.h>
#include <socket.h>
#include <ioLib.h>
#include <in.h>
#include <logLib.h>
#include <sockLib.h>
#include <errnoLib.h>
#include <sysLib.h>
#include <tickLib.h>
#include <stdioLib.h>
#include <usrLib.h>
#include <inetLib.h>

#include "ellLib.h"
#include "taskwd.h"
#include "db_access.h"
#include "task_params.h"
#include "envDefs.h"
#include "freeList.h"
#include "server.h"

	
LOCAL void clean_addrq();



/*
 * CAST_SERVER
 *
 * service UDP messages
 * 
 */
int cast_server(void)
{
  	struct sockaddr_in		sin;	
  	FAST int			status;
  	int				count=0;
	struct sockaddr_in		new_recv_addr;
  	int  				recv_addr_size;
  	unsigned			nchars;
	unsigned short			port;

	taskwdInsert((int)taskIdCurrent,NULL,NULL);

	port = caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

  	recv_addr_size = sizeof(new_recv_addr);

  	if( IOC_cast_sock!=0 && IOC_cast_sock!=ERROR )
    		if( (status = close(IOC_cast_sock)) == ERROR )
      			logMsg("CAS: Unable to close master cast socket\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);

  	/* 
  	 *  Open the socket.
 	 *  Use ARPA Internet address format and datagram socket.
 	 *  Format described in <sys/socket.h>.
 	 */

  	if((IOC_cast_sock = socket (AF_INET, SOCK_DGRAM, 0)) == ERROR){
    		logMsg("CAS: casts socket creation error\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
    		taskSuspend(taskIdSelf());
  	}

        {
                /*
                 *
                 * this allows for faster connects by queuing
                 * additional incomming UDP search frames
                 *
                 * this allocates a 32k buffer
                 * (uses a power of two)
                 */
                int size = 1u<<15u;
                status = setsockopt(
				IOC_cast_sock,
                                SOL_SOCKET,
                                SO_RCVBUF,
                                (char *)&size,
                                sizeof(size));
                if (status<0) {
			logMsg("CAS: unable to set cast socket size\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
                }
        }

  	/*  Zero the sock_addr structure */
  	bfill((char *)&sin, sizeof(sin), 0);
  	sin.sin_family = AF_INET;
  	sin.sin_addr.s_addr = INADDR_ANY;
  	sin.sin_port = htons(port);
	
  	/* get server's Internet address */
  	if( bind(IOC_cast_sock, (struct sockaddr *)&sin, sizeof (sin)) == ERROR){
    		logMsg("CAS: cast bind error\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
    		close (IOC_cast_sock);
    		taskSuspend(0);
  	}

  	/* tell clients we are on line again */
  	status = taskSpawn(
		CA_ONLINE_NAME,
		CA_ONLINE_PRI,
		CA_ONLINE_OPT,
		CA_ONLINE_STACK,
		rsrv_online_notify_task,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL);
  	if(status==ERROR){
    		logMsg("CAS: couldnt start up online notify task\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
    		printErrno(errnoGet());
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
			(struct sockaddr *)&new_recv_addr, 
			&recv_addr_size);
    		if(status<0){
      			logMsg("CAS: UDP recv error (errno=%d)\n",
				errnoGet(),
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			taskDelay(sysClkRateGet());
			continue;
    		}

		prsrv_cast_client->recv.cnt = (unsigned long) status;
		prsrv_cast_client->recv.stk = 0ul;
		prsrv_cast_client->ticks_at_last_recv = tickGet();

		/*
		 * If we are talking to a new client flush the old one 
		 * in case it is holding UDP messages waiting to 
		 * see if the next message is for this same client.
		 */
		status = bcmp(
				(char *)&prsrv_cast_client->addr, 
				(char *)&new_recv_addr, 
				recv_addr_size);
		if(status){ 	
			/* 
			 * if the address is different 
			 */
			cas_send_msg(prsrv_cast_client, TRUE);
  			prsrv_cast_client->addr = new_recv_addr;
		}

    		if(CASDEBUG>1){
			char	buf[40];

       			logMsg(	"CAS: cast server msg of %d bytes\n", 
				prsrv_cast_client->recv.cnt,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			inet_ntoa_b (prsrv_cast_client->addr.sin_addr, 
				buf);
       			logMsg(	"CAS: from addr %s, port %d \n", 
				(int)buf,
				ntohs(prsrv_cast_client->addr.sin_port),
				NULL,
				NULL,
				NULL,
				NULL);
    		}

    		if(CASDEBUG>2)
      			count = ellCount(&prsrv_cast_client->addrq);

    		status = camessage(
				prsrv_cast_client, 
				&prsrv_cast_client->recv);
		if(status == OK){
			if(prsrv_cast_client->recv.cnt != 
				prsrv_cast_client->recv.stk){

				logMsg(	"CAS: partial UDP msg of %d bytes ?\n",
					prsrv_cast_client->recv.cnt-
						prsrv_cast_client->recv.stk,
					NULL,
					NULL,
					NULL,
					NULL,
					NULL);
			}
		}

      		if(CASDEBUG>2){
			if(ellCount(&prsrv_cast_client->addrq)){
        			logMsg(	"CAS: Fnd %d name matches (%d tot)\n",
					ellCount(&prsrv_cast_client->addrq)
						-count,
					ellCount(&prsrv_cast_client->addrq),
					NULL,
					NULL,
					NULL,
					NULL);
			}
		}

		/*
		 * allow message to batch up if more are comming
		 */
 	   	status = ioctl(IOC_cast_sock, FIONREAD, (int) &nchars);
 	   	if(status == ERROR){
  	    		taskSuspend(0);
 	   	}

    		if(nchars == 0){
			cas_send_msg(prsrv_cast_client, TRUE);
	      		clean_addrq();
    		}	
  	}
}


/*
 * clean_addrq
 *
 * 
 */
#define		TIMEOUT	60 /* sec */

LOCAL void clean_addrq()
{
	struct channel_in_use	*pciu;
	struct channel_in_use	*pnextciu;
	unsigned long 		current;
	unsigned long   	delay;
	unsigned long   	maxdelay = 0;
	unsigned		ndelete=0;
  	unsigned long		timeout = TIMEOUT*sysClkRateGet();
	int			s;

	current = tickGet();

	FASTLOCK(&prsrv_cast_client->addrqLock);
	pnextciu = (struct channel_in_use *) 
			prsrv_cast_client->addrq.node.next;

	while( (pciu = pnextciu) ) {
		pnextciu = (struct channel_in_use *)pciu->node.next;

		if (current >= pciu->ticks_at_creation) {
			delay = current - pciu->ticks_at_creation;
		} 
		else {
			delay = current + (~0L - pciu->ticks_at_creation);
		}

		if (delay > timeout) {

			ellDelete(&prsrv_cast_client->addrq, &pciu->node);
        		FASTLOCK(&clientQlock);
			s = bucketRemoveItemUnsignedId (
				pCaBucket,
				&pciu->sid);
			if(s){
				errMessage (s, "Bad id at close");
			}
       			FASTUNLOCK(&clientQlock);
			freeListFree(rsrvChanFreeList, pciu);
			ndelete++;
			maxdelay = max(delay, maxdelay);
		}
	}
	FASTUNLOCK(&prsrv_cast_client->addrqLock);

#	ifdef DEBUG
	if(ndelete){
		logMsg(	"CAS: %d CA channels have expired after %d sec\n",
			ndelete,
			maxdelay / sysClkRateGet(),
			NULL,
			NULL,
			NULL,
			NULL);
	}
#	endif

}


/*
 * CREATE_UDP_CLIENT
 *
 * 
 */
struct client *create_udp_client(unsigned sock)
{
  	struct client *client;

	client = freeListMalloc(rsrvClientFreeList);
	if(!client){
		logMsg("CAS: no spae in pool for a new client\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		return NULL;
	}

      	if(CASDEBUG>2)
        	logMsg(	"CAS: Creating new udp client\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);

 	/*
	 * The following inits to zero done instead of a bfill since the send
	 * and recv buffers are large and don't need initialization.
	 * 
	 * memset(client, 0, sizeof(*client));
	 */
   
	client->blockSem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if(!client->blockSem){
		freeListFree(rsrvClientFreeList, client);
		return NULL;
	}

	/*
	 * user name initially unknown
	 */
	client->pUserName = malloc(1); 
	if(!client->pUserName){
		semDelete(client->blockSem);
		freeListFree(rsrvClientFreeList, client);
		return NULL;
	}
	client->pUserName[0] = '\0';

	/*
	 * host name initially unknown
	 */
	client->pHostName = malloc(1); 
	if(!client->pHostName){
		semDelete(client->blockSem);
		free(client->pUserName);
		freeListFree(rsrvClientFreeList, client);
		return NULL;
	}
	client->pHostName[0] = '\0';

      	ellInit(&client->addrq);
      	ellInit(&client->putNotifyQue);
  	bfill((char *)&client->addr, sizeof(client->addr), 0);
      	client->tid = taskIdSelf();
      	client->send.stk = 0ul;
      	client->send.cnt = 0ul;
      	client->recv.stk = 0ul;
      	client->recv.cnt = 0ul;
      	client->evuser = NULL;
	client->disconnect = FALSE;	/* for TCP only */
	client->ticks_at_last_send = tickGet();
	client->ticks_at_last_recv = tickGet();
      	client->proto = IPPROTO_UDP;
      	client->sock = sock;
      	client->minor_version_number = CA_UKN_MINOR_VERSION;

      	client->send.maxstk = MAX_UDP-sizeof(client->recv.cnt);

      	FASTLOCKINIT(&client->lock);
      	FASTLOCKINIT(&client->putNotifyLock);
      	FASTLOCKINIT(&client->addrqLock);
      	FASTLOCKINIT(&client->eventqLock);

      	client->recv.maxstk = MAX_UDP;

      	return client;
}



/*
 * UDP_TO_TCP
 *
 * send lock must be applied
 * 
 */
int udp_to_tcp(
struct client 	*client,
unsigned	sock
)
{
	int	status;
	int	addrSize;

  	if(CASDEBUG>2){
    		logMsg("CAS: converting udp client to tcp\n",
			NULL,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

  	client->proto 		= IPPROTO_TCP;
  	client->send.maxstk 	= MAX_TCP;
  	client->recv.maxstk 	= MAX_TCP;
  	client->sock 		= sock;
      	client->tid = 		taskIdSelf();

	addrSize = sizeof(client->addr);
        status = getpeername(
                        sock,
                        (struct sockaddr *)&client->addr,
                        &addrSize);
        if(status == ERROR){
                logMsg("CAS: peer address fetch failed\n",
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL,
                        NULL);
                return ERROR;
        }

  	return OK;
}


