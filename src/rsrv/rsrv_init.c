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
 *	.01 073093 Added task spawn args for 5.1 vxworks
 */

static char *sccsId = "@(#) $Id$";

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "osiSock.h"
#include "osiThread.h"
#include "ellLib.h"
#include "dbDefs.h"
#include "db_access.h"
#include "freeList.h"
#include "server.h"

#define DELETE_TASK(NAME)\
if(threadNameToId(NAME)!=0)threadDestroy(threadNameToId(NAME));


/*
 * rsrv_init()
 */
int rsrv_init()
{
        clientQlock = semMutexCreate();

	ellInit(&clientQ);
	freeListInitPvt(&rsrvClientFreeList, sizeof(struct client), 8);
	freeListInitPvt(&rsrvChanFreeList, sizeof(struct channel_in_use), 512);
	freeListInitPvt(&rsrvEventFreeList, 
		sizeof(struct event_ext)+db_sizeof_event_block(), 512);
	ellInit(&beaconAddrList);
	prsrv_cast_client = NULL;
	pCaBucket = NULL;

	DELETE_TASK("CAUDP");
	DELETE_TASK("CATCP");
	DELETE_TASK("CAonline");

	threadCreate("CATCP",
	    threadPriorityChannelAccessServer,
	    threadGetStackSize(threadStackMedium),
	    (THREADFUNC)req_server,0);
	threadCreate("CAUDP",
	    threadPriorityChannelAccessServer-1,
	    threadGetStackSize(threadStackMedium),
	    (THREADFUNC)cast_server,0);
	return OK;
}
