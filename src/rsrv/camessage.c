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
 *	.01 joh 012290 	placed break in ca_cancel_event search so entire 
 *			list is not searched.
 *	.02 joh	011091	added missing break to error message send 
 *			chix select switch
 *	.03 joh	071291	now timestanmps channel in use block
 *	.04 joh	071291	code for IOC_CLAIM_CIU command
 *	.05 joh 082691  use db_post_single_event() instead of read_reply()
 *			to avoid deadlock condition between the client
 *			and the server.
 *	.06 joh	110491	lock added for IOC_CLAIM_CIU command
 *	.07 joh	021292	Better diagnostics
 *	.08 joh	021492	use lstFind() to verify chanel in clear_channel()
 *	.09 joh 031692	dont send exeception to the client if bad msg
 *			detected- just disconnect instead
 *	.10 joh	050692	added new routine - cas_send_heartbeat()
 *	.11 joh 062492	dont allow flow control to turn off gets
 *	.12 joh 090893	converted pointer to server id	
 *	.13 joh 091493	made events on action a subroutine for debugging
 *	.14 joh 020194	New command added for CAV4.1 - client name
 */


static char *sccsId = "$Id$";

#include <assert.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <types.h>
#include <in.h>
#include <logLib.h>
#include <tickLib.h>
#include <stdioLib.h>
#include <sysLib.h>
#include <string.h>

#include <ellLib.h>
#include <db_access.h>
#include <task_params.h>
#include <server.h>
#include <caerr.h>


static struct extmsg nill_msg;

#define	RECORD_NAME(PADDR) ((PADDR)->precord)

LOCAL void clear_channel_reply(
struct extmsg *mp,
struct client  *client
);

LOCAL void event_cancel_reply(
struct extmsg *mp,
struct client  *client
);

LOCAL EVENTFUNC	read_reply;

LOCAL void read_sync_reply(
struct extmsg *mp,
struct client  *client
);

LOCAL void build_reply(
struct extmsg *mp,
struct client  *client
);

LOCAL void search_fail_reply(
struct extmsg 	*mp,
struct client  	*client
);

LOCAL void send_err(
struct extmsg  *curp,
int            status,
struct client  *client,
char           *footnote
);

LOCAL void log_header(
struct extmsg 	*mp,
int		mnum
);

LOCAL void event_add_action(
struct extmsg *mp,
struct client  *client
);

LOCAL void logBadId(
struct client *client, 
struct extmsg *mp
);

LOCAL struct channel_in_use *MPTOPCIU(
struct extmsg *mp
);

LOCAL void events_on_action(
struct client  *client
);

LOCAL void write_notify_call_back(PUTNOTIFY *ppn);

LOCAL void write_notify_action(
struct extmsg *mp,
struct client  *client
);

LOCAL void putNotifyErrorReply(
struct client *client, 
struct extmsg *mp, 
int statusCA
);

LOCAL void claim_ciu_action(
struct extmsg *mp,
struct client  *client
);

LOCAL void client_name_action(
struct extmsg 	*mp,
struct client  	*client
);

LOCAL void host_name_action(
struct extmsg 	*mp,
struct client  	*client
);

LOCAL void casAccessRightsCB(
ASCLIENTPVT ascpvt, 
asClientStatus type
);

LOCAL void no_read_access_event(
struct client  		*client,
struct event_ext 	*pevext
);

LOCAL unsigned long	bucketID;


/*
 * CAMESSAGE()
 *
 *
 */
int camessage(
struct client  *client,
struct message_buffer *recv
)
{
	int            		nmsg = 0;
	int			v41;
	unsigned		msgsize;
	unsigned		bytes_left;
	int             	status;
	struct extmsg 		*mp;
	struct channel_in_use 	*pciu;

	if(!pCaBucket){
		pCaBucket = bucketCreate(NBBY*sizeof(mp->m_cid));
		if(!pCaBucket){
			return ERROR;
		}
	}

	if (CASDEBUG > 2){
		logMsg(	"CAS: Parsing %d(decimal) bytes\n", 
			recv->cnt,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	bytes_left = recv->cnt;
	while (bytes_left) {

		if(bytes_left < sizeof(*mp))
			return OK;

		mp = (struct extmsg *) &recv->buf[recv->stk];

		msgsize = mp->m_postsize + sizeof(*mp);

		if(msgsize > bytes_left) 
			return OK;

		nmsg++;

		if (CASDEBUG > 2)
			log_header(mp, nmsg);

		switch (mp->m_cmmd) {
		case IOC_NOOP:	/* verify TCP */
			break;

		case IOC_CLIENT_NAME:
			client_name_action(mp, client);
			break;

		case IOC_HOST_NAME:
			host_name_action(mp, client);
			break;
		
		case IOC_EVENT_ADD:
			event_add_action(mp, client);
			break;

		case IOC_EVENT_CANCEL:
			event_cancel_reply(mp, client);
			break;

		case IOC_CLEAR_CHANNEL:
			clear_channel_reply(mp, client);
			break;

		case IOC_READ_NOTIFY:
		case IOC_READ:
		{
			struct event_ext evext;

			pciu = MPTOPCIU(mp);
			if(!pciu){
				logBadId(client, mp);
				break;
			}

			evext.msg = *mp;
			evext.pciu = pciu;
			evext.send_lock = TRUE;
			evext.get = TRUE;
			evext.pdbev = NULL;
			evext.size = dbr_size_n(mp->m_type, mp->m_count);

			/*
			 * Arguments to this routine organized in
			 * favor of the standard db event calling
			 * mechanism-  routine(userarg, paddr). See
			 * events added above.
			 * 
			 * Hold argument set true so the send message
			 * buffer is not flushed once each call.
			 */
			read_reply(&evext, &pciu->addr, TRUE, NULL);
			break;
		}
		case IOC_SEARCH:
		case IOC_BUILD:
			build_reply(mp, client);
			break;

		case IOC_WRITE_NOTIFY:
			write_notify_action(mp, client);
			break;

		case IOC_WRITE:
			pciu = MPTOPCIU(mp);
			if(!pciu){
				logBadId(client, mp);
				break;
			}
			if(!asCheckPut(pciu->asClientPVT)){
				v41 = CA_V41(
					CA_PROTOCOL_VERSION,
					client->minor_version_number);
				if(v41){
					status = ECA_NOWTACCESS;
				}
				else{
					status = ECA_PUTFAIL;
				}
				LOCK_CLIENT(client);
				send_err(
					mp, 
					status, 
					client, 
					RECORD_NAME(&pciu->addr));
				UNLOCK_CLIENT(client);
				break;
			}
			status = db_put_field(
					      &pciu->addr,
					      mp->m_type,
					      mp + 1,
					      mp->m_count);
			if (status < 0) {
				LOCK_CLIENT(client);
				send_err(
					mp, 
					ECA_PUTFAIL, 
					client, 
					RECORD_NAME(&pciu->addr));
				UNLOCK_CLIENT(client);
			}
			break;

		case IOC_EVENTS_ON:
			events_on_action(client);
			break;

		case IOC_EVENTS_OFF:
			client->eventsoff = TRUE;
			break;

		case IOC_READ_SYNC:
			read_sync_reply(mp, client);
			break;

		case IOC_CLAIM_CIU:
			claim_ciu_action(mp, client);
			break;

		default:
			logMsg("CAS: bad msg detected\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			log_header(mp, nmsg);
			/* 
			 *	most clients dont recover
			 *	from this
			 */
			LOCK_CLIENT(client);
			send_err(mp, ECA_INTERNAL, client, "Invalid Msg");
			UNLOCK_CLIENT(client);
			/*
			 * returning ERROR here disconnects
			 * the client with the bad message
			 */
			logMsg("CAS: forcing disconnect ...\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			return ERROR;
		}

		recv->stk += msgsize;
		bytes_left = recv->cnt - recv->stk;
	}

	return OK;
}


/*
 * host_name_action()
 */
LOCAL void host_name_action(
struct extmsg 	*mp,
struct client  	*client
)
{
	struct channel_in_use 	*pciu;
	unsigned		size;
	char 			*pName;
	char 			*pMalloc;
	int			status;

	LOCK_CLIENT(client);

	pName = (char *)(mp+1);
	size = strlen(pName)+1;
	/*
	 * user name will not change if there isnt enough memory
	 */
	pMalloc = malloc(size);
	if(!pMalloc){
		send_err(
			mp,
			ECA_ALLOCMEM,
			client,
			"");
		return;
	}
	if(client->pHostName){
		free(client->pHostName);
	}
	client->pHostName = pMalloc;
	strncpy(
		client->pHostName, 
		pName, 
		size-1);
	client->pHostName[size-1]='\0';

	pciu = (struct channel_in_use *) client->addrq.node.next;
	while(pciu){
		status = asChangeClient(
				pciu->asClientPVT,
				asDbGetAsl(&pciu->addr),
				client->pUserName,
				client->pHostName);	
		if(status){
			UNLOCK_CLIENT(prsrv_cast_client);
       			free_client(client);
			exit(0);
		}
		pciu = (struct channel_in_use *) pciu->node.next;
	}

	UNLOCK_CLIENT(client);
}


/*
 * client_name_action()
 */
LOCAL void client_name_action(
struct extmsg 	*mp,
struct client  	*client
)
{
	struct channel_in_use 	*pciu;
	unsigned		size;
	char 			*pName;
	char 			*pMalloc;
	int			status;

	LOCK_CLIENT(client);

	pName = (char *)(mp+1);
	size = strlen(pName)+1;
	/*
	 * user name will not change if there isnt enough memory
	 */
	pMalloc = malloc(size);
	if(!pMalloc){
		send_err(
			mp,
			ECA_ALLOCMEM,
			client,
			"");
		return;
	}
	if(client->pUserName){
		free(client->pUserName);
	}
	client->pUserName = pMalloc;
	strncpy(
		client->pUserName, 
		pName, 
		size-1);
	client->pUserName[size-1]='\0';

	pciu = (struct channel_in_use *) client->addrq.node.next;
	while(pciu){
		status = asChangeClient(
				pciu->asClientPVT,
				asDbGetAsl(&pciu->addr),
				client->pUserName,
				client->pHostName);	
		if(status){
			UNLOCK_CLIENT(prsrv_cast_client);
       			free_client(client);
			exit(0);
		}
		pciu = (struct channel_in_use *) pciu->node.next;
	}

	UNLOCK_CLIENT(client);
}


/*
 * claim_ciu_action()
 */
LOCAL void claim_ciu_action(
struct extmsg *mp,
struct client  *client
)
{
	int			status;
	struct channel_in_use 	*pciu;


	LOCK_CLIENT(prsrv_cast_client);

	/*
	 * clients which dont claim their 
	 * channel in use block prior to
	 * timeout must reconnect
	 */
	pciu = MPTOPCIU(mp);
	if(!pciu){
		logMsg("CAS: client timeout disconnect id=%d\n",
			mp->m_cid,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		UNLOCK_CLIENT(prsrv_cast_client);
       		free_client(client);
		exit(0);
	}


	/* 
	 * Any other client attachment is a severe error
	 */
	if(pciu->client!=prsrv_cast_client){
		logMsg("CAS: bad channel claim disconnect %d %x %x\n",
			mp->m_cid,
			(int)pciu,
			(int)pciu->client,
			NULL,
			NULL,
			NULL);
		UNLOCK_CLIENT(prsrv_cast_client);
       		free_client(client);
		exit(0);
	}

	/*
	 * set up access security for this channel
	 */
	status = asChangeClient(
			pciu->asClientPVT,
			asDbGetAsl(&pciu->addr),
			client->pUserName,
			client->pHostName);	
	if(status){
		UNLOCK_CLIENT(prsrv_cast_client);
       		free_client(client);
		exit(0);
	}

	/*
	 * remove channel in use block from
	 * the UDP client where it could time
	 * out and place it on the client
	 * who is claiming it
	 */
	ellDelete(
		&prsrv_cast_client->addrq, 
		&pciu->node);
	UNLOCK_CLIENT(prsrv_cast_client);

	pciu->client = client;

	LOCK_CLIENT(client);
	ellAdd(&client->addrq, &pciu->node);
	UNLOCK_CLIENT(client);

	/*
	 * The available field is used (abused)
	 * here to communicate the miner version number
	 * starting with CA 4.1. The field was set to zero
	 * prior to 4.1
	 */
	pciu->client->minor_version_number = mp->m_available;

	/*
	 * register for asynch updates of access rights changes
	 * (only after the lock is released, we are added to 
	 * the correct client, and the clients version is
	 * known)
	 */
	asRegisterClientCallback(pciu->asClientPVT, casAccessRightsCB);
}


/*
 * write_notify_call_back()
 *
 * (called by the db call back thread)
 */
LOCAL void write_notify_call_back(PUTNOTIFY *ppn)
{
	struct client		*pclient;
	struct channel_in_use 	*pciu;

	/*
	 * we choose to suspend the task if there
	 * is an internal failure
	 */
	pciu = (struct channel_in_use *) ppn->usrPvt;
	if(!pciu){
		taskSuspend(0);
	}
	if(!pciu->pPutNotify){
		taskSuspend(0);
	}

	pclient = pciu->client;

	/*
	 * independent lock used here in order to
	 * avoid any possibility of blocking
	 * the database (or indirectly blocking
	 * one client on another client).
	 */
	FASTLOCK(&pclient->putNotifyLock);
	ellAdd(&pclient->putNotifyQue, &pciu->pPutNotify->node);
	FASTUNLOCK(&pclient->putNotifyLock);

	/*
	 * offload the labor for this to the
	 * event task so that we never block
	 * the db or another client.
	 */
	db_post_extra_labor(pclient->evuser);
}


/* 
 * write_notify_reply()
 *
 * (called by the CA server event task via the extra labor interface)
 */
void write_notify_reply(void *pArg)
{
	RSRVPUTNOTIFY		*ppnb;
	struct client 		*pClient;
	struct extmsg		*preply;
	int			status;

	pClient = pArg;

	LOCK_CLIENT(pClient);
	while(TRUE){
		/*
		 * independent lock used here in order to
		 * avoid any possibility of blocking
		 * the database (or indirectly blocking
		 * one client on another client).
		 */
		FASTLOCK(&pClient->putNotifyLock);
		ppnb = (RSRVPUTNOTIFY *)ellGet(&pClient->putNotifyQue);
		FASTUNLOCK(&pClient->putNotifyLock);
		/*
		 * break to loop exit
		 */
		if(!ppnb){
			break;
		}

		/*
		 * aquire sufficient output buffer
		 */
		preply = ALLOC_MSG(pClient, 0);
		if (!preply) {
			/*
			 * inability to aquire buffer space
			 * Indicates corruption  
			 */
			logMsg("CA server corrupted - put call back(s) discarded\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			break;
		}
		*preply = ppnb->msg;
		/*
		 *
		 * Map from DB status to CA status
		 *
		 * the channel id field is being abused to carry 
		 * status here
		 */
		if(ppnb->dbPutNotify.status){
			if(ppnb->dbPutNotify.status == S_db_Blocked){
				preply->m_cid = ECA_PUTCBINPROG;
			}
			else{
				preply->m_cid = ECA_PUTFAIL;
			}
		}
		else{
			preply->m_cid = ECA_NORMAL;
		}

		/* commit the message */
		END_MSG(pClient);
		ppnb->busy = FALSE;
	}
	UNLOCK_CLIENT(pClient);

	/*
	 * wakeup the TCP thread if it is waiting for a cb to complete
	 */
	status = semGive(pClient->blockSem);
	if(status != OK){
		logMsg("CA block sem corrupted\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
	}
}


/*
 * write_notify_action()
 */
LOCAL void write_notify_action(
struct extmsg *mp,
struct client  *client
)
{
	unsigned long		size;
	int			status;
	struct channel_in_use 	*pciu;

	pciu = MPTOPCIU(mp);
	if(!pciu){
		logBadId(client, mp);
		return;
	}

	if(!asCheckPut(pciu->asClientPVT)){
		putNotifyErrorReply(client, mp, ECA_NOWTACCESS);
		return;
	}

	size = dbr_size_n(mp->m_type, mp->m_count);

	if(pciu->pPutNotify){
		/*
		 * serialize concurrent put notifies 
		 */
		while(pciu->pPutNotify->busy){
			status = semTake(
					client->blockSem, 
					sysClkRateGet()*60);
			if(status != OK && pciu->pPutNotify->busy){
				log_header(mp,0);
				dbNotifyCancel(&pciu->pPutNotify->dbPutNotify);
				pciu->pPutNotify->busy = FALSE;
				putNotifyErrorReply(
						client, 
						mp, 
						ECA_PUTCBINPROG);
				return;
			}
		}

		/*
		 * if not busy then free the current
		 * block if it is to small
		 */
		if(pciu->pPutNotify->valueSize<size){
			free(pciu->pPutNotify);
			pciu->pPutNotify = NULL;
		}
	}

	/*
	 * send error and go to next request
	 * if there isnt enough memory left
	 */
	if(!pciu->pPutNotify){
		pciu->pPutNotify = 
			(RSRVPUTNOTIFY *) calloc(1, sizeof(*pciu->pPutNotify)+size);
		if(!pciu->pPutNotify){
			putNotifyErrorReply(client, mp, ECA_ALLOCMEM);
			return;
		}
		pciu->pPutNotify->valueSize = size;
		pciu->pPutNotify->dbPutNotify.pbuffer = (pciu->pPutNotify+1);
		pciu->pPutNotify->dbPutNotify.usrPvt = pciu;
		pciu->pPutNotify->dbPutNotify.paddr = &pciu->addr;
		pciu->pPutNotify->dbPutNotify.userCallback = write_notify_call_back;
	}

	pciu->pPutNotify->busy = TRUE;
	pciu->pPutNotify->msg = *mp;
	pciu->pPutNotify->dbPutNotify.nRequest = mp->m_count;
	memcpy(pciu->pPutNotify->dbPutNotify.pbuffer, (char *)(mp+1), size);
	status = dbPutNotifyMapType(&pciu->pPutNotify->dbPutNotify, mp->m_type);
	if(status){
		putNotifyErrorReply(client, mp, ECA_PUTFAIL);
		pciu->pPutNotify->busy = FALSE;
		return;
	}

	status = dbPutNotify(&pciu->pPutNotify->dbPutNotify);
	if(status){
		/*
		 * let the call back take care of failure
		 * even if it is immediate
		 */
		pciu->pPutNotify->dbPutNotify.status = status;
		(*pciu->pPutNotify->dbPutNotify.userCallback)
					(&pciu->pPutNotify->dbPutNotify);
	}
}


/*
 * putNotifyErrorReply
 */
LOCAL void putNotifyErrorReply(struct client *client, struct extmsg *mp, int statusCA)
{
	struct extmsg		*preply;

	LOCK_CLIENT(client);
	preply = ALLOC_MSG(client, 0);
	if(!preply){
		logMsg("Fatal Error:%s, %d\n",
			(int)__FILE__,
			__LINE__,
			NULL,
			NULL,
			NULL,
			NULL);
		taskSuspend(0);
	}

	*preply = *mp;
	/*
 	 * the cid field abused to contain status
 	 * during put cb replies
 	 */
	preply->m_cid = statusCA;
	END_MSG(client);
	UNLOCK_CLIENT(client);
}


/*
 *
 * events_on_action()
 *
 */
LOCAL void events_on_action(
struct client  *client
)
{
	struct channel_in_use 	*pciu;
	struct event_ext *pevext;
	struct event_ext evext;

	client->eventsoff = FALSE;

	LOCK_CLIENT(client);

	pciu = (struct channel_in_use *) 
		client->addrq.node.next;
	while (pciu) {
		pevext = (struct event_ext *) 
			pciu->eventq.node.next;
		while (pevext){

			if (pevext->modified) {
				evext = *pevext;
				evext.send_lock = FALSE;
				evext.get = TRUE;
				read_reply(
					&evext, 
					&pciu->addr, 
					TRUE, 
					NULL);
				pevext->modified = FALSE;
			}
			pevext = (struct event_ext *)
				pevext->node.next;
		}

		pciu = (struct channel_in_use *)
			pciu->node.next;
	}
	UNLOCK_CLIENT(client);
}


/*
 *
 * event_add_action()
 *
 */
LOCAL void event_add_action(
struct extmsg *mp,
struct client  *client
)
{
	struct channel_in_use *pciu;
	struct event_ext *pevext;
	int		status;
	int		size;

	pciu = MPTOPCIU(mp);
	if(!pciu){
		logBadId(client, mp);
		return;
	}

	size = db_sizeof_event_block() + sizeof(*pevext);
	FASTLOCK(&rsrv_free_eventq_lck);
	pevext = (struct event_ext *) 
		ellGet(&rsrv_free_eventq);
	FASTUNLOCK(&rsrv_free_eventq_lck);
	if (!pevext) {
		pevext = (struct event_ext *) malloc(size);
		if (!pevext) {
			LOCK_CLIENT(client);
			send_err(
				mp,
				ECA_ALLOCMEM, 
				client, 
				RECORD_NAME(&pciu->addr));
			UNLOCK_CLIENT(client);
			return;
		}
	}
	memset(pevext,0,size);
	pevext->msg = *mp;
	pevext->pciu = pciu;
	pevext->send_lock = TRUE;
	pevext->size = dbr_size_n(mp->m_type, mp->m_count);
	pevext->mask = ((struct monops *) mp)->m_info.m_mask;
	pevext->get = FALSE;

	LOCK_CLIENT(client);
	ellAdd(	&pciu->eventq, &pevext->node);
	UNLOCK_CLIENT(client);

	/*
	 * dont set up the event if no read access
	 */
	if(!asCheckGet(pciu->asClientPVT)){
		pevext->pdbev = NULL;
		no_read_access_event(client, pevext);
		return;
	}

	pevext->pdbev = (struct event_block *)(pevext+1);

	status = db_add_event(
			client->evuser,
		      	&pciu->addr,
		      	read_reply,
		      	pevext,
	   		pevext->mask,
			pevext->pdbev);
	if (status == ERROR) {
		pevext->pdbev = NULL;
		LOCK_CLIENT(client);
		send_err(
			mp, 
			ECA_ADDFAIL, 
			client, 
			RECORD_NAME(&pciu->addr));
		UNLOCK_CLIENT(client);
		return;
	}

	/*
	 * always send it once at event add
	 */
	/*
	 * if the client program issues many monitors
	 * in a row then I recv when the send side
	 * of the socket would block. This prevents
	 * a application program initiated deadlock.
	 *
	 * However when I am reconnecting I reissue 
	 * the monitors and I could get deadlocked.
	 * The client is blocked sending and the server
	 * task for the client is blocked sending in
	 * this case. I cant check the recv part of the
	 * socket in the client since I am still handling an
	 * outstanding recv ( they must be processed in order).
	 * I handle this problem in the server by using
	 * post_single_event() below instead of calling
	 * read_reply() in this module. This is a complete
	 * fix since a monitor setup is the only request
	 * soliciting a reply in the client which is 
	 * issued from inside of service.c (from inside
	 * of the part of the ca client which services
	 * messages sent by the server).
	 */

	db_post_single_event(pevext->pdbev);

	return;
}



/*
 *
 *	clear_channel_reply()
 *
 *
 */
LOCAL void clear_channel_reply(
struct extmsg *mp,
struct client  *client
)
{
        struct extmsg *reply;
        struct event_ext *pevext;
        struct channel_in_use *pciu;
        int        status;

        /*
         *
         * Verify the channel
         *
         */
	pciu = MPTOPCIU(mp);
	if(pciu?pciu->client!=client:TRUE){
		logBadId(client, mp);
		return;
	}

	/*
	 * if a put notify is outstanding then cancel it
	 */
	if(pciu->pPutNotify){
		if(pciu->pPutNotify->busy){
			dbNotifyCancel(&pciu->pPutNotify->dbPutNotify);
		}
	}

        while (TRUE){
		LOCK_CLIENT(client);
		pevext = (struct event_ext *) ellGet(&pciu->eventq);
		UNLOCK_CLIENT(client);

		if(!pevext){
			break;
		}

		if(pevext->pdbev){
			status = db_cancel_event(pevext->pdbev);
			assert(status == OK);
		}
		FASTLOCK(&rsrv_free_eventq_lck);
		ellAdd(&rsrv_free_eventq, &pevext->node);
		FASTUNLOCK(&rsrv_free_eventq_lck);
        }

	status = db_flush_extra_labor_event(client->evuser);
	if(status){
		taskSuspend(0);
	}

	if(pciu->pPutNotify){
		free(pciu->pPutNotify);
	}

	/*
	 * remove from access control list
	 */
	status = asRemoveClient(&pciu->asClientPVT);
	if(status){
		taskSuspend(0);
	}

	/*
	 * send delete confirmed message
	 */
	LOCK_CLIENT(client);
	reply = (struct extmsg *) ALLOC_MSG(client, 0);
	if (!reply) {
		UNLOCK_CLIENT(client);
		taskSuspend(0);
	}
	*reply = *mp;

	END_MSG(client);
	ellDelete(&client->addrq, &pciu->node);
	UNLOCK_CLIENT(client);

	FASTLOCK(&rsrv_free_addrq_lck);
	status = bucketRemoveItem(pCaBucket, pciu->sid, pciu);
	if(status != BUCKET_SUCCESS){
		logBadId(client, mp);
	}
	ellAdd(&rsrv_free_addrq, &pciu->node);
	FASTUNLOCK(&rsrv_free_addrq_lck);

	return;
}




/*
 *
 *	event_cancel_reply()
 *
 *
 * Much more efficient now since the event blocks hang off the channel in use
 * blocks not all together off the client block.
 */
LOCAL void event_cancel_reply(
struct extmsg *mp,
struct client  *client
)
{
        struct channel_in_use 	*pciu;
	struct extmsg 		*reply;
	struct event_ext 	*pevext;
	int        		status;

        /*
         *
         * Verify the channel
         *
         */
	pciu = MPTOPCIU(mp);
	if(pciu?pciu->client!=client:TRUE){
		logBadId(client, mp);
		return;
	}

	/*
	 * search events on this channel for a match
	 * (there are usually very few monitors per channel)
	 */
	LOCK_CLIENT(client);
	for (pevext = (struct event_ext *) ellFirst(&pciu->eventq);
	     pevext;
	     pevext = (struct event_ext *) ellNext(&pevext->node)){

		if (pevext->msg.m_available == mp->m_available) {
			ellDelete(&pciu->eventq, &pevext->node);
			break;
		}
	}
	UNLOCK_CLIENT(client);

	/*
	 * Not Found- return an exception event 
	 */
	if(!pevext){
		LOCK_CLIENT(client);
		send_err(mp, ECA_BADMONID, client, RECORD_NAME(&pciu->addr));
		UNLOCK_CLIENT(client);
		return;
	}


	/*
	 * cancel monitor activity in progress
	 */
	if(pevext->pdbev){
		status = db_cancel_event(pevext->pdbev);
		assert(status == OK);
	}

	/*
	 * send delete confirmed message
	 */
	LOCK_CLIENT(client);
	reply = (struct extmsg *) ALLOC_MSG(client, 0);
	if (!reply) {
		UNLOCK_CLIENT(client);
		assert(0);
	}
	*reply = pevext->msg;
	reply->m_postsize = 0;

	END_MSG(client);
	UNLOCK_CLIENT(client);

	FASTLOCK(&rsrv_free_eventq_lck);
	ellAdd(&rsrv_free_eventq, &pevext->node);
	FASTUNLOCK(&rsrv_free_eventq_lck);
}



/*
 *
 *	read_reply()
 *
 *
 */
LOCAL void read_reply(
void			*pArg,
struct db_addr 		*paddr,
int 			eventsRemaining,
db_field_log		*pfl
)
{
	struct event_ext *pevext = pArg;
	struct client *client = pevext->pciu->client;
        struct channel_in_use *pciu = pevext->pciu;
	struct extmsg *reply;
	int        status;
	int        strcnt;
	int		v41;

	/*
	 * If flow control is on set modified and send for later
 	 * (only if this is not a get)
	 */
	if (client->eventsoff && !pevext->get) {
		pevext->modified = TRUE;
		return;
	}
	if (pevext->send_lock)
		LOCK_CLIENT(client);

	reply = (struct extmsg *) ALLOC_MSG(client, pevext->size);
	if (!reply) {
		send_err(&pevext->msg, ECA_TOLARGE, client, RECORD_NAME(paddr));
		if (!eventsRemaining)
			cas_send_msg(client,!pevext->send_lock);
		if (pevext->send_lock)
			UNLOCK_CLIENT(client);
		return;
	}

	/* 
	 * setup response message
	 */
	*reply = pevext->msg;
	reply->m_postsize = pevext->size;
	reply->m_cid = (unsigned long)pciu->cid;

	/*
	 * verify read access
	 */
	v41 = CA_V41(CA_PROTOCOL_VERSION,client->minor_version_number);
	if(!asCheckGet(pciu->asClientPVT)){
		if(reply->m_cmmd==IOC_READ||
			reply->m_cmmd==IOC_READ_BUILD){
			if(v41){
				status = ECA_NORDACCESS;
			}
			else{
				status = ECA_GETFAIL;
			}
			/*
			 * old client & plain get & search/get
			 * continue to return an exception
			 * on failure
			 */
			send_err(&pevext->msg, status, 
				client, RECORD_NAME(paddr));
		}
		else{
			no_read_access_event(client, pevext);
		}
		if (!eventsRemaining)
			cas_send_msg(client,!pevext->send_lock);
		if (pevext->send_lock){
			UNLOCK_CLIENT(client);
		}
		return;
	}
	status = db_get_field(
			      paddr,
			      pevext->msg.m_type,
			      reply + 1,
			      pevext->msg.m_count,
			      pfl);
	if (status < 0) {
		/*
		 * I cant wait to redesign this protocol from scratch!
		 */
		if(!v41||reply->m_cmmd==IOC_READ||reply->m_cmmd==IOC_READ_BUILD){
			/*
			 * old client & plain get & search/get
			 * continue to return an exception
			 * on failure
			 */
			send_err(&pevext->msg, ECA_GETFAIL, client, RECORD_NAME(paddr));
			log_header(&pevext->msg, 0);
		}
		else{
			/*
			 * New clients recv the status of the
			 * operation directly to the
			 * event/put/get callback.
			 *
			 * Fetched value is zerod in case they
			 * use it even when the status indicates 
			 * failure.
			 *
			 * The m_cid field in the protocol
			 * header is abused to carry the status
			 */
			bzero((char *)(reply+1), pevext->size);
			reply->m_cid = ECA_GETFAIL;
			END_MSG(client);
		}
	}
	else{
		/*
		 * New clients recv the status of the
		 * operation directly to the
		 * event/put/get callback.
		 *
		 * The m_cid field in the protocol
		 * header is abused to carry the status
		 *
		 * get &search/get calls still use the
		 * m_cid field to identify the channel.
		 */
		if(	v41&&
			reply->m_cmmd!=IOC_READ&&
			reply->m_cmmd!=IOC_READ_BUILD ){
			reply->m_cid = ECA_NORMAL;
		}
		
		/*
		 * force string message size to be the true size rounded to even
		 * boundary
		 */
		if (pevext->msg.m_type == DBR_STRING 
			&& pevext->msg.m_count == 1) {
			/* add 1 so that the string terminator will be shipped */
			strcnt = strlen((char *)(reply + 1)) + 1;
			reply->m_postsize = strcnt;
		}

		END_MSG(client);
	}

	/*
	 * Ensures timely response for events, but does que 
	 * them up like db requests when the OPI does not keep up.
	 */
	if (!eventsRemaining)
		cas_send_msg(client,!pevext->send_lock);

	if (pevext->send_lock)
		UNLOCK_CLIENT(client);

	return;
}


/*
 * no_read_access_event()
 *
 * !! LOCK needs to applied by caller !!
 *
 * substantial complication introduced here by the need for backwards
 * compatibility
 */
LOCAL void no_read_access_event(
struct client  		*client,
struct event_ext 	*pevext
)
{
	struct extmsg 	*reply;
	int 		v41;

	v41 = CA_V41(CA_PROTOCOL_VERSION,client->minor_version_number);

	/*
	 * continue to return an exception
	 * on failure to pre v41 clients
	 */
	if(!v41){
		send_err(
			&pevext->msg, 
			ECA_GETFAIL, 
			client, 
			RECORD_NAME(&pevext->pciu->addr));
		return;
	}

	reply = (struct extmsg *) ALLOC_MSG(client, pevext->size);
	if (!reply) {
		send_err(
			&pevext->msg, 
			ECA_TOLARGE, 
			client, 
			RECORD_NAME(&pevext->pciu->addr));
		return;
	}
	else{
		/*
		 * New clients recv the status of the
		 * operation directly to the
		 * event/put/get callback.
		 *
		 * Fetched value is zerod in case they
		 * use it even when the status indicates 
		 * failure.
		 *
		 * The m_cid field in the protocol
		 * header is abused to carry the status
		 */
		bzero((char *)(reply+1), pevext->size);
		reply->m_cid = ECA_NORDACCESS;
		END_MSG(client);
	}
}


/*
 *
 *	read_sync_reply()
 *
 *
 */
LOCAL void read_sync_reply(
struct extmsg *mp,
struct client  *client
)
{
	FAST struct extmsg *reply;

	LOCK_CLIENT(client);
	reply = (struct extmsg *) ALLOC_MSG(client, 0);
	if (!reply)
		taskSuspend(0);

	*reply = *mp;

	END_MSG(client);

	UNLOCK_CLIENT(client);

	return;
}


/*
 *
 *	build_reply()
 *
 *
 */
LOCAL void build_reply(
struct extmsg *mp,
struct client  *client
)
{
	struct extmsg 		*search_reply;
	struct extmsg 		*get_reply;
	unsigned short		*pMinorVersion;
	int        		status;
	struct db_addr  	tmp_addr;
	struct channel_in_use 	*pchannel;
	unsigned long		sid;


	/* Exit quickly if channel not on this node */
	status = db_name_to_addr(
			mp->m_cmmd == IOC_BUILD ? mp + 2 : mp + 1, 
			&tmp_addr);
	if (status < 0) {
		if (CASDEBUG > 2)
			logMsg(	"CAS: Lookup for channel \"%s\" failed\n", 
				(int)(mp+1),
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
		if (mp->m_type == DOREPLY)
			search_fail_reply(mp, client);
		return;
	}


	/* get block off free list if possible */
	FASTLOCK(&rsrv_free_addrq_lck);
	pchannel = (struct channel_in_use *) ellGet(&rsrv_free_addrq);
	FASTUNLOCK(&rsrv_free_addrq_lck);
	if (!pchannel) {
		pchannel = (struct channel_in_use *) malloc(sizeof(*pchannel));
		if (!pchannel) {
			LOCK_CLIENT(client);
			send_err(mp, ECA_ALLOCMEM, client, RECORD_NAME(&tmp_addr));
			UNLOCK_CLIENT(client);
			return;
		}
	}
	memset((char *)pchannel, 0, sizeof(*pchannel));
	ellInit(&pchannel->eventq);
	pchannel->ticks_at_creation = tickGet();
	pchannel->addr = tmp_addr;
	pchannel->client = client;
	pchannel->cid = mp->m_cid;

	/*
	 * set up access security for this channel
	 *
	 * done here rather than at channel claim so
	 * that search reply will fail if there 
	 * isnt enough memory.
	 */
	status = asAddClient(
			&pchannel->asClientPVT,
			asDbGetMemberPvt(&pchannel->addr),
			asDbGetAsl(&pchannel->addr),
			"",
			"");	
	if(status){
		LOCK_CLIENT(client);
		send_err(mp, ECA_ALLOCMEM, client, "No room for security table");
		UNLOCK_CLIENT(client);
		FASTLOCK(&rsrv_free_addrq_lck);
		ellAdd(&rsrv_free_addrq, &pchannel->node);
		FASTUNLOCK(&rsrv_free_addrq_lck);
		return;
	}
	asPutClientPvt(pchannel->asClientPVT, pchannel);

	/*
	 * Existing build() interface to the client does not provide mechanism
	 * to inform them that the channel connected but the value
	 * couldnt be fetched so search/get combined op
	 * to no read access channel not allowed.
	 */
	if (mp->m_cmmd == IOC_BUILD && !asCheckGet(pchannel->asClientPVT)) {
		LOCK_CLIENT(client);
		send_err(mp, ECA_NORDACCESS, client, RECORD_NAME(&tmp_addr));
		UNLOCK_CLIENT(client);
		asRemoveClient(&pchannel->asClientPVT);
		FASTLOCK(&rsrv_free_addrq_lck);
		ellAdd(&rsrv_free_addrq, &pchannel->node);
		FASTUNLOCK(&rsrv_free_addrq_lck);
		return;
	}

	/*
	 * allocate a server id and enter the channel pointer
	 * in the table
	 */
	FASTLOCK(&rsrv_free_addrq_lck);
	sid = bucketID++;
	pchannel->sid = sid;
	status = bucketAddItem(pCaBucket, sid, pchannel);
	FASTUNLOCK(&rsrv_free_addrq_lck);
	if(status!=BUCKET_SUCCESS){
		LOCK_CLIENT(client);
		send_err(mp, ECA_ALLOCMEM, client, "No room for hash table");
		UNLOCK_CLIENT(client);
		asRemoveClient(&pchannel->asClientPVT);
		FASTLOCK(&rsrv_free_addrq_lck);
		ellAdd(&rsrv_free_addrq, &pchannel->node);
		FASTUNLOCK(&rsrv_free_addrq_lck);
		return;
	}

	/*
	 * UDP reliability schemes rely on both msgs in same reply Therefore
	 * the send buffer locked while both messages are placed
	 */
	LOCK_CLIENT(client);


	if (mp->m_cmmd == IOC_BUILD) {
		short      type = (mp + 1)->m_type;
		unsigned int count = (mp + 1)->m_count;
		unsigned int size;

		/*
		 * must be large enough to hold both the search and the build-get
		 * reply in one UDP message. Hence the extra sizeof(*mp) added 
		 * in below.
		 */
		size = 	sizeof(*mp) + 		/* search reply hdr size 	*/
			sizeof(*pMinorVersion) +/* version id after search hdr	*/
			sizeof(*mp) +		/* build get reply hdr size 	*/
			+ dbr_size_n(type, count);/* size of the structure fetched */

		get_reply = (struct extmsg *) ALLOC_MSG(client, size);
		if (!get_reply) {
			/* tell them that their request is to large */
			send_err(mp, ECA_TOLARGE, client, RECORD_NAME(&tmp_addr));
			UNLOCK_CLIENT(client);
			asRemoveClient(&pchannel->asClientPVT);
			FASTLOCK(&rsrv_free_addrq_lck);
			bucketRemoveItem(
					pCaBucket, 
					pchannel->sid, 
					pchannel);
			FASTUNLOCK(&rsrv_free_addrq_lck);
			FASTLOCK(&rsrv_free_addrq_lck);
			ellAdd(&rsrv_free_addrq, &pchannel->node);
			FASTUNLOCK(&rsrv_free_addrq_lck);
			return;
		} else {
			struct event_ext evext;

			evext.pciu = pchannel;
			evext.msg = *(mp+1);
			/*
			 * this allows extra build replies 
			 * to be dicarded
			 */
			evext.msg.m_cmmd = IOC_READ_BUILD;
			evext.msg.m_cid = sid;
			evext.send_lock = FALSE;
			evext.size = dbr_size_n(type, count);
			evext.pdbev = NULL;
			evext.get = TRUE;

			/*
			 * Arguments to this routine organized in favor of
			 * the standard	db event calling mechanism-
			 * routine(userarg, paddr). See events added above.
			 * Hold argument set true so the send message buffer
			 * is not flushed once each call.
			 */
			read_reply(&evext, &tmp_addr, TRUE, NULL);
		}
	}
	search_reply = (struct extmsg *) 
		ALLOC_MSG(client, sizeof(*pMinorVersion));
	if (!search_reply)
		taskSuspend(0);

	*search_reply = *mp;
	search_reply->m_postsize = sizeof(*pMinorVersion);

	/* this field for rmt machines where paddr invalid */
	search_reply->m_type = tmp_addr.field_type;
	search_reply->m_count = tmp_addr.no_elements;
	search_reply->m_cid = sid;

	/*
	 * Starting with CA V4.1 the minor version number
	 * is appended to the end of each search reply.
	 * This value is ignored by earlier clients. 
	 */
	pMinorVersion = (unsigned short *)(search_reply+1);
	*pMinorVersion = htons(CA_MINOR_VERSION);

	END_MSG(client);

	/* store the addr block on the cast queue until it is claimed */
	ellAdd(&client->addrq, &pchannel->node);

	UNLOCK_CLIENT(client);

	return;
}


/* 	search_fail_reply()
 *
 *	Only when requested by the client 
 *	send search failed reply
 *
 *
 */
LOCAL void search_fail_reply(
struct extmsg *mp,
struct client  *client
)
{
	FAST struct extmsg *reply;

	LOCK_CLIENT(client);
	reply = (struct extmsg *) ALLOC_MSG(client, 0);
	if (!reply) {
		taskSuspend(0);
	}
	*reply = *mp;
	reply->m_cmmd = IOC_NOT_FOUND;
	reply->m_postsize = 0;

	END_MSG(client);
	UNLOCK_CLIENT(client);

}


/* 	send_err()
 *
 *	reflect error msg back to the client
 *
 *	send buffer lock must be on while in this routine
 *
 */
LOCAL void send_err(
struct extmsg  *curp,
int            status,
struct client  *client,
char           *footnote
)
{
        struct channel_in_use 	*pciu;
	int        		size;
	struct extmsg 		*reply;


	/*
	 * force string post size to be the true size rounded to even
	 * boundary
	 */
	size = strlen(footnote)+1;
	size += sizeof(*curp);

	reply = (struct extmsg *) ALLOC_MSG(client, size);
	if (!reply){
		printf(	"caserver: Unable to deliver err msg [%s]\n",
			ca_message(status));
		return;
	}

	reply[0] = nill_msg;
	reply[0].m_cmmd = IOC_ERROR;
	reply[0].m_available = status;
	reply[0].m_postsize = size;

	switch (curp->m_cmmd) {
	case IOC_EVENT_ADD:
	case IOC_EVENT_CANCEL:
	case IOC_READ:
	case IOC_READ_NOTIFY:
	case IOC_SEARCH:
	case IOC_BUILD:
	case IOC_WRITE:
	case IOC_WRITE_NOTIFY:
	        /*
		 *
		 * Verify the channel
		 *
		 */
		pciu = MPTOPCIU(curp);
		if(pciu){
			reply->m_cid = (unsigned long) pciu->cid;
		}
		else{
			reply->m_cid = ~0L;
		}
		break;

	case IOC_EVENTS_ON:
	case IOC_EVENTS_OFF:
	case IOC_READ_SYNC:
	case IOC_SNAPSHOT:
	default:
		reply->m_cid = NULL;
		break;
	}
	reply[1] = *curp;
	strcpy((char *)(reply + 2), footnote);

	END_MSG(client);

}


/*
 * logBadId()
 */
LOCAL void logBadId(
struct client *client, 
struct extmsg *mp
)
{
	log_header(mp,0);
	LOCK_CLIENT(client);
	send_err(mp, ECA_INTERNAL, client, "ID lookup failed");
	UNLOCK_CLIENT(client);
}


/* 	log_header()
 *
 *	Debug aid - print the header part of a message.
 *
 */
LOCAL void log_header(
struct extmsg 	*mp,
int		mnum
)
{
        struct channel_in_use *pciu;

	pciu = MPTOPCIU(mp);

	logMsg(	"CAS: cmd=%d cid=%x typ=%d cnt=%d psz=%d avail=%x\n",
	  	mp->m_cmmd,
		mp->m_cid,
	  	mp->m_type,
	  	mp->m_count,
	  	mp->m_postsize,
	  	(int)mp->m_available);

	logMsg(	"CAS: \tN=%d paddr=%x\n",
	  	mnum, 
	  	(int)(pciu?&pciu->addr:NULL),
		NULL,
		NULL,
		NULL,
		NULL);

  	if(mp->m_cmmd==IOC_WRITE && mp->m_type==DBF_STRING)
    		logMsg("CAS: The string written: %s \n",
			(int)(mp+1),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
}



/*
 *
 *      cac_send_heartbeat()
 *
 *	lock must be applied while in this routine
 */
void cas_send_heartbeat(
struct client	*pc
)
{
        FAST struct extmsg 	*reply;

        reply = (struct extmsg *) ALLOC_MSG(pc, 0);
        if(!reply){
                taskSuspend(0);
	}

	*reply = nill_msg;
        reply->m_cmmd = IOC_NOOP;

        END_MSG(pc);

        return;
}



/*
 * MPTOPCIU()
 *
 * used to be a macro
 */
LOCAL struct channel_in_use *MPTOPCIU(struct extmsg *mp)
{
	struct channel_in_use *pciu;

	FASTLOCK(&rsrv_free_addrq_lck);
	pciu = bucketLookupItem(pCaBucket, mp->m_cid);
	FASTUNLOCK(&rsrv_free_addrq_lck);

	return pciu;
}


/*
 * casAccessRightsCB()
 *
 * If access right state changes then inform the client.
 *
 */
LOCAL void casAccessRightsCB(ASCLIENTPVT ascpvt, asClientStatus type)
{
	struct client		*pclient;
	struct channel_in_use 	*pciu;
        struct extmsg 		*reply;
	struct event_ext	*pevext;
	int			status;
	unsigned		ar;
	int			v41;

	pciu = asGetClientPvt(ascpvt);
	if(!pciu){
		printf("casAccessRightsCB() without channel pointer ??\n");
		return;
	}
	pclient = pciu->client;
	assert(pclient);
	v41 = CA_V41(CA_PROTOCOL_VERSION,pclient->minor_version_number);

	switch(type)
	{
	case asClientCOAR:
		/*
		 * noop if this is an old client
		 */
		if(!v41){
			break;
		}

		ar = 0; /* none */
		if(asCheckGet(ascpvt)){
			ar |= CA_ACCESS_RIGHT_READ;
		}
		if(asCheckPut(ascpvt)){
			ar |= CA_ACCESS_RIGHT_WRITE;
		}

		LOCK_CLIENT(pclient);
        	reply = (struct extmsg *)ALLOC_MSG(pclient, 0);
		assert(reply);

		*reply = nill_msg;
        	reply->m_cmmd = IOC_ACCESS_RIGHTS;
		reply->m_cid = pciu->cid;
		reply->m_available = ar;

        	END_MSG(pclient);

		/*
		 * Update all event call backs 
		 */
		for (pevext = (struct event_ext *) ellFirst(&pciu->eventq);
		     pevext;
		     pevext = (struct event_ext *) ellNext(&pevext->node)){

			if(pevext->pdbev && !(ar&CA_ACCESS_RIGHT_READ)){
				status = db_cancel_event(pevext->pdbev);
				assert(status == OK);
				pevext->pdbev = NULL;
			}
			else if(!pevext->pdbev && ar&CA_ACCESS_RIGHT_READ){
				pevext->pdbev = 
					(struct event_block *)(pevext+1);

				status = db_add_event(
						pclient->evuser,
		    				&pciu->addr,
		      				read_reply,
		      				pevext,
	   					pevext->mask,
						pevext->pdbev);
				if (status == ERROR) {
					pevext->pdbev = NULL;
						send_err(
							&pevext->msg, 
							ECA_ADDFAIL, 
							pclient, 
							RECORD_NAME(&pciu->addr));
				}
			}

			if(pevext->pdbev){
				db_post_single_event(pevext->pdbev);
			}
		}

		UNLOCK_CLIENT(pclient);

		break;

	default:
		break;
	}
}

