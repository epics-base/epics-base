#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sockLib.h>
#include <socket.h>
#include <in.h>

#include <ellLib.h>
#include <server.h>

void client_stat_summary(void)
{
	struct client 	*pclient;

	LOCK_CLIENTQ;
	pclient = (struct client *) ellFirst(&clientQ);
	while (pclient) {
	 	printf( "Client Name=%s, Client Host=%s Number channels %d\n",
			pclient->pUserName,
			pclient->pHostName,
			ellCount(&pclient->addrq));
		pclient = (struct client *) ellNext(&pclient->node);
	}
	UNLOCK_CLIENTQ;
}
