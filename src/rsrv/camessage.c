/*	@(#)camessage.c
 *   @(#)camessage.c	1.2	6/27/91
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
 */

#include <vxWorks.h>
#include <taskLib.h>
#include <types.h>
#include <in.h>
#include <db_access.h>
#include <task_params.h>
#include <server.h>
#include <caerr.h>


static struct extmsg nill_msg;

#define MPTOPADDR(MP) (&((struct channel_in_use *)(MP)->m_pciu)->addr)
#define	RECORD_NAME(PADDR) (((struct db_addr *)(PADDR))->precord)

void  	search_reply();
void  	build_reply();
void	read_reply();
void	read_sync_reply();
void	event_cancel_reply();
void	clear_channel_reply();
void	send_err();
void	log_header();
void    search_fail_reply();


/*
 * CAMESSSAGE()
 *
 *
 */
camessage(client, recv)
	struct client  *client;
	struct message_buffer *recv;
{
	int            		nmsg = 0;
	unsigned		msgsize;
	unsigned		bytes_left;
	int             	status;
	FAST struct extmsg 	*mp;
	FAST struct event_ext 	*pevext;


	if (MPDEBUG == 1)
		logMsg("Parsing %d(decimal) bytes\n", recv->cnt);

	bytes_left = recv->cnt;
	while (bytes_left) {

		if(bytes_left < sizeof(*mp))
			return OK;

		mp = (struct extmsg *) &recv->buf[recv->stk];

		msgsize = mp->m_postsize + sizeof(*mp);

		if(msgsize > bytes_left) 
			return OK;

		nmsg++;

		if (MPDEBUG == 1)
			log_header(mp, nmsg);

		switch (mp->m_cmmd) {
		case IOC_NOOP:	/* verify TCP */
			break;

		case IOC_EVENT_ADD:

			FASTLOCK(&rsrv_free_eventq_lck);
			pevext = (struct event_ext *) 
				lstGet(&rsrv_free_eventq);
			FASTUNLOCK(&rsrv_free_eventq_lck);
			if (!pevext) {
				int size = db_sizeof_event_block() 
						+ sizeof(*pevext);
				pevext =
					(struct event_ext *) malloc(size);
				if (!pevext) {
					LOCK_SEND(client);
					send_err(
						mp,
						ECA_ALLOCMEM, 
						client, 
						RECORD_NAME(MPTOPADDR(mp)));
					UNLOCK_SEND(client);
					break;
				}
			}
			pevext->msg = *mp;
			pevext->mp = &pevext->msg;	/* for speed- see
							 * IOC_READ */
			pevext->client = client;
			pevext->send_lock = TRUE;
			pevext->size = (mp->m_count - 1) 
				* dbr_value_size[mp->m_type] +
				dbr_size[mp->m_type];

			lstAdd(	&((struct channel_in_use *) mp->m_pciu)->eventq, 
				pevext);

			status = db_add_event(client->evuser,
					      MPTOPADDR(mp),
					      read_reply,
					      pevext,
			   (unsigned) ((struct monops *) mp)->m_info.m_mask,
					      pevext + 1);
			if (status == ERROR) {
				LOCK_SEND(client);
				send_err(
					mp, 
					ECA_ADDFAIL, 
					client, 
					RECORD_NAME(MPTOPADDR(mp)));
				UNLOCK_SEND(client);
			}
			/*
			 * allways send it once at event add
			 * 
			 * Hold argument is supplied true so the send message
			 * buffer is not flushed once each call.
			 */
			read_reply(pevext, MPTOPADDR(mp), TRUE, NULL);

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

				pevext = &evext;
				pevext->mp = mp;
				pevext->client = client;
				pevext->send_lock = TRUE;
				if (mp->m_count == 1)
					pevext->size = dbr_size[mp->m_type];
				else
					pevext->size = (mp->m_count - 1) * 
						dbr_value_size[mp->m_type] +
						dbr_size[mp->m_type];

				/*
				 * Arguments to this routine organized in
				 * favor of the standard db event calling
				 * mechanism-  routine(userarg, paddr). See
				 * events added above.
				 * 
				 * Hold argument set true so the send message
				 * buffer is not flushed once each call.
				 */
				read_reply(pevext, MPTOPADDR(mp), TRUE, NULL);
				break;
			}
		case IOC_SEARCH:
		case IOC_BUILD:
			build_reply(mp, client);
			break;
		case IOC_WRITE:
			status = db_put_field(
					      MPTOPADDR(mp),
					      mp->m_type,
					      mp + 1,
					      mp->m_count
				);
			if (status < 0) {
				LOCK_SEND(client);
				send_err(
					mp, 
					ECA_PUTFAIL, 
					client, 
					RECORD_NAME(MPTOPADDR(mp)));
				UNLOCK_SEND(client);
			}
			break;
		case IOC_EVENTS_ON:
			{
				struct channel_in_use *pciu =
				(struct channel_in_use *) & client->addrq;

				client->eventsoff = FALSE;

				while (pciu = (struct channel_in_use *) 
					pciu->node.next) {

					pevext = (struct event_ext *) 
						& pciu->eventq;
					while (pevext = (struct event_ext *) 
						pevext->node.next){

						if (pevext->modified) {
							read_reply(
								pevext, 
								MPTOPADDR(&pevext->msg), 
								TRUE, 
								NULL);
							pevext->modified = FALSE;
						}
					}
				}
				break;
			}
		case IOC_EVENTS_OFF:
			client->eventsoff = TRUE;
			break;
		case IOC_READ_SYNC:
			read_sync_reply(mp, client);
			break;
		case IOC_CLAIM_CIU:
			/*
			 * remove channel in use block from
			 * the UDP client where it could time
			 * out and place it on the client
			 * who is claiming it
			 */

			/*
			 * clients which dont claim their 
			 * channel in use block prior to
			 * timeout must reconnect
			 */
               		status = lstFind(
					&prsrv_cast_client->addrq, 
					mp->m_pciu);
			if(status<0){
       				free_client(client);
				exit();
			}

			lstDelete(
				&prsrv_cast_client->addrq, 
				mp->m_pciu);
			lstAdd(&client->addrq, mp->m_pciu);
			break;
		default:
			log_header(mp, nmsg);
			LOCK_SEND(client);
			send_err(mp, ECA_INTERNAL, client, "Invalid Msg");
			UNLOCK_SEND(client);
			return ERROR;

		}

		recv->stk += msgsize;
		bytes_left = recv->cnt - recv->stk;
	}


	return OK;
}


/*
 *
 *	clear_channel_reply()
 *
 *
 */
static void
clear_channel_reply(mp, client)
FAST struct extmsg *mp;
struct client  *client;
{
	FAST struct extmsg *reply;
	FAST struct event_ext *pevext;
	FAST int        status;
	struct channel_in_use *pciu = (struct channel_in_use *) mp->m_pciu;
	LIST           *peventq = &pciu->eventq;

	for (pevext = (struct event_ext *) peventq->node.next;
	     pevext;
	     pevext = (struct event_ext *) pevext->node.next) {
		status = db_cancel_event(pevext + 1);
		if (status == ERROR)
			taskSuspend(0);
		lstDelete(peventq, pevext);

		FASTLOCK(&rsrv_free_eventq_lck);
		lstAdd(&rsrv_free_eventq, pevext);
		FASTUNLOCK(&rsrv_free_eventq_lck);
	}

	/*
	 * send delete confirmed message
	 */
	LOCK_SEND(client);
	reply = (struct extmsg *) ALLOC_MSG(client, 0);
	if (!reply) {
		UNLOCK_SEND(client);
		taskSuspend(0);
	}
	*reply = *mp;

	END_MSG(client);
	UNLOCK_SEND(client);

	lstDelete(&client->addrq, pciu);
	FASTLOCK(&rsrv_free_addrq_lck);
	lstAdd(&rsrv_free_addrq, pciu);
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
static void
event_cancel_reply(mp, client)
	FAST struct extmsg *mp;
	struct client  *client;
{
	FAST struct extmsg *reply;
	FAST struct event_ext *pevext;
	FAST int        status;
	LIST           *peventq =
	&((struct channel_in_use *) mp->m_pciu)->eventq;


	for (pevext = (struct event_ext *) peventq->node.next;
	     pevext;
	     pevext = (struct event_ext *) pevext->node.next)
		if (pevext->msg.m_available == mp->m_available) {
			status = db_cancel_event(pevext + 1);
			if (status == ERROR)
				taskSuspend(0);
			lstDelete(peventq, pevext);

			/*
			 * send delete confirmed message
			 */
			LOCK_SEND(client);
			reply = (struct extmsg *) ALLOC_MSG(client, 0);
			if (!reply) {
				UNLOCK_SEND(client);
				taskSuspend(0);
			}
			*reply = pevext->msg;
			reply->m_postsize = 0;

			END_MSG(client);
			UNLOCK_SEND(client);

			FASTLOCK(&rsrv_free_eventq_lck);
			lstAdd(&rsrv_free_eventq, pevext);
			FASTUNLOCK(&rsrv_free_eventq_lck);

			return;
		}
	/*
	 * Not Found- return an error message
	 */
	LOCK_SEND(client);
	send_err(mp, ECA_BADMONID, client, NULL);
	UNLOCK_SEND(client);

	return;
}



/*
 *
 *	read_reply()
 *
 *
 */
static void
read_reply(pevext, paddr, hold, pfl)
FAST struct event_ext *pevext;
FAST struct db_addr *paddr;
int             hold;	/* more on the way if true */
void		*pfl;
{
	FAST struct extmsg *mp = pevext->mp;
	FAST struct client *client = pevext->client;
	FAST struct extmsg *reply;
	FAST int        status;
	FAST int        strcnt;

	/*
	 * If flow control is on set modified and send for later
	 */
	if (client->eventsoff) {
		pevext->modified = TRUE;
		return;
	}
	if (pevext->send_lock)
		LOCK_SEND(client);

	reply = (struct extmsg *) ALLOC_MSG(client, pevext->size);
	if (!reply) {
		send_err(mp, ECA_TOLARGE, client, RECORD_NAME(paddr));
		if (pevext->send_lock)
			UNLOCK_SEND(client);
		return;
	}
	*reply = *mp;
	reply->m_postsize = pevext->size;
	reply->m_pciu = (void *) ((struct channel_in_use *) mp->m_pciu)->chid;
	status = db_get_field(
			      paddr,
			      mp->m_type,
			      reply + 1,
			      mp->m_count,
			      pfl);
	if (status < 0) {
		send_err(mp, ECA_GETFAIL, client, RECORD_NAME(paddr));
		log_header(mp, 0);
	}
	else{
		/*
		 * force string message size to be the true size rounded to even
		 * boundary
		 */
		if (mp->m_type == DBR_STRING && mp->m_count == 1) {
			/* add 1 so that the string terminator will be shipped */
			strcnt = strlen(reply + 1) + 1;
			reply->m_postsize = strcnt;
		}
		END_MSG(client);
	
		/*
		 * Ensures timely response for events, but does que 
		 * them up like db requests when the OPI does not keep up.
		 */
		if (!hold)
			cas_send_msg(client,FALSE);
	}

	if (pevext->send_lock)
		UNLOCK_SEND(client);

	return;
}


/*
 *
 *	read_sync_reply()
 *
 *
 */
static void
read_sync_reply(mp, client)
	FAST struct extmsg *mp;
	struct client  *client;
{
	FAST struct extmsg *reply;

	LOCK_SEND(client);
	reply = (struct extmsg *) ALLOC_MSG(client, 0);
	if (!reply)
		taskSuspend(0);

	*reply = *mp;

	END_MSG(client);

	UNLOCK_SEND(client);

	return;
}


/*
 *
 *	build_reply()
 *
 *
 */
static void
build_reply(mp, client)
	FAST struct extmsg *mp;
	struct client  *client;
{
	LIST           			*addrq = &client->addrq;
	FAST struct extmsg 		*search_reply;
	FAST struct extmsg 		*get_reply;
	FAST int        		status;
	struct db_addr  		tmp_addr;
	FAST struct channel_in_use 	*pchannel;



	/* Exit quickly if channel not on this node */
	status = db_name_to_addr(
			mp->m_cmmd == IOC_BUILD ? mp + 2 : mp + 1, 
			&tmp_addr);
	if (status < 0) {
		if (MPDEBUG == 1)
			logMsg(	"Lookup for channel \"%s\" failed\n", 
				mp + 1);
		if (mp->m_type == DOREPLY)
			search_fail_reply(mp, client);
		return;
	}

	/* get block off free list if possible */
	FASTLOCK(&rsrv_free_addrq_lck);
	pchannel = (struct channel_in_use *) lstGet(&rsrv_free_addrq);
	FASTUNLOCK(&rsrv_free_addrq_lck);
	if (!pchannel) {
		pchannel = (struct channel_in_use *) calloc(1, sizeof(*pchannel));
		if (!pchannel) {
			LOCK_SEND(client);
			send_err(mp, ECA_ALLOCMEM, client, RECORD_NAME(&tmp_addr));
			UNLOCK_SEND(client);
			return;
		}
	}
	pchannel->ticks_at_creation = tickGet();
	pchannel->addr = tmp_addr;
	pchannel->chid = (void *) mp->m_pciu;
	/* store the addr block in a Q so it can be deallocated */
	lstAdd(addrq, pchannel);


	/*
	 * ALLOC_MSG allways allocs at least the sizeof extmsg Large
	 * requested size insures both messages sent in one reply NOTE: my
	 * UDP reliability schemes rely on both msgs in same reply Therefore
	 * the send buffer locked while both messages are placed
	 */
	LOCK_SEND(client);
	if (mp->m_cmmd == IOC_BUILD) {
		FAST short      type = (mp + 1)->m_type;
		FAST unsigned int count = (mp + 1)->m_count;
		FAST unsigned int size;

		size = (count - 1) * dbr_value_size[type] + dbr_size[type];

		get_reply = (struct extmsg *) ALLOC_MSG(client, size + sizeof(*mp));
		if (!get_reply) {
			/* tell them that their request is to large */
			send_err(mp, ECA_TOLARGE, client, RECORD_NAME(&tmp_addr));
		} else {
			struct event_ext evext;

			evext.mp = mp + 1;
			/* pchannel ukn prior to connect */
			evext.mp->m_pciu = pchannel;
			evext.client = client;
			evext.send_lock = FALSE;
			evext.size = size;

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
	search_reply = (struct extmsg *) ALLOC_MSG(client, 0);
	if (!search_reply)
		taskSuspend();

	*search_reply = *mp;
	search_reply->m_postsize = 0;

	/* this field for rmt machines where paddr invalid */
	search_reply->m_type = tmp_addr.field_type;
	search_reply->m_count = tmp_addr.no_elements;
	search_reply->m_pciu = (void *) pchannel;

	END_MSG(client);
	UNLOCK_SEND(client);

	return;
}


/* 	search_fail_reply()
 *
 *	Only when requested by the client 
 *	send search failed reply
 *
 *
 */
static void
search_fail_reply(mp, client)
	FAST struct extmsg *mp;
	struct client  *client;
{
	FAST struct extmsg *reply;

	LOCK_SEND(client);
	reply = (struct extmsg *) ALLOC_MSG(client, 0);
	if (!reply) {
		taskSuspend(0);
	}
	*reply = *mp;
	reply->m_cmmd = IOC_NOT_FOUND;
	reply->m_postsize = 0;

	END_MSG(client);
	UNLOCK_SEND(client);

}


/* 	send_err()
 *
 *	reflect error msg back to the client
 *
 *	send buffer lock must be on while in this routine
 *
 */
static void
send_err(curp, status, client, footnote)
	struct extmsg  *curp;
	int             status;
	struct client  *client;
	char           *footnote;
{
	FAST struct extmsg *reply;
	FAST int        size;

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

	*reply = nill_msg;
	reply->m_cmmd = IOC_ERROR;
	reply->m_available = status;
	reply->m_postsize = size;
	switch (curp->m_cmmd) {
	case IOC_EVENT_ADD:
	case IOC_EVENT_CANCEL:
	case IOC_READ:
	case IOC_READ_NOTIFY:
	case IOC_SEARCH:
	case IOC_BUILD:
	case IOC_WRITE:
		reply->m_pciu = (void *)
			 ((struct channel_in_use *) curp->m_pciu)->chid;
		break;
	case IOC_EVENTS_ON:
	case IOC_EVENTS_OFF:
	case IOC_READ_SYNC:
	case IOC_SNAPSHOT:
	default:
		reply->m_pciu = (void *) NULL;
		break;
	}
	*(reply + 1) = *curp;
	strcpy(reply + 2, footnote);

	END_MSG(client);

}


/* 	log_header()
 *
 *	Debug aid - print the header part of a message.
 *
 */
static void
log_header (mp, mnum)
FAST struct extmsg *mp;
{
	logMsg(	"N=%d cmd=%d type=%d pstsize=%d paddr=%x avail=%x\n",
	  	mnum, 
	  	mp->m_cmmd,
	  	mp->m_type,
	  	mp->m_postsize,
	  	MPTOPADDR(mp),
	  	mp->m_available);
  	if(mp->m_cmmd==IOC_WRITE && mp->m_type==DBF_STRING)
    		logMsg("The string written: %s \n",mp+1);
}
