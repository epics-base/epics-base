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
 *	.04 joh	071291	code for CA_PROTO_CLAIM_CIU command
 *	.05 joh 082691  use db_post_single_event() instead of read_reply()
 *			to avoid deadlock condition between the client
 *			and the server.
 *	.06 joh	110491	lock added for CA_PROTO_CLAIM_CIU command
 *	.07 joh	021292	Better diagnostics
 *	.08 joh	021492	use lstFind() to verify chanel in clear_channel()
 *	.09 joh 031692	dont send exeception to the client if bad msg
 *			detected- just disconnect instead
 *	.10 joh	050692	added new routine - cas_send_heartbeat()
 *	.11 joh 062492	dont allow flow control to turn off gets
 *	.12 joh 090893	converted pointer to server id	
 *	.13 joh 091493	made events on action a subroutine for debugging
 *	.14 joh 020194	New command added for CAV4.1 - client name
 *	.15 joh 052594  Removed CA_PROTO_BUILD cmd 	
 */


static char *sccsId = "%W% %G%";

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <types.h>
#include <in.h>
#include <logLib.h>
#include <tickLib.h>
#include <stdioLib.h>
#include <sysLib.h>

#include "db_access.h"
#include "task_params.h"
#include "ellLib.h"
#include "freeList.h"
#include "caerr.h"

#include "server.h"

static caHdr nill_msg;

#define	RECORD_NAME(PADDR) ((PADDR)->precord->name)

LOCAL void clear_channel_reply(
caHdr *mp,
struct client  *client
);

LOCAL void event_cancel_reply(
caHdr *mp,
struct client  *client
);

LOCAL EVENTFUNC       read_reply;

LOCAL void read_sync_reply(
caHdr *mp,
struct client  *client
);

LOCAL void search_reply(
caHdr *mp,
struct client  *client
);

LOCAL void search_fail_reply(
caHdr 	*mp,
struct client  	*client
);

LOCAL void send_err(
caHdr  *curp,
int            status,
struct client  *client,
char           *footnote,
		...
);

LOCAL void log_header(
caHdr 	*mp,
int		mnum
);

LOCAL void event_add_action(
caHdr *mp,
struct client  *client
);

LOCAL void logBadIdWithFileAndLineno(
struct client 	*client, 
caHdr 	*mp,
char		*pFileName,
unsigned	lineno
);

#define logBadId(CLIENT, MP)\
logBadIdWithFileAndLineno(CLIENT, MP, __FILE__, __LINE__)

LOCAL struct channel_in_use *MPTOPCIU(
caHdr *mp
);

LOCAL void write_notify_call_back(PUTNOTIFY *ppn);

LOCAL void write_notify_action(
caHdr *mp,
struct client  *client
);

LOCAL void putNotifyErrorReply(
struct client *client, 
caHdr *mp, 
int statusCA
);

LOCAL void claim_ciu_action(
caHdr *mp,
struct client  *client
);

LOCAL void client_name_action(
caHdr 	*mp,
struct client  	*client
);

LOCAL void host_name_action(
caHdr 	*mp,
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

LOCAL void access_rights_reply(
struct channel_in_use *pciu
);

LOCAL struct channel_in_use *casCreateChannel (
struct client  	*client,
struct dbAddr 	*pAddr,
unsigned	cid
);

LOCAL void *casCalloc (size_t count, size_t size);

LOCAL void *casMalloc (size_t size);


/*
 * CAMESSAGE()
 */
int camessage(
struct client  *client,
struct message_buffer *recv
)
{
	unsigned long	 	tmp_postsize;
	int            		nmsg = 0;
	int			v41;
	unsigned long		msgsize;
	unsigned long		bytes_left;
	int             	status;
	caHdr 			*mp;
	struct channel_in_use 	*pciu;

	if(!pCaBucket){
		pCaBucket = bucketCreate(CAS_HASH_TABLE_SIZE);
		if(!pCaBucket){
			return ERROR;
		}
	}

	DLOG (2, "CAS: Parsing %d(decimal) bytes\n", 
		recv->cnt, NULL, NULL, NULL, NULL, NULL);

	bytes_left = recv->cnt;
	while (bytes_left)
	{

		/* assert that we have at least a complete caHdr */
		if(bytes_left < sizeof(*mp))
			return OK;

		mp = (caHdr *) &recv->buf[recv->stk];

		/* problem: we have a complete header,
		 * but before we check msgsize we don't know
		 * if we have a complete message body
		 * -> we may be called again with the same header
		 *    after receiving the full message
		 */
		tmp_postsize = ntohs (mp->m_postsize);
		msgsize = tmp_postsize + sizeof(*mp);

		if(msgsize > bytes_left) 
			return OK;

		/* Have complete message (header + content)
		 * -> convert the header elements
		 */
		mp->m_cmmd      = ntohs (mp->m_cmmd);
		mp->m_postsize  = tmp_postsize;
		mp->m_type      = ntohs (mp->m_type);
		mp->m_count     = ntohs (mp->m_count);
		mp->m_cid       = ntohl (mp->m_cid);
		mp->m_available = ntohl (mp->m_available);

		nmsg++;

		if (CASDEBUG > 2)
			log_header(mp, nmsg);

		switch (mp->m_cmmd) {
		case CA_PROTO_NOOP:	/* verify TCP */
			break;

		case CA_PROTO_ECHO:	/* verify TCP */
		{
			caHdr 	*reply; 

			SEND_LOCK(client);
			reply = ALLOC_MSG(client, 0);
			assert (reply);
			/*
			 * header (host) will we converted in send,
			 * content is still in net format:
			 */
			*reply = *mp;
			END_MSG(client);
			SEND_UNLOCK(client);
			break;
		}
		case CA_PROTO_CLIENT_NAME:
			client_name_action(mp, client);
			break;

		case CA_PROTO_HOST_NAME:
			host_name_action(mp, client);
			break;
		
		case CA_PROTO_EVENT_ADD:
			event_add_action(mp, client);
			break;

		case CA_PROTO_EVENT_CANCEL:
			event_cancel_reply(mp, client);
			break;

		case CA_PROTO_CLEAR_CHANNEL:
			clear_channel_reply(mp, client);
			break;

		case CA_PROTO_READ_NOTIFY:
		case CA_PROTO_READ:
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
		case CA_PROTO_SEARCH:
			search_reply(mp, client);
			break;

		case CA_PROTO_WRITE_NOTIFY:
			write_notify_action(mp, client);
			break;

		case CA_PROTO_WRITE:
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
				SEND_LOCK(client);
				send_err(
					mp, 
					status, 
					client, 
					RECORD_NAME(&pciu->addr));
				SEND_UNLOCK(client);
				break;
			}

#ifdef CONVERSION_REQUIRED
			if (mp->m_type >= NELEMENTS(cac_dbr_cvrt)) {
				SEND_LOCK(client);
				send_err(
					mp, 
					ECA_PUTFAIL, 
					client, 
					RECORD_NAME(&pciu->addr));
				SEND_UNLOCK(client);
				break;
			}

			/* use type as index into conversion jumptable */
			(* cac_dbr_cvrt[mp->m_type])
				( mp + 1,
				  mp + 1,
				  FALSE,       /* net -> host format */
				  mp->m_count);
#endif
			status = db_put_field(
					      &pciu->addr,
					      mp->m_type,
					      mp + 1,
					      mp->m_count);
			if (status < 0) {
				SEND_LOCK(client);
				send_err(
					mp, 
					ECA_PUTFAIL, 
					client, 
					RECORD_NAME(&pciu->addr));
				SEND_UNLOCK(client);
			}
			break;

		case CA_PROTO_EVENTS_ON:
			db_event_flow_ctrl_mode_off(client->evuser);
			break;

		case CA_PROTO_EVENTS_OFF:
			db_event_flow_ctrl_mode_on(client->evuser);
			break;

		case CA_PROTO_READ_SYNC:
			read_sync_reply(mp, client);
			break;

		case CA_PROTO_CLAIM_CIU:
			claim_ciu_action(mp, client);
			break;

		case CA_PROTO_READ_BUILD:
		case CA_PROTO_BUILD:
			/*
			 * starting with 3.12 CA ignores this 
			 * protocol. No message is sent so that we avoid
			 * a broadcast storm.
			 */
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
			SEND_LOCK(client);
			send_err(mp, ECA_INTERNAL, client, "Invalid Msg");
			SEND_UNLOCK(client);
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
			logMsg("recv->stk: %d, bytes_left: %d\n",
				(int) recv->stk, (int) bytes_left,
				0, 0, 0, 0);
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
caHdr 	*mp,
struct client  	*client
)
{
	struct channel_in_use 	*pciu;
	unsigned		size;
	char 			*pName;
	char 			*pMalloc;
	int			status;

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
	strncpy(
		pMalloc, 
		pName, 
		size-1);
	pMalloc[size-1]='\0';

	FASTLOCK(&client->addrqLock);
	pName = client->pHostName;
	client->pHostName = pMalloc;
	if(pName){
		free(pName);
	}

	pciu = (struct channel_in_use *) client->addrq.node.next;
	while(pciu){
		status = asChangeClient(
				pciu->asClientPVT,
				asDbGetAsl(&pciu->addr),
				client->pUserName,
				client->pHostName);	
		if(status != 0 && status != S_asLib_asNotActive){
			FASTUNLOCK(&client->addrqLock);
       			free_client(client);
			exit(0);
		}
		pciu = (struct channel_in_use *) pciu->node.next;
	}
	FASTUNLOCK(&client->addrqLock);

	DLOG(2, "CAS: host_name_action for \"%s\"\n", 
			(int) client->pHostName,
			NULL, NULL, NULL, NULL, NULL);
}


/*
 * client_name_action()
 */
LOCAL void client_name_action(
caHdr 	*mp,
struct client  	*client
)
{
	struct channel_in_use 	*pciu;
	unsigned		size;
	char 			*pName;
	char 			*pMalloc;
	int			status;

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
	strncpy(
		pMalloc, 
		pName, 
		size-1);
	pMalloc[size-1]='\0';

	FASTLOCK(&client->addrqLock);
	pName = client->pUserName;
	client->pUserName = pMalloc;
	if(pName){
		free(pName);
	}

	pciu = (struct channel_in_use *) client->addrq.node.next;
	while(pciu){
		status = asChangeClient(
				pciu->asClientPVT,
				asDbGetAsl(&pciu->addr),
				client->pUserName,
				client->pHostName);	
		if(status != 0 && status != S_asLib_asNotActive){
			FASTUNLOCK(&client->addrqLock);
       			free_client(client);
			exit(0);
		}
		pciu = (struct channel_in_use *) pciu->node.next;
	}
	FASTUNLOCK(&client->addrqLock);

	DLOG (2, "CAS: client_name_action for \"%s\"\n", 
			(int) client->pUserName,
			NULL, NULL, NULL, NULL, NULL);
}


/*
 * claim_ciu_action()
 */
LOCAL void claim_ciu_action(
caHdr *mp,
struct client  *client
)
{
	int			v42;
	int			status;
	struct channel_in_use 	*pciu;

	/*
	 * The available field is used (abused)
	 * here to communicate the miner version number
	 * starting with CA 4.1. The field was set to zero
	 * prior to 4.1
	 */
	client->minor_version_number = mp->m_available;

	if (CA_V44(CA_PROTOCOL_VERSION,client->minor_version_number)) {
		struct dbAddr  tmp_addr;

		status = db_name_to_addr(
				(char *)(mp+1), 
				&tmp_addr);
		if (status < 0) {
			return;
		}

		DLOG(2,"CAS: claim_ciu_action found '%s', type %d, count %d\n", 
			(int) (mp+1),
			tmp_addr.dbr_field_type,
			tmp_addr.no_elements,
			NULL, NULL, NULL);
		
		pciu = casCreateChannel (
				client, 
				&tmp_addr, 
				mp->m_cid);
		if (!pciu) {
			SEND_LOCK(client);
			send_err(mp, 
				ECA_ALLOCMEM, 
				client, 
				RECORD_NAME(&tmp_addr));
			SEND_UNLOCK(client);
			return;
		}
	}
	else {
		FASTLOCK(&prsrv_cast_client->addrqLock);
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
			FASTUNLOCK(&prsrv_cast_client->addrqLock);
			free_client(client);
			exit(0);
		}

		/* 
		 * duplicate claim message are unacceptable 
		 * (so we disconnect the client)
		 */
		if (pciu->client!=prsrv_cast_client) {
			logMsg("CAS: double claim disconnect id=%d\n",
				mp->m_cid,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
			FASTUNLOCK(&prsrv_cast_client->addrqLock);
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
		FASTUNLOCK(&prsrv_cast_client->addrqLock);

		FASTLOCK(&prsrv_cast_client->addrqLock);
		pciu->client = client;
		ellAdd(&client->addrq, &pciu->node);
		FASTUNLOCK(&prsrv_cast_client->addrqLock);
	}


	/*
	 * set up access security for this channel
	 */
	status = asAddClient(
			&pciu->asClientPVT,
			asDbGetMemberPvt(&pciu->addr),
			asDbGetAsl(&pciu->addr),
			client->pUserName,
			client->pHostName);	
	if(status != 0 && status != S_asLib_asNotActive){
		SEND_LOCK(client);
		send_err(mp, ECA_ALLOCMEM, client, "No room for security table");
		SEND_UNLOCK(client);
       		free_client(client);
		exit(0);
	}

	/*
	 * store ptr to channel in use block 
	 * in access security private
	 */
	asPutClientPvt(pciu->asClientPVT, pciu);

	v42 = CA_V42(
		CA_PROTOCOL_VERSION,
		client->minor_version_number);

	/*
	 * register for asynch updates of access rights changes
	 * (only after the lock is released, we are added to 
	 * the correct client, and the clients version is
	 * known)
	 */
	status = asRegisterClientCallback(
			pciu->asClientPVT, 
			casAccessRightsCB);
	if(status == S_asLib_asNotActive){
		/*
		 * force the initial update
		 */
		access_rights_reply(pciu);
	}
	else{
		assert(status==0);
	}

	if(v42){
		caHdr 	*claim_reply; 

		SEND_LOCK(client);
		claim_reply = (caHdr *) ALLOC_MSG(client, 0);
		assert (claim_reply);

		*claim_reply = nill_msg;
		claim_reply->m_cmmd = CA_PROTO_CLAIM_CIU;
		claim_reply->m_type = pciu->addr.dbr_field_type;
		claim_reply->m_count = pciu->addr.no_elements;
		claim_reply->m_cid = pciu->cid;
		claim_reply->m_available = pciu->sid;

		DBLOCK(3,
			printf ("claim_cui reply:\n");
			log_header (claim_reply, 0);
		)

		END_MSG(client);
		SEND_UNLOCK(client);
	}
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
	assert(pciu);
	assert(pciu->pPutNotify);

	if(!pciu->pPutNotify->busy){
		logMsg("Double DB put notify call back!!\n",
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL);
		return;
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
	caHdr		*preply;
	int			status;

	pClient = pArg;

	SEND_LOCK(pClient);
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

	cas_send_msg(pClient,FALSE);

	SEND_UNLOCK(pClient);

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
caHdr *mp,
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

	if (mp->m_type > LAST_BUFFER_TYPE) {
		putNotifyErrorReply(client, mp, ECA_BADTYPE);
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
		pciu->pPutNotify = (RSRVPUTNOTIFY *) 
			casCalloc(1, sizeof(*pciu->pPutNotify)+size);
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
#ifdef CONVERSION_REQUIRED
	/* use type as index into conversion jumptable */
	(* cac_dbr_cvrt[mp->m_type])
		( mp + 1,
		  pciu->pPutNotify->dbPutNotify.pbuffer,
		  FALSE,       /* net -> host format */
		  mp->m_count);
#else
	memcpy(pciu->pPutNotify->dbPutNotify.pbuffer, (char *)(mp+1), size);
#endif
	status = dbPutNotifyMapType(&pciu->pPutNotify->dbPutNotify, mp->m_type);
	if(status){
		putNotifyErrorReply(client, mp, ECA_PUTFAIL);
		pciu->pPutNotify->busy = FALSE;
		return;
	}

	status = dbPutNotify(&pciu->pPutNotify->dbPutNotify);
	if(status && status != S_db_Pending){
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
LOCAL void putNotifyErrorReply(struct client *client, caHdr *mp, int statusCA)
{
	caHdr		*preply;

	SEND_LOCK(client);
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
	SEND_UNLOCK(client);
}


/*
 *
 * event_add_action()
 *
 */
LOCAL void event_add_action(
caHdr *mp,
struct client  *client
)
{
	struct monops *pmo = (struct monops *) mp;
	struct channel_in_use *pciu;
	struct event_ext *pevext;
	int		status;
	int		size;

	if (client==prsrv_cast_client) {
		printf("cas: add event from UDP???\n");
		return;
	}

	pciu = MPTOPCIU(mp);
	if(!pciu){
		logBadId(client, mp);
		return;
	}

	pevext = (struct event_ext *) freeListMalloc(rsrvEventFreeList);
	size = db_sizeof_event_block() + sizeof(*pevext);
	if (!pevext) {
		SEND_LOCK(client);
		send_err(
			mp,
			ECA_ALLOCMEM, 
			client, 
			RECORD_NAME(&pciu->addr));
		SEND_UNLOCK(client);
		return;
	}

#ifdef CONVERSION_REQUIRED

	/* I convert here is the full message that we get,
	 * though only m_mask seems to be used
	 */
	dbr_ntohf (&pmo->m_info.m_lval , &pmo->m_info.m_lval);
	dbr_ntohf (&pmo->m_info.m_hval , &pmo->m_info.m_hval);
	dbr_ntohf (&pmo->m_info.m_toval, &pmo->m_info.m_toval);
	pmo->m_info.m_mask = ntohs (pmo->m_info.m_mask);
#endif

	memset(pevext,0,size);
	pevext->msg = *mp;
	pevext->pciu = pciu;
	pevext->send_lock = TRUE;
	pevext->size = dbr_size_n(mp->m_type, mp->m_count);
	pevext->mask = pmo->m_info.m_mask;

	FASTLOCK(&client->eventqLock);
	ellAdd(	&pciu->eventq, &pevext->node);
	FASTUNLOCK(&client->eventqLock);

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
		SEND_LOCK(client);
		send_err(
			mp, 
			ECA_ADDFAIL, 
			client, 
			RECORD_NAME(&pciu->addr));
		SEND_UNLOCK(client);
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

	DLOG(3, "event_add_action: db_post_single_event (0x%X)\n",
		(int) pevext->pdbev, 0, 0, 0, 0, 0);
	db_post_single_event(pevext->pdbev);

	/*
	 * disable future labor if no read access
	 */
	if(!asCheckGet(pciu->asClientPVT)){
		db_event_disable(pevext->pdbev);
		DLOG(3, "Disable event because cannot read\n",
			0, 0, 0, 0, 0, 0);
	}

	return;
}



/*
 *
 *	clear_channel_reply()
 *
 *
 */
LOCAL void clear_channel_reply(
caHdr *mp,
struct client  *client
)
{
        caHdr *reply;
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
		FASTLOCK(&client->eventqLock);
		pevext = (struct event_ext *) ellGet(&pciu->eventq);
		FASTUNLOCK(&client->eventqLock);

		if(!pevext){
			break;
		}

		if(pevext->pdbev){
			status = db_cancel_event(pevext->pdbev);
			assert(status == OK);
		}
		freeListFree(rsrvEventFreeList, pevext);
        }

	status = db_flush_extra_labor_event(client->evuser);
	if(status){
		taskSuspend(0);
	}

	if (pciu->pPutNotify) {
		free (pciu->pPutNotify);
	}

	/*
	 * send delete confirmed message
	 */
	SEND_LOCK(client);
	reply = (caHdr *) ALLOC_MSG(client, 0);
	if (!reply) {
		SEND_UNLOCK(client);
		taskSuspend(0);
	}
	*reply = *mp;

	END_MSG(client);
	SEND_UNLOCK(client);

	FASTLOCK(&client->addrqLock);
	ellDelete(&client->addrq, &pciu->node);
	FASTUNLOCK(&client->addrqLock);

	/*
	 * remove from access control list
	 */
	status = asRemoveClient(&pciu->asClientPVT);
	assert(status == 0 || status == S_asLib_asNotActive);
	if(status != 0 && status != S_asLib_asNotActive){
		errMessage(status, RECORD_NAME(&pciu->addr));
	}

	FASTLOCK(&clientQlock);
	status = bucketRemoveItemUnsignedId (pCaBucket, &pciu->sid);
	if(status != S_bucket_success){
		errMessage (status, "Bad resource id during channel clear");
		logBadId(client, mp);
	}
	FASTUNLOCK(&clientQlock);
	freeListFree(rsrvChanFreeList, pciu);

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
caHdr *mp,
struct client  *client
)
{
        struct channel_in_use 	*pciu;
	caHdr 		*reply;
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
	FASTLOCK(&client->eventqLock);
	for (pevext = (struct event_ext *) ellFirst(&pciu->eventq);
	     pevext;
	     pevext = (struct event_ext *) ellNext(&pevext->node)){

		if (pevext->msg.m_available == mp->m_available) {
			ellDelete(&pciu->eventq, &pevext->node);
			break;
		}
	}
	FASTUNLOCK(&client->eventqLock);

	/*
	 * Not Found- return an exception event 
	 */
	if(!pevext){
		SEND_LOCK(client);
		send_err(mp, ECA_BADMONID, client, RECORD_NAME(&pciu->addr));
		SEND_UNLOCK(client);
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
	SEND_LOCK(client);
	reply = (caHdr *) ALLOC_MSG(client, 0);
	if (!reply) {
		SEND_UNLOCK(client);
		assert(0);
	}
	*reply = pevext->msg;
	reply->m_postsize = 0;

	END_MSG(client);
	SEND_UNLOCK(client);

	freeListFree (rsrvEventFreeList, pevext);
}



/*
 *
 *	read_reply()
 *
 *
 */
LOCAL void read_reply(
void			*pArg,
struct dbAddr 		*paddr,
int 			eventsRemaining,
db_field_log		*pfl
)
{
	struct event_ext *pevext = pArg;
	struct client *client = pevext->pciu->client;
        struct channel_in_use *pciu = pevext->pciu;
	caHdr *reply;
	int        status;
	int        strcnt;
	int		v41;

	if (pevext->send_lock)
		SEND_LOCK(client);

	reply = (caHdr *) ALLOC_MSG(client, pevext->size);
	if (!reply) {
		send_err(&pevext->msg, ECA_TOLARGE, client, RECORD_NAME(paddr));
		if (!eventsRemaining)
			cas_send_msg(client,!pevext->send_lock);
		if (pevext->send_lock)
			SEND_UNLOCK(client);
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
		if(reply->m_cmmd==CA_PROTO_READ){
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
			SEND_UNLOCK(client);
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
		if(!v41||reply->m_cmmd==CA_PROTO_READ){
			/*
			 * old client & plain get 
			 * continue to return an exception
			 * on failure
			 */
			send_err(&pevext->msg, ECA_GETFAIL, client, RECORD_NAME(paddr));
		}
		else{
			/*
			 * New clients recv the status of the
			 * operation directly to the
			 * event/put/get callback.
			 *
			 * Fetched value is set to zero in case they
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
	else{   /* status of db_get_field >= 0
		 *
		 * New clients recv the status of the
		 * operation directly to the
		 * event/put/get callback.
		 *
		 * The m_cid field in the protocol
		 * header is abused to carry the status
		 *
		 * get calls still use the
		 * m_cid field to identify the channel.
		 */
		if(	v41 && reply->m_cmmd!=CA_PROTO_READ){
			reply->m_cid = ECA_NORMAL;
		}
		
#ifdef CONVERSION_REQUIRED
		/*
		 * assert() is safe here because the type was
		 * checked by db_get_field()
		 */
		assert (pevext->msg.m_type < NELEMENTS(cac_dbr_cvrt));

		/* use type as index into conversion jumptable */
		(* cac_dbr_cvrt[pevext->msg.m_type])
			( reply + 1,
			  reply + 1,
			  TRUE,       /* host -> net format */
			  pevext->msg.m_count);
#endif
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
		SEND_UNLOCK(client);

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
	caHdr 	*reply;
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

	reply = (caHdr *) ALLOC_MSG(client, pevext->size);
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
		*reply = pevext->msg;
		reply->m_postsize = pevext->size;
		reply->m_cid = ECA_NORDACCESS;
		bzero((char *)(reply+1), pevext->size);
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
caHdr *mp,
struct client  *client
)
{
	FAST caHdr *reply;

	SEND_LOCK(client);
	reply = (caHdr *) ALLOC_MSG(client, 0);
	if (!reply)
		taskSuspend(0);

	*reply = *mp;

	END_MSG(client);

	SEND_UNLOCK(client);

	return;
}


/*
 *
 *	search_reply()
 *
 *
 */
LOCAL void search_reply(
caHdr *mp,
struct client  *client
)
{
	struct dbAddr  		tmp_addr;
	caHdr 			*search_reply;
	unsigned short		*pMinorVersion;
	int        		status;
	unsigned		sid;
	unsigned long		count;
	ca_uint16_t		type;
	int			spaceAvailOnFreeList;

	/* Exit quickly if channel not on this node */
	status = db_name_to_addr(
			(char *) (mp+1), 
			&tmp_addr);
	if (status < 0) {
		DLOG (2, "CAS: Lookup for channel \"%s\" failed\n", 
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

	/*
	 * stop further use of server if max block drops 
	 * below MAX_BLOCK_THRESHOLD
	 */
	spaceAvailOnFreeList = freeListItemsAvail(rsrvClientFreeList)>0 
			&& freeListItemsAvail(rsrvChanFreeList)>0
			&& freeListItemsAvail(rsrvEventFreeList)>0;
	if (casBelowMaxBlockThresh && !spaceAvailOnFreeList) { 
		SEND_LOCK(client);
		send_err(mp, 
			ECA_ALLOCMEM, 
			client, 
			"Server memory exhausted");
		SEND_UNLOCK(client);
		return;
	}

	/*
	 * starting with V4.4 the count field is used (abused)
	 * to store the minor version number of the client.
	 *
	 * New versions dont alloc the channel in response
	 * to a search request.
	 *
	 * m_count, m_cid are already in host format...
	 */
	if (CA_V44(CA_PROTOCOL_VERSION, mp->m_count)) {
		sid = ~0U;
		count = 0;
		type = ca_server_port;
	}
	else {
		struct channel_in_use 	*pchannel;

		pchannel = casCreateChannel (
				client, 
				&tmp_addr, 
				mp->m_cid);
		if (!pchannel) {
			SEND_LOCK(client);
			send_err(mp, 
				ECA_ALLOCMEM, 
				client, 
				RECORD_NAME(&tmp_addr));
			SEND_UNLOCK(client);
			return;
		}
		sid = pchannel->sid;
		count =  tmp_addr.no_elements;
		type = (ca_uint16_t) tmp_addr.dbr_field_type;
	}

	SEND_LOCK(client);

	search_reply = (caHdr *) 
		ALLOC_MSG(client, sizeof(*pMinorVersion));
	assert (search_reply);

	*search_reply = *mp;
	search_reply->m_postsize = sizeof(*pMinorVersion);

	/* this field for rmt machines where paddr invalid */
	search_reply->m_type = type;
	search_reply->m_count = count;
	search_reply->m_cid = sid;

	/*
	 * Starting with CA V4.1 the minor version number
	 * is appended to the end of each search reply.
	 * This value is ignored by earlier clients. 
	 */
	pMinorVersion = (unsigned short *)(search_reply+1);
	*pMinorVersion = htons(CA_MINOR_VERSION);

	END_MSG(client);
	SEND_UNLOCK(client);

	return;
}


/*
 * casCreateChannel ()
 */
LOCAL struct channel_in_use *casCreateChannel (
struct client  	*client,
struct dbAddr	*pAddr,
unsigned	cid
)
{
	static unsigned 	bucketID;
	unsigned		*pCID;
	struct channel_in_use 	*pchannel;
	int			status;

	/* get block off free list if possible */
	pchannel = (struct channel_in_use *) 
		freeListCalloc(rsrvChanFreeList);
	if (!pchannel) {
		return NULL;
	}
	ellInit(&pchannel->eventq);
	pchannel->ticks_at_creation = tickGet();
	pchannel->addr = *pAddr;
	pchannel->client = client;
	/*
	 * bypass read only warning
	 */
	pCID = (unsigned *) &pchannel->cid;
	*pCID = cid;

	/*
	 * allocate a server id and enter the channel pointer
	 * in the table
	 *
	 * NOTE: This detects the case where the PV id wraps
	 * around and we attempt to have two resources on the same id.
	 * The lock is applied here because on some architectures the
	 * ++ operator isnt atomic.
	 */
	FASTLOCK(&clientQlock);

	do {
		/*
		 * bypass read only warning
		 */
		pCID = (unsigned *) &pchannel->sid;
		*pCID = bucketID++;

		/*
		 * Verify that this id is not in use
		 */
		status = bucketAddItemUnsignedId (
				pCaBucket, 
				&pchannel->sid, 
				pchannel);
	} while (status != S_bucket_success);

	FASTUNLOCK(&clientQlock);

	if(status!=S_bucket_success){
		freeListFree(rsrvChanFreeList, pchannel);
		errMessage (status, "Unable to allocate server id");
		return NULL;
	}

	FASTLOCK(&client->addrqLock);
	ellAdd(&client->addrq, &pchannel->node);
	FASTUNLOCK(&client->addrqLock);

	return pchannel;
}


/* 	search_fail_reply()
 *
 *	Only when requested by the client 
 *	send search failed reply
 *
 *
 */
LOCAL void search_fail_reply(
caHdr *mp,
struct client  *client
)
{
	FAST caHdr *reply;

	SEND_LOCK(client);
	reply = (caHdr *) ALLOC_MSG(client, 0);
	if (!reply) {
		taskSuspend(0);
	}
	*reply = *mp;
	reply->m_cmmd = CA_PROTO_NOT_FOUND;
	reply->m_postsize = 0;

	END_MSG(client);
	SEND_UNLOCK(client);

}


/* 	send_err()
 *
 *	reflect error msg back to the client
 *
 *	send buffer lock must be on while in this routine
 *
 */
LOCAL void send_err(
caHdr  *curp,
int            status,
struct client  *client,
char           *pformat,
		...
)
{
	va_list			args;
        struct channel_in_use 	*pciu;
	int        		size;
	caHdr 		*reply;
	char			*pMsgString;

	va_start(args, pformat);

	/*
	 * allocate plenty of space for a sprintf() buffer
	 */
	reply = (caHdr *) ALLOC_MSG(client, 512);
	if (!reply){
		int    	logMsgArgs[6];
		size_t	i;

		for(i=0; i< NELEMENTS(logMsgArgs); i++){
			logMsgArgs[i] = va_arg(args, int);
		}

		logMsg(	"caserver: Unable to deliver err msg [%s]\n",
			(int) ca_message(status),
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
		logMsg(
			pformat,
			logMsgArgs[0],
			logMsgArgs[1],
			logMsgArgs[2],
			logMsgArgs[3],
			logMsgArgs[4],
			logMsgArgs[5]);

		return;
	}

	reply[0] = nill_msg;
	reply[0].m_cmmd = CA_PROTO_ERROR;
	reply[0].m_available = status;

	switch (curp->m_cmmd) {
	case CA_PROTO_EVENT_ADD:
	case CA_PROTO_EVENT_CANCEL:
	case CA_PROTO_READ:
	case CA_PROTO_READ_NOTIFY:
	case CA_PROTO_WRITE:
	case CA_PROTO_WRITE_NOTIFY:
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

	case CA_PROTO_SEARCH:
		reply->m_cid = curp->m_cid;
		break;

	case CA_PROTO_EVENTS_ON:
	case CA_PROTO_EVENTS_OFF:
	case CA_PROTO_READ_SYNC:
	case CA_PROTO_SNAPSHOT:
	default:
		reply->m_cid = ~0L;
		break;
	}

	/*
	 * copy back the request protocol
	 */
	reply[1] = *curp;

	/*
	 * add their context string into the protocol
	 */
	pMsgString = (char *) (reply+2);
	status = vsprintf(pMsgString, pformat, args);

	/*
	 * force string post size to be the true size rounded to even
	 * boundary
	 */
	size = strlen(pMsgString)+1;
	size += sizeof(*curp);
	reply->m_postsize = size;
	END_MSG(client);

}


/*
 * logBadIdWithFileAndLineno()
 */
LOCAL void logBadIdWithFileAndLineno(
struct client 	*client, 
caHdr 	*mp,
char		*pFileName,
unsigned	lineno
)
{
	log_header(mp,0);
	SEND_LOCK(client);
	send_err(
		mp, 
		ECA_INTERNAL, 
		client, 
		"Bad Resource ID at %s.%d",
		pFileName,
		lineno);
	SEND_UNLOCK(client);
}


/* 	log_header()
 *
 *	Debug aid - print the header part of a message.
 *
 */
LOCAL void log_header(
caHdr 	*mp,
int		mnum
)
{
        struct channel_in_use *pciu;

	pciu = MPTOPCIU(mp);

	logMsg (
"CAS header: cmmd=%d cid=0x%x type=%d count=%d postsize=%u available=0x%x\n",
	  	mp->m_cmmd,
		mp->m_cid,
	  	mp->m_type,
	  	mp->m_count,
	  	(unsigned)mp->m_postsize,
	  	(unsigned)mp->m_available);


	logMsg(	"CAS: \tN=%d paddr=%x\n",
	  	mnum, 
	  	(int)(pciu?&pciu->addr:NULL),
		NULL,
		NULL,
		NULL,
		NULL);

  	if(mp->m_cmmd==CA_PROTO_WRITE && mp->m_type==DBF_STRING)
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
        FAST caHdr 	*reply;

        reply = (caHdr *) ALLOC_MSG(pc, 0);
        if(!reply){
                taskSuspend(0);
	}

	*reply = nill_msg;
        reply->m_cmmd = CA_PROTO_NOOP;

        END_MSG(pc);

        return;
}



/*
 * MPTOPCIU()
 *
 * used to be a macro
 */
LOCAL struct channel_in_use *MPTOPCIU (caHdr *mp)
{
	struct channel_in_use 	*pciu;
	const unsigned		id = mp->m_cid;

	FASTLOCK(&clientQlock);
	pciu = bucketLookupItemUnsignedId (pCaBucket, &id);
	FASTUNLOCK(&clientQlock);

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
	struct event_ext	*pevext;

	pciu = asGetClientPvt(ascpvt);
	assert(pciu);

	pclient = pciu->client;
	assert(pclient);

	if(pclient == prsrv_cast_client){
		return;
	}


	switch(type)
	{
	case asClientCOAR:

		access_rights_reply(pciu);

		/*
		 * Update all event call backs 
		 */
		FASTLOCK(&pclient->eventqLock);
		for (pevext = (struct event_ext *) ellFirst(&pciu->eventq);
		     pevext;
		     pevext = (struct event_ext *) ellNext(&pevext->node)){
			int readAccess;

			readAccess = asCheckGet(pciu->asClientPVT);

			if(pevext->pdbev && !readAccess){
				db_post_single_event(pevext->pdbev);
				db_event_disable(pevext->pdbev);
			}
			else if(pevext->pdbev && readAccess){
				db_event_enable(pevext->pdbev);
				db_post_single_event(pevext->pdbev);
			}
		}
		FASTUNLOCK(&pclient->eventqLock);

		break;

	default:
		break;
	}
}


/*
 * access_rights_reply()
 */
LOCAL void access_rights_reply(struct channel_in_use *pciu)
{
	struct client		*pclient;
        caHdr 		*reply;
	unsigned		ar;
	int			v41;

	pclient = pciu->client;

	assert(pclient != prsrv_cast_client);

	/*
	 * noop if this is an old client
	 */
	v41 = CA_V41(CA_PROTOCOL_VERSION,pclient->minor_version_number);
	if(!v41){
		return;
	}

	ar = 0; /* none */
	if(asCheckGet(pciu->asClientPVT)){
		ar |= CA_PROTO_ACCESS_RIGHT_READ;
	}
	if(asCheckPut(pciu->asClientPVT)){
		ar |= CA_PROTO_ACCESS_RIGHT_WRITE;
	}

	SEND_LOCK(pclient);
        reply = (caHdr *)ALLOC_MSG(pclient, 0);
	assert(reply);

	*reply = nill_msg;
        reply->m_cmmd = CA_PROTO_ACCESS_RIGHTS;
	reply->m_cid = pciu->cid;
	reply->m_available = ar;
        END_MSG(pclient);
	SEND_UNLOCK(pclient);
}


/*
 * casCalloc()
 *
 * (dont drop below some max block threshold)
 */
LOCAL void *casCalloc(size_t count, size_t size)
{
        if (casBelowMaxBlockThresh) {
                return NULL;
        }
        return calloc(count, size);
}
 

/*
 * casMalloc()
 *
 * (dont drop below some max block threshold)
 */
LOCAL void *casMalloc(size_t size)
{
        if (casBelowMaxBlockThresh) {
                return NULL;
        }
        return malloc(size);
}

