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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <errno.h>

#include "osiSock.h"
#include "osiClock.h"
#include "os_depen.h"
#include "osiThread.h"
#include "errlog.h"
#include "ellLib.h"
#include "taskwd.h"
#include "db_access.h"
#include "envDefs.h"
#include "freeList.h"
#include "server.h"
#include "bsdSocketResource.h"

	
LOCAL void clean_addrq();



/*
 * CAST_SERVER
 *
 * service UDP messages
 * 
 */
int cast_server(void)
{
  	struct sockaddr_in	sin;	
  	int			status;
  	int			count=0;
	struct sockaddr_in	new_recv_addr;
  	int  			recv_addr_size;
	unsigned short		port;
  	int			nchars;
	threadId		tid;

	taskwdInsert(threadGetIdSelf(),NULL,NULL);

	port = caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

  	recv_addr_size = sizeof(new_recv_addr);

    if( IOC_cast_sock!=0 && IOC_cast_sock!=ERROR ) {
        if( (status = socket_close(IOC_cast_sock)) == ERROR ) {
            epicsPrintf ("CAS: Unable to close master cast socket\n");
        }
    }

  	/* 
  	 *  Open the socket.
 	 *  Use ARPA Internet address format and datagram socket.
     */

  	if((IOC_cast_sock = socket (AF_INET, SOCK_DGRAM, 0)) == ERROR){
    		epicsPrintf ("CAS: casts socket creation error\n");
    		threadSuspend(threadGetIdSelf());
  	}

	/*
	 * some concern that vxWorks will run out of mBuf's
	 * if this change is made
	 *
	 * joh 11-10-98
	 */
#if 0
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
		status = setsockopt (IOC_cast_sock, SOL_SOCKET,
						SO_RCVBUF, (char *)&size, sizeof(size));
		if (status<0) {
			epicsPrintf ("CAS: unable to set cast socket size\n");
		}
	}
#endif
    
    /*  Zero the sock_addr structure */
    memset((char *)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    
    /* get server's Internet address */
    if( bind(IOC_cast_sock, (struct sockaddr *)&sin, sizeof (sin)) == ERROR){
        epicsPrintf ("CAS: cast bind error\n");
        socket_close (IOC_cast_sock);
        threadSuspend(threadGetIdSelf());
    }
    
    /* tell clients we are on line again */
    tid = threadCreate("CAonline",threadPriorityChannelAccessClient-3,
	threadGetStackSize(threadStackSmall),
	(THREADFUNC)rsrv_online_notify_task,0);
    if(tid == 0) {
        epicsPrintf ("CAS: couldnt start up online notify task because \"%s\"\n",
            strerror(errno));
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
	threadSleep(300.0);
    }
    
    while (TRUE) {
        status = recvfrom (
            IOC_cast_sock,
            prsrv_cast_client->recv.buf,
            sizeof(prsrv_cast_client->recv.buf),
            NULL,
            (struct sockaddr *)&new_recv_addr, 
            &recv_addr_size);
        if (status<0) {
            epicsPrintf ("CAS: UDP recv error (errno=%s)\n",
                    SOCKERRSTR(SOCKERRNO));
	    threadSleep(1.0);
        }
        else {
            prsrv_cast_client->recv.cnt = (unsigned long) status;
            prsrv_cast_client->recv.stk = 0ul;
            prsrv_cast_client->ticks_at_last_recv = clockGetRate();

            /*
             * If we are talking to a new client flush to the old one 
             * in case we are holding UDP messages waiting to 
             * see if the next message is for this same client.
             */
            if (prsrv_cast_client->send.stk) {
                status = memcmp( (void *)&prsrv_cast_client->addr, (void *)&new_recv_addr, recv_addr_size);
                if(status){ 	
                    /* 
                     * if the address is different 
                     */
                    cas_send_msg(prsrv_cast_client, TRUE);
                    prsrv_cast_client->addr = new_recv_addr;
                }
            }
            else {
                prsrv_cast_client->addr = new_recv_addr;
            }

            if(CASDEBUG>1){
                char	buf[40];
    
                ipAddrToA (&prsrv_cast_client->addr, buf, sizeof(buf));
                epicsPrintf ("CAS: cast server msg of %d bytes from addr %s\n", 
                    prsrv_cast_client->recv.cnt, buf);
            }

            if(CASDEBUG>2)
                count = ellCount(&prsrv_cast_client->addrq);

            status = camessage(
                prsrv_cast_client,&prsrv_cast_client->recv);
            if(status == OK){
                if(prsrv_cast_client->recv.cnt != 
                    prsrv_cast_client->recv.stk){
                    char buf[40];
        
                    ipAddrToA (&prsrv_cast_client->addr, buf, sizeof(buf));

                    epicsPrintf ("CAS: partial (damaged?) UDP msg of %d bytes from %s ?\n",
                        prsrv_cast_client->recv.cnt-prsrv_cast_client->recv.stk, buf);
                }
            }
            else {
                char buf[40];
    
                ipAddrToA (&prsrv_cast_client->addr, buf, sizeof(buf));

                epicsPrintf ("CAS: invalid (damaged?) UDP request from %s ?\n", buf);
            }

            if(CASDEBUG>2){
                if(ellCount(&prsrv_cast_client->addrq)){
                    epicsPrintf ("CAS: Fnd %d name matches (%d tot)\n",
                        ellCount(&prsrv_cast_client->addrq)-count,
                        ellCount(&prsrv_cast_client->addrq));
                }
            }
        }

		/*
		 * allow messages to batch up if more are comming
		 */
 	   	status = socket_ioctl(IOC_cast_sock, FIONREAD, /* sic */(int) &nchars);
 	   	if(status == ERROR){
            threadSuspend(threadGetIdSelf());
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
  	unsigned long		timeout = TIMEOUT*clockGetRate();
	int			s;

	current = clockGetRate();

	semMutexTakeAssert(prsrv_cast_client->addrqLock);
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
        		LOCK_CLIENTQ;
			s = bucketRemoveItemUnsignedId (
				pCaBucket,
				&pciu->sid);
			if(s){
				errMessage (s, "Bad id at close");
			}
       			UNLOCK_CLIENTQ;
			freeListFree(rsrvChanFreeList, pciu);
			ndelete++;
			maxdelay = max(delay, maxdelay);
		}
	}
	semMutexGive(prsrv_cast_client->addrqLock);

#	ifdef DEBUG
	if(ndelete){
		epicsPrintf ("CAS: %d CA channels have expired after %d sec\n",
			ndelete, maxdelay / clockGetRate());
	}
#	endif

}


/*
 * CREATE_UDP_CLIENT
 *
 * 
 */
struct client *create_udp_client(SOCKET sock)
{
	struct client *client;
	
	client = freeListMalloc(rsrvClientFreeList);
	if(!client){
		epicsPrintf ("CAS: no space in pool for a new client\n");
		return NULL;
	}
	
	if(CASDEBUG>2)
		epicsPrintf ("CAS: Creating new udp client\n");
	
	/*
	 * The following inits to zero done instead of a bfill since the send
	 * and recv buffers are large and don't need initialization.
	 * 
	 * memset(client, 0, sizeof(*client));
	 */
	
	client->blockSem = semBinaryCreate(semEmpty);
	if(!client->blockSem){
		freeListFree(rsrvClientFreeList, client);
		return NULL;
	}
	
	/*
	 * user name initially unknown
	 */
	client->pUserName = malloc(1); 
	if(!client->pUserName){
		semBinaryDestroy(client->blockSem);
		freeListFree(rsrvClientFreeList, client);
		return NULL;
	}
	client->pUserName[0] = '\0';
	
	/*
	 * host name initially unknown
	 */
	client->pHostName = malloc(1); 
	if(!client->pHostName){
		semBinaryDestroy(client->blockSem);
		free(client->pUserName);
		freeListFree(rsrvClientFreeList, client);
		return NULL;
	}
	client->pHostName[0] = '\0';
	
	ellInit(&client->addrq);
	ellInit(&client->putNotifyQue);
	memset((char *)&client->addr, 0, sizeof(client->addr));
	client->tid = threadGetIdSelf();
	client->send.stk = 0ul;
	client->send.cnt = 0ul;
	client->recv.stk = 0ul;
	client->recv.cnt = 0ul;
	client->evuser = NULL;
	client->disconnect = FALSE;	/* for TCP only */
	client->ticks_at_last_send = clockGetRate();
	client->ticks_at_last_recv = clockGetRate();
	client->proto = IPPROTO_UDP;
	client->sock = sock;
	client->minor_version_number = CA_UKN_MINOR_VERSION;
	
	client->send.maxstk = MAX_UDP;
	
	client->lock = semMutexCreate();
	client->putNotifyLock = semMutexCreate();
	client->addrqLock = semMutexCreate();
	client->eventqLock = semMutexCreate();
	
	client->recv.maxstk = ETHERNET_MAX_UDP;
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
SOCKET	sock
)
{
	int	status;
	int	addrSize;

  	if(CASDEBUG>2){
    		epicsPrintf ("CAS: converting udp client to tcp\n");
	}

  	client->proto 		= IPPROTO_TCP;
  	client->send.maxstk 	= MAX_TCP;
  	client->recv.maxstk 	= MAX_TCP;
  	client->sock 		= sock;
      	client->tid = 		threadGetIdSelf();

	addrSize = sizeof(client->addr);
        status = getpeername(
                        sock,
                        (struct sockaddr *)&client->addr,
                        &addrSize);
        if(status == ERROR){
                epicsPrintf ("CAS: peer address fetch failed\n");
                return ERROR;
        }

  	return OK;
}


