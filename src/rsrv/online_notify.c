/*
 *	O N L I N E _ N O T I F Y . C
 *
 *	tell CA clients this a server has joined the network
 *
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
 *      .00 joh 021192  better diagnostics
 */

static char *sccsId = "@(#) $Id$";

/*
 * ansi includes
 */
#include <string.h>

/*
 *	system includes
 */
#include <vxWorks.h>
#include <types.h>
#include <sockLib.h>
#include <socket.h>
#include <errnoLib.h>
#include <in.h>
#include <logLib.h>
#include <sysLib.h>
#include <taskLib.h>

#define MAX_BLOCK_THRESHOLD 100000

/*
 *	EPICS includes
 */
#include "envDefs.h"
#include "server.h"
#include "task_params.h"

/*
 *	RSRV_ONLINE_NOTIFY_TASK
 *	
 *
 *
 */
int rsrv_online_notify_task()
{
	caAddrNode		*pNode;
  	unsigned long		delay;
	unsigned long		maxdelay;
	long			longStatus;
	double			maxPeriod;
	caHdr			msg;
  	struct sockaddr_in	recv_addr;
	int			status;
	int			sock;
  	int			true = TRUE;
	unsigned short		port;

	taskwdInsert(taskIdSelf(),NULL,NULL);

	longStatus = envGetDoubleConfigParam (
				&EPICS_CA_BEACON_PERIOD,
				&maxPeriod);
	if (longStatus) {
		maxPeriod = 15.0;
		ca_printf (
			"EPICS \"%s\" float fetch failed\n",
			EPICS_CA_BEACON_PERIOD.name);
		ca_printf (
			"Setting \"%s\" = %f\n",
			EPICS_CA_BEACON_PERIOD.name,
			maxPeriod);
	}

	/*
	 * 1 sec init delay between beacons
	 */
	delay = sysClkRateGet();
	maxdelay = max(maxPeriod*sysClkRateGet(),sysClkRateGet());

  	/* 
  	 *  Open the socket.
  	 *  Use ARPA Internet address format and datagram socket.
  	 *  Format described in <sys/socket.h>.
   	 */
  	if((sock = socket (AF_INET, SOCK_DGRAM, 0)) == ERROR){
    		logMsg("CAS: online socket creation error\n",
			0,
			0,
			0,
			0,
			0,
			0);
    		abort();
  	}

      	status = setsockopt(	sock,
				SOL_SOCKET,
				SO_BROADCAST,
				(char *)&true,
				sizeof(true));
      	if(status<0){
    		abort();
      	}

      	bfill((char *)&recv_addr, sizeof recv_addr, 0);
      	recv_addr.sin_family = AF_INET;
      	recv_addr.sin_addr.s_addr = INADDR_ANY; /* let slib pick lcl addr */
      	recv_addr.sin_port = htons(0);	 /* let slib pick port */
      	status = bind(sock, (struct sockaddr *)&recv_addr, sizeof recv_addr);
      	if(status<0)
		abort();

   	bfill((char *)&msg, sizeof msg, 0);
	msg.m_cmmd = htons (CA_PROTO_RSRV_IS_UP);
	msg.m_count = htons (ca_server_port);

	ellInit(&beaconAddrList);

	/*
	 * load user and auto configured
	 * broadcast address list
	 */
	port = caFetchPortConfig(&EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT);
	caSetupBCastAddrList (&beaconAddrList, sock, port);

#	ifdef DEBUG
		caPrintAddrList(&beaconAddrList);
#	endif

 	while(TRUE){
		int maxBlock;

		/*
		 * check max block and disable new channels
		 * if its to small
		 */
		maxBlock = memFindMax();
		if(maxBlock<MAX_BLOCK_THRESHOLD){
			casBelowMaxBlockThresh = TRUE;
		}
		else{
			casBelowMaxBlockThresh = FALSE;
		}

		pNode = (caAddrNode *) beaconAddrList.node.next;
		while(pNode){
			msg.m_available = 
				pNode->srcAddr.in.sin_addr.s_addr;
        		status = sendto(
					sock,
        				(char *)&msg,
        				sizeof(msg),
        				0,
					&pNode->destAddr.sa,
					sizeof(pNode->destAddr.sa));
      			if(status < 0){
				logMsg( "%s: CA beacon error was \"%s\"\n",
					(int) __FILE__,
					(int) strerror(errnoGet()),
					0,
					0,
					0,
					0);
			}
			else{
				assert(status == sizeof(msg));
			}
			
			pNode = (caAddrNode *)pNode->node.next;
		}
		taskDelay(delay);
		delay = min(delay << 1, maxdelay);
	}

}


