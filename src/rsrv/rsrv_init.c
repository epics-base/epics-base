/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	5-88
 */

static char *sccsId = "@(#) $Id$";

#include <vxWorks.h>
#include <taskLib.h>
#include <types.h>
#include <socket.h>
#include <in.h>
#include <errnoLib.h>
#include <usrLib.h>

#include "ellLib.h"
#include "dbDefs.h"
#include "db_access.h"
#include "task_params.h"
#include "freeList.h"
#include "server.h"

#define DELETE_TASK(NAME)\
if(taskNameToId(NAME)!=ERROR)taskDelete(taskNameToId(NAME));


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

	DELETE_TASK(CAST_SRVR_NAME);
	DELETE_TASK(REQ_SRVR_NAME);
	DELETE_TASK(CA_ONLINE_NAME);

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
