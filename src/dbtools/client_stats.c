
/*
	You must run PVSstart() in your vxWorks startup script before these
	functions can be used.

	Just ld() the object file for this code in your vxWorks startup script.

	Services added in this file:
		PVS_MemStats - Send the bytes free and allocated to the requestor.
		PVS_ClientStats - Send a CA client statistics summary report to
			the requestor.  Includes host name, user id, number of channels
			in use.
		PVS_TaskList - Send a report of all the tasks in the IOC, along
			with there state and priority.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vxWorks.h>
#include <iv.h>
#include <taskLib.h>
#include <memLib.h>
#include <private/memPartLibP.h> /* sucks, don't it */

/* required for client stat section */
#include <sockLib.h>
#include <socket.h>
#include <in.h>
#include <ellLib.h>
#include <server.h>

#include "PVS.h"

void PVS_ClientStats(BS* bs)
{
	int len;
	char line[120];
	struct client 	*pclient;

	/* report columns:
		client_host_name client_login_id number_of_channels
	*/

	LOCK_CLIENTQ;
	pclient = (struct client *) ellFirst(&clientQ);
	while (pclient)
	{
	 	sprintf(line,"%s %s %d\n",
			pclient->pHostName, pclient->pUserName, ellCount(&pclient->addrq));
		len=strlen(line)+1;

		if(BSsendHeader(bs,PVS_Data,len)<0)
			printf("PVSserver: data cmd failed\n");
		else
		{
			if(BSsendData(bs,line,len)<0)
				printf("PVSserver: data send failed\n");
		}
		pclient = (struct client *) ellNext(&pclient->node);
	}
	UNLOCK_CLIENTQ;
	BSsendHeader(bs,BS_Done,0);
}

