/*
 *	E X I S I T I N G _ C L I E N T . C
 *
 *
 *	@(#)existing_client.c
 *   @(#)existing_client.c	1.3	6/27/91
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
 *	History
 * .01 120690 joh	added code to avoid overflow of the client timeout
 *			timer after one year of continuous operation
 * .02 071291 joh	all of this code eliminate by mods in other files
 * .03 021292 joh	Better diagnostics
 *				
 */
#ifdef JUNKYARD

#include <vxWorks.h>
#include <taskLib.h>
#include <ellLib.h>
#include <types.h>
#include <in.h>
#include <db_access.h>
#include <server.h>


/*
 *	existing_client()
 *
 *	does this client exist
 *
 *	NOTE: The clientQ should be locked while in this routine
 */
struct client *
existing_client(netaddr)
	FAST struct sockaddr_in *netaddr;
{
	FAST struct client *client;
	FAST struct in_addr addr;
	FAST u_short    port = netaddr->sin_port;

	addr.s_addr = netaddr->sin_addr.s_addr;

	client = (struct client *) & clientQ;

	while (client = (struct client *) client->node.next)
		if (client->addr.sin_addr.s_addr == addr.s_addr)
			if (client->addr.sin_port == port)
				break;

	return client;
}

/*
 * clean_clientq()
 * 
 * remove all leftover UDP clients
 * 
 * NOTE: The clientQ should be locked while in this routine
 */
clean_clientq(timeout)
	FAST unsigned long timeout;
{
	FAST struct client *client, *nextclient;
	FAST unsigned long current = tickGet();
	unsigned long   delay;

	nextclient = (struct client *) & clientQ;

	while (client = nextclient) {
		nextclient = (struct client *) client->node.next;

		if (client->proto == IPPROTO_UDP) {

			/*
			 * compute delay, but avoid overflow
			 */
			if (current >= client->ticks_at_creation) {
				delay = current - client->ticks_at_creation;
			} else {
				delay = current +
					(~0 - client->ticks_at_creation);
			}

#ifdef DEBUG
			if (delay)
				logMsg("CAS: %d sec wait for client conn\n",
				       delay / sysClkRateGet());
			if (timeout)
				logMsg("CAS: UDP client timeout was %d sec\n",
				       timeout / sysClkRateGet());
#endif
			if (delay > timeout) {
				ellDelete(&clientQ, client);
				logMsg("CAS: UDP client expired after %d sec\n",
				       delay / sysClkRateGet());
				terminate_one_client(client);
				ellAdd(&rsrv_free_clientQ, client);
			}
		}
	}

	return OK;
}

#endif
