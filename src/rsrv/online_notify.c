/*
 *	O N L I N E _ N O T I F Y . C
 *
 *	tell CA clients this a server has joined the network
 *
 *	@(#)online_notify.c
 *   @(#)online_notify.c	1.3	6/27/91
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	103090
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
 *	History
 */

/*
 *	system includes
 */
#include <vxWorks.h>
#include <types.h>
#include <socket.h>
#include <in.h>

/*
 *	EPICS includes
 */
#include <task_params.h>
#include <iocmsg.h>

#define abort taskSuspend


/*
 *	RSRV_ONLINE_NOTIFY_TASK
 *	
 *
 *
 */
void rsrv_online_notify_task()
{
	/*
	 * 1 sec init delay
	 */
  	unsigned long		delay = sysClkRateGet();
	/*
	 * CA_ONLINE_DELAY [sec] max delay
	 */
  	unsigned long		maxdelay = CA_ONLINE_DELAY * sysClkRateGet();
	struct extmsg		msg;

  	struct sockaddr_in	send_addr;
  	struct sockaddr_in	recv_addr;
	int			status;
	int			sock;
	struct sockaddr_in	lcl;
  	int			true = TRUE;
	int 			i;

  	/* 
  	 *  Open the socket.
  	 *  Use ARPA Internet address format and datagram socket.
  	 *  Format described in <sys/socket.h>.
   	 */
  	if((sock = socket (AF_INET, SOCK_DGRAM, 0)) == ERROR){
    		logMsg("Socket creation error\n");
    		printErrno (errnoGet ());
    		abort(0);
  	}

	status = local_addr(sock, &lcl);
	if(status<0){
		logMsg("online notify: Network interface unavailable\n");
		abort();
	}

      	status = setsockopt(	sock,
				SOL_SOCKET,
				SO_BROADCAST,
				&true,
				sizeof(true));
      	if(status<0){
    		printErrno (errnoGet ());
    		abort(0);
      	}

      	bfill(&recv_addr, sizeof recv_addr, 0);
      	recv_addr.sin_family = AF_INET;
      	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* let slib pick lcl addr */
      	recv_addr.sin_port = htons(0);	 /* let slib pick port */
      	status = bind(sock, &recv_addr, sizeof recv_addr);
      	if(status<0)
		abort(0);

   	bfill(&msg, sizeof msg, NULL);
	msg.m_cmmd = htons(IOC_RSRV_IS_UP);
	msg.m_available = lcl.sin_addr.s_addr;

 	/*  Zero the sock_addr structure */
  	bfill(&send_addr, sizeof send_addr, 0);
  	send_addr.sin_family 	= AF_INET;
  	send_addr.sin_port 	= htons(CA_CLIENT_PORT);
	status = broadcast_addr(&send_addr.sin_addr);
	if(status<0){
		logMsg("online notify: no interface to broadcast on\n");
		abort(0);
	}

 	while(TRUE){
        	status = sendto(
			sock,
        		&msg,
        		sizeof msg,
        		0,
       			&send_addr, 
			sizeof send_addr);
      		if(status != sizeof msg)
			abort();

		taskDelay(delay);
		delay = min(delay << 1, maxdelay);
	}

}


