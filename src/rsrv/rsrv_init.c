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

#include <vxWorks.h>
#include "ellLib.h"
#include <taskLib.h>
#include <types.h>
#include <socket.h>
#include <in.h>
#include <errnoLib.h>
#include <usrLib.h>

#include "dbDefs.h"
#include "db_access.h"
#include "task_params.h"
#include "freeList.h"
#include "server.h"

#define DELETE_TASK(TID)\
if(taskIdVerify(TID)==OK)taskDelete(TID);


/*
 * rsrv_init()
 */
int rsrv_init()
{
	FASTLOCKINIT(&clientQlock);

	ellInit(&clientQ);
	freeListInitPvt(&rsrvClientFreeList, sizeof(struct client), 8);
	freeListInitPvt(&rsrvChanFreeList, sizeof(struct channel_in_use), 512);
	freeListInitPvt(&rsrvEventFreeList, 
		sizeof(struct event_ext)+db_sizeof_event_block(), 512);
	ellInit(&beaconAddrList);
	prsrv_cast_client = NULL;
	pCaBucket = NULL;

	DELETE_TASK(taskNameToId(CAST_SRVR_NAME));
	DELETE_TASK(taskNameToId(REQ_SRVR_NAME));
	DELETE_TASK(taskNameToId(CA_ONLINE_NAME));

	taskSpawn(REQ_SRVR_NAME,
		  REQ_SRVR_PRI,
		  REQ_SRVR_OPT,
		  REQ_SRVR_STACK,
		  req_server,
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

	taskSpawn(CAST_SRVR_NAME,
		  CAST_SRVR_PRI,
		  CAST_SRVR_OPT,
		  CAST_SRVR_STACK,
		  cast_server,
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

	return OK;
}
