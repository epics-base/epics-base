/************************************************************************/
/*									*/
/*	        	      L O S  A L A M O S			*/
/*		        Los Alamos National Laboratory			*/
/*		         Los Alamos, New Mexico 87545			*/
/*									*/
/*	Copyright, 1986, The Regents of the University of California.	*/
/*									*/
/*									*/
/*	History								*/
/*	-------								*/
/*									*/
/*	Date	Person	Comments					*/
/*	----	------	--------					*/
/*	8/87	joh	Init Release					*/
/*	1/90	joh	Made cmmd for errors which			*/
/*					eliminated the status field	*/
/*	030791	joh	Fixed problem where pend count was decremented	*/
/*			prior to connecting chan (setting the type).	*/
/*	041591	joh	added call to channel exsits routine		*/
/*	060491  joh	fixed structure pass for SPARC cc		*/
/*	060691	joh	reworked to remove 4 byte count before each	*/
/*			macro message					*/
/*	071291	joh	now claiming channel in use block over TCP so	*/
/*			problems with duplicate port assigned to	*/
/*			client after reboot go away			*/
/*	072391	joh	added event locking for vxWorks			*/
/*    	100391  joh     added missing ntohs() for the VAX               */
/*	111991	joh	converted MACRO in iocinf.h to cac_io_done()	*/
/*			here						*/
/*	022692	joh	moved modify of chan->state inside lock to	*/
/*			provide sync necissary for new use of this 	*/
/*			field.						*/
/*	031692	joh	When bad cmd in message detected disconnect	*/
/*			instead of terminating the client		*/
/*	041692	joh	set state to cs_conn before caling 		*/
/* 			ca_request_event() so their channel 		*/
/*			connect tests wont fail				*/
/*	042892	joh	No longer checking the status from free() 	*/
/*			since it varies from os to os			*/
/*	040592	joh	took out extra cac_send_msg() calls		*/
/*	072792	joh	better messages					*/
/*	110592	joh	removed ptr dereference from test for valid	*/
/*			monitor handler function			*/
/*	111792	joh	reset retry delay for cast iiu when host	*/
/*			indicates the channel's address has changed	*/
/*	120992	joh	converted to dll list routines			*/
/*	122192	joh	now decrements the outstanding ack count	*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	channel access service routines				*/
/*	File:	atcs:[ca]service.c					*/
/*	Environment: VMS, UNIX, VRTX					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	channel access service routines					*/
/*									*/
/*									*/
/*	Special comments						*/
/*	------- --------						*/
/*									*/
/************************************************************************/
/*_end									*/

static char *sccsId = "$Id$"; 

#if defined(VMS)
#	include		<sys/types.h>
#	include		<stsdef.h>
#else
#  if defined(UNIX)
#	include		<sys/types.h>
#	include		<stdio.h>
#  else
#    if defined(vxWorks)
#	include		<string.h>
#	include		<stdio.h>
#	include		<vxWorks.h>
#    else
	@@@@ dont compile @@@@
#    endif
#  endif
#endif

#include 	<cadef.h>
#include	<os_depen.h>
#include	<iocmsg.h>
#include 	<iocinf.h>
#include	<db_access.h>
#include 	<net_convert.h>

#ifdef __STDC__

LOCAL void reconnect_channel(
IIU			*piiu,
struct in_addr          *pnet_addr
);

LOCAL int cacMsg(
struct ioc_in_use 	*piiu,
struct in_addr  	*pnet_addr
);

#else

LOCAL void 	reconnect_channel();
LOCAL int 	cacMsg();

#endif



/*
 * post_msg()
 * 
 * 
 * LOCK should be applied when calling this routine
 * 
 */
#ifdef __STDC__
int post_msg(
struct ioc_in_use 	*piiu,
struct in_addr  	*pnet_addr,
char			*pInBuf,
unsigned long		blockSize
)
#else
int post_msg(piiu, pnet_addr)
struct ioc_in_use 	*piiu;
struct in_addr  	*pnet_addr;
char			*pInBuf;
unsigned long		blockSize;
#endif
{
	int		status;
	unsigned long 	size;

	while (blockSize) {

		/*
		 * fetch a complete message header
		 */
		if(piiu->curMsgBytes < sizeof(piiu->curMsg)){
			char  *pHdr;

			size = sizeof(piiu->curMsg) - piiu->curMsgBytes;
			size = min(size, blockSize);
			
			pHdr = (char *) &piiu->curMsg;
			memcpy(	pHdr + piiu->curMsgBytes, 
				pInBuf, 
				size);
			
			piiu->curMsgBytes += size;
			if(piiu->curMsgBytes < sizeof(piiu->curMsg)){
				return OK;
			}

			pInBuf += size;
			blockSize -= size;
		
			/* 
			 * fix endian of bytes 
			 */
			piiu->curMsg.m_postsize = 
				ntohs(piiu->curMsg.m_postsize);
			piiu->curMsg.m_cmmd = ntohs(piiu->curMsg.m_cmmd);
			piiu->curMsg.m_type = ntohs(piiu->curMsg.m_type);
			piiu->curMsg.m_count = ntohs(piiu->curMsg.m_count);
#if 0
printf("%s Cmd=%3d Type=%3d Count=%4d Size=%4d Avail=%8x Cid=%6d\n",
	piiu->host_name_str,
	piiu->curMsg.m_cmmd,
	piiu->curMsg.m_type,
	piiu->curMsg.m_count,
	piiu->curMsg.m_postsize,
	piiu->curMsg.m_available,
	piiu->curMsg.m_cid);
#endif
		}


		/*
		 * dont allow huge msg body until
		 * the server supports it
		 */
		if(piiu->curMsg.m_postsize>(unsigned)MAX_TCP){
			piiu->curMsgBytes = 0;
			piiu->curDataBytes = 0;
			return ERROR;
		}

		/*
		 * make sure we have a large enough message body cache
		 */
		if(piiu->curMsg.m_postsize>piiu->curDataMax){
			if(piiu->pCurData){
				free(piiu->pCurData);
			}
			piiu->curDataMax = 0;
			piiu->pCurData = (void *) 
				malloc(piiu->curMsg.m_postsize);
			if(!piiu->pCurData){
				piiu->curMsgBytes = 0;
				piiu->curDataBytes = 0;
				return ERROR;
			}
			piiu->curDataMax = 
				piiu->curMsg.m_postsize;
		}

		/*
 		 * Fetch a complete message body
		 */
		if(piiu->curMsg.m_postsize>piiu->curDataBytes){
			char *pBdy;

			size = piiu->curMsg.m_postsize - piiu->curDataBytes; 
			size = min(size, blockSize);
			pBdy = piiu->pCurData;
			memcpy(	pBdy+piiu->curDataBytes, 
				pInBuf, 
				size);
			piiu->curDataBytes += size;
			if(piiu->curDataBytes < piiu->curMsg.m_postsize){
				return OK;
			}
			pInBuf += size;
			blockSize -= size;
		}	


		/*
 		 * execute the response message
		 */
		status = cacMsg(piiu, pnet_addr);
		piiu->curMsgBytes = 0;
		piiu->curDataBytes = 0;
		if(status != OK){
			return ERROR;
		}
	}
	return OK;
}

#ifdef __STDC__
LOCAL int cacMsg(
struct ioc_in_use 	*piiu,
struct in_addr  	*pnet_addr
)
#else
LOCAL int cacMsg(piiu, pnet_addr)
struct ioc_in_use 	*piiu;
struct in_addr  	*pnet_addr;
#endif
{
	evid            monix;
	int             status;

	switch (piiu->curMsg.m_cmmd) {

	case IOC_NOOP:
		break;

	case IOC_WRITE_NOTIFY:
	{
		/*
		 * run the user's event handler
		 * m_available points to event descriptor
		 */
		struct event_handler_args args;

		monix = (evid) piiu->curMsg.m_available;

		/*
		 * 
		 * call handler, only if they did not clear the
		 * chid in the interim
		 */
		if (monix->usr_func) {
			args.usr = monix->usr_arg;
			args.chid = monix->chan;
			args.type = monix->type;
			args.count = monix->count;
			args.dbr = NULL;
			/*
			 * the channel id field is abused for
			 * write notify status
			 *
			 * write notify was added vo CA V4.1
			 */
			args.status = ntohl(piiu->curMsg.m_cid); 

			LOCKEVENTS;
			(*monix->usr_func) (args);
			UNLOCKEVENTS;
		}
		LOCK;
		ellDelete(&pend_write_list, &monix->node);
		ellAdd(&free_event_list, &monix->node);
		UNLOCK;

		break;

	}	
	case IOC_READ_NOTIFY:
	{
		/*
		 * run the user's event handler
		 * m_available points to event descriptor
		 */
		struct event_handler_args args;

		monix = (evid) piiu->curMsg.m_available;

		/*
		 * 
		 * call handler, only if they did not clear the
	 	 * chid in the interim
		 */
		if (monix->usr_func) {
			int v41;

			/*
			 * convert the data buffer from net
			 * format to host format
			 *
			 * Currently only the VAXs need data
			 * conversion
			 */
#			ifdef VAX
				(*cvrt[piiu->curMsg.m_type])(
					piiu->pCurData, 
					piiu->pCurData, 
					FALSE,
					piiu->curMsg.m_count);
#			endif

			args.usr = monix->usr_arg;
			args.chid = monix->chan;
			args.type = piiu->curMsg.m_type;
			args.count = piiu->curMsg.m_count;
			args.dbr = piiu->pCurData;
			/*
			 * the channel id field is abused for
			 * read notify status starting
			 * with CA V4.1
			 */
			v41 = CA_V41(
				CA_PROTOCOL_VERSION, 
				piiu->minor_version_number);
			if(v41){
				args.status = ntohl(piiu->curMsg.m_cid);
			}
			else{
				args.status = ECA_NORMAL;
			}

			LOCKEVENTS;
			(*monix->usr_func) (args);
			UNLOCKEVENTS;
		}
		LOCK;
		ellDelete(&pend_read_list, &monix->node);
		ellAdd(&free_event_list, &monix->node);
		UNLOCK;

		break;
	}
	case IOC_EVENT_ADD:
	{
		int v41;
		struct event_handler_args args;

		/*
		 * run the user's event handler m_available
		 * points to event descriptor
		 */
		monix = (evid) piiu->curMsg.m_available;


		/*
		 * m_postsize = 0  is a confirmation of a
		 * monitor cancel
		 */
		if (!piiu->curMsg.m_postsize) {
			LOCK;
			ellDelete(&monix->chan->eventq, &monix->node);
			ellAdd(&free_event_list, &monix->node);
			UNLOCK;

			break;
		}
		/* only call if not disabled */
		if (!monix->usr_func)
			break;

		/*
		 * convert the data buffer from net
		 * format to host format
		 *
		 * Currently only the VAXs need data
		 * conversion
		 */
#		ifdef VAX
			(*cvrt[piiu->curMsg.m_type])(
				piiu->pCurData, 
				piiu->pCurData, 
				FALSE,
				piiu->curMsg.m_count);
#		endif

		/*
		 * Orig version of CA didnt use this
		 * strucure. This would run faster if I had
		 * decided to pass a pointer to this
		 * structure rather than the structure itself
		 * early on.
		 */
		args.usr = monix->usr_arg;
		args.chid = monix->chan;
		args.type = piiu->curMsg.m_type;
		args.count = piiu->curMsg.m_count;
		args.dbr = piiu->pCurData;
		/*
		 * the channel id field is abused for
		 * event status starting
		 * with CA V4.1
		 */
		v41 = CA_V41(
			CA_PROTOCOL_VERSION, 
			piiu->minor_version_number);
		if(v41){
			args.status = ntohl(piiu->curMsg.m_cid); 
		}
		else{
			args.status = ECA_NORMAL;
		}

		/* call their handler */
		LOCKEVENTS;
		(*monix->usr_func) (args);
		UNLOCKEVENTS;

		break;
	}
	case IOC_READ:
	case IOC_READ_BUILD:
	{
		chid            chan;

		/*
		 * verify the channel id
		 */
		LOCK;
		chan = bucketLookupItem(pBucket, piiu->curMsg.m_cid);
		UNLOCK;
		if(!chan){
			if(piiu->curMsg.m_cmmd != IOC_READ_BUILD){
				ca_signal(ECA_INTERNAL, 
					"bad client channel id from server");
			}
			break;
		}

		/*
		 * ignore IOC_READ_BUILDS after 
		 * connection occurs
		 */
		if(piiu->curMsg.m_cmmd == IOC_READ_BUILD){
			if(chan->state == cs_conn || 
				chan->state == cs_closed){
				break;
			}
		}

		/*
		 * only count get returns if from the current
		 * read seq
		 */
		if (!VALID_MSG(piiu))
			break;

		/*
		 * convert the data buffer from net
		 * format to host format
		 *
		 * Currently only the VAXs need data
		 * conversion
		 */
#		ifdef VAX
			(*cvrt[piiu->curMsg.m_type])(
				piiu->pCurData, 
				piiu->pCurData, 
				FALSE,
				piiu->curMsg.m_count);
#		else
			memcpy(
				(char *)piiu->curMsg.m_available,
				piiu->pCurData,
				dbr_size_n(piiu->curMsg.m_type, piiu->curMsg.m_count));
#		endif

		/*
		 * decrement the outstanding IO count
		 * 
		 * This relies on the IOC_READ_BUILD msg
		 * returning prior to the IOC_BUILD msg.
		 */
		if (piiu->curMsg.m_cmmd == IOC_READ){
			CLRPENDRECV(TRUE);
		}
		else if(chan->connection_func == NULL && 
				chan->state == cs_never_conn){
			CLRPENDRECV(TRUE);
		}

		break;
	}
	case IOC_SEARCH:
	case IOC_BUILD:
		reconnect_channel(piiu, pnet_addr);
		break;

	case IOC_READ_SYNC:
		piiu->read_seq++;
		break;

	case IOC_RSRV_IS_UP:
		LOCK;
		{
			struct in_addr ina;
			
			ina.s_addr = (long) piiu->curMsg.m_available;
			mark_server_available(&ina);
		}
		UNLOCK;
		break;

	case REPEATER_CONFIRM:
		ca_static->ca_repeater_contacted = TRUE;
#ifdef DEBUG
		ca_printf("CAC: repeater confirmation recv\n");
#endif
		break;

	case IOC_NOT_FOUND:
		break;

	case IOC_CLEAR_CHANNEL:
	{
		chid            	chix = (chid) piiu->curMsg.m_available;
		struct ioc_in_use 	*piiu = chix->piiu;
		register evid   	monix;


		LOCK;
		/*
		 * remove any orphaned get callbacks for this
		 * channel
		 */
		for (monix = (evid) pend_read_list.node.next;
		     monix;
		     monix = (evid) monix->node.next){
			if (monix->chan == chix) {
				ellDelete(	
					&pend_read_list, 
					&monix->node);
				ellAdd(
					&free_event_list, 
					&monix->node);
			}
		}
		ellConcat(&free_event_list, &chix->eventq);
		ellDelete(&piiu->chidlist, &chix->node);
		status = bucketRemoveItem(pBucket, chix->cid, chix);
		if(status != BUCKET_SUCCESS){
			ca_signal(
				ECA_INTERNAL,
				"bad id at channel delete");
		}
		free(chix);
		if (!piiu->chidlist.count){
			piiu->conn_up = FALSE;
		}
		UNLOCK;
		break;
	}
	case IOC_ERROR:
	{
		char		nameBuf[64];
		char           	context[255];
		struct extmsg  	*req = piiu->pCurData;
		int             op;
		struct exception_handler_args args;


		/*
		 * dont process the message if they have
		 * disable notification
		 */
		if (!ca_static->ca_exception_func){
			break;
		}

		host_from_addr(pnet_addr, nameBuf, sizeof(nameBuf));

		if (piiu->curMsg.m_postsize > sizeof(struct extmsg)){
			sprintf(context, 
				"detected by: %s for: %s", 
				nameBuf, 
				(char *)(req+1));
		}
		else{
			sprintf(context, "detected by: %s", nameBuf);
		}

		/*
		 * Map internal op id to external op id so I
		 * can freely change the protocol in the
		 * future. This is quite wasteful of space
		 * however.
		 */
		switch (ntohs(req->m_cmmd)) {
		case IOC_READ_NOTIFY:
		case IOC_READ:
			op = CA_OP_GET;
			break;
		case IOC_WRITE_NOTIFY:
		case IOC_WRITE:
			op = CA_OP_PUT;
			break;
		case IOC_SEARCH:
		case IOC_BUILD:
			op = CA_OP_SEARCH;
			break;
		case IOC_EVENT_ADD:
			op = CA_OP_ADD_EVENT;
			break;
		case IOC_EVENT_CANCEL:
			op = CA_OP_CLEAR_EVENT;
			break;
		default:
			op = CA_OP_OTHER;
			break;
		}

		LOCK;
		args.chid = bucketLookupItem(pBucket, piiu->curMsg.m_cid);
		UNLOCK;
		args.usr = ca_static->ca_exception_arg;
		args.type = ntohs(req->m_type);	
		args.count = ntohs(req->m_count);
		args.addr = (void *) (req->m_available);
		args.stat = ntohl((long) piiu->curMsg.m_available);							args.op = op;
		args.ctx = context;

		LOCKEVENTS;
		(*ca_static->ca_exception_func) (args);
		UNLOCKEVENTS;
		break;
	}
	default:
		ca_printf("CAC: post_msg(): Corrupt cmd in msg %x\n", 
			piiu->curMsg.m_cmmd);

		return ERROR;
	}

	return OK;
}


/*
 *
 *	reconnect_channel()
 *
 */
#ifdef __STDC__
LOCAL void reconnect_channel(
IIU			*piiu,
struct in_addr          *pnet_addr
)
#else
LOCAL void reconnect_channel(hdrptr,pnet_addr)
IIU			*piiu;
struct in_addr		*pnet_addr;
#endif
{
	char			rej[64];
      	chid			chan;
      	evid			pevent;
	int			status;
	enum channel_state	prev_cs;
	IIU			*allocpiiu;
	IIU			*chpiiu;
	unsigned short		*pMinorVersion;

	/*
	 * ignore broadcast replies for deleted channels
	 * 
	 * lock required around use of the sprintf buffer
	 */
	LOCK;
	chan = bucketLookupItem(
			pBucket, 
			piiu->curMsg.m_available);
	if(!chan){
		UNLOCK;
		return;
	}

	chpiiu = chan->piiu;

	if(!chpiiu){
		ca_printf("cast reply to local channel??\n");
		UNLOCK;
		return;
	}

	/*
	 * Ignore search replies to closing channels 
	 */ 
	if(chan->state == cs_closed) {
		UNLOCK;
		return;
	}

	/*
	 * Ignore duplicate search replies
	 */
	if (chan->state == cs_conn) {

		if (chpiiu->sock_addr.sin_addr.s_addr == 
				pnet_addr->s_addr) {
			ca_printf("<Extra> ");
#		ifdef UNIX
			fflush(stdout);
#		endif
		} else {

			host_from_addr(pnet_addr,rej,sizeof(rej));
			sprintf(
				sprintf_buf,
		"Channel: %s Accepted: %s Rejected: %s ",
				(char *)(chan + 1),
				chpiiu->host_name_str,
				rej);
			ca_signal(ECA_DBLCHNL, sprintf_buf);
		}
		UNLOCK;
		return;
	}

        status = alloc_ioc	(
				pnet_addr,
				IPPROTO_TCP,		
				&allocpiiu);
	if(status != ECA_NORMAL){
		host_from_addr(pnet_addr,rej,sizeof(rej));
	  	ca_printf("CAC: ... %s ...\n", ca_message(status));
	 	ca_printf("CAC: for %s on %s\n", chan+1, rej);
	 	ca_printf("CAC: ignored search reply- proceeding\n");
		UNLOCK;
	  	return;
	}

        /*	Update rmt chid fields from extmsg fields	*/
        chan->type  = piiu->curMsg.m_type;      
        chan->count = piiu->curMsg.m_count;      
        chan->id.sid = piiu->curMsg.m_cid;

	/*
	 * Starting with CA V4.1 the minor version number
	 * is appended to the end of each search reply.
	 * This value is ignored by earlier clients.
	 */
	if(piiu->curMsg.m_postsize >= sizeof(*pMinorVersion)){
		pMinorVersion = (unsigned short *)(piiu->pCurData);
        	allocpiiu->minor_version_number = ntohs(*pMinorVersion);      
	}
	else{
		allocpiiu->minor_version_number = CA_UKN_MINOR_VERSION;
	}

        if(chpiiu != allocpiiu){

		/*
		 * The address changed (or was found for the first time)
		 */
	  	if(chpiiu != piiuCast)
	   		ca_signal(ECA_NEWADDR, (char *)(chan+1));
          	ellDelete(&chpiiu->chidlist, &chan->node);
          	chan->piiu = chpiiu = allocpiiu; 
          	ellAdd(&chpiiu->chidlist, &chan->node);

		/*
		 * If this is the first channel to be
		 * added to this IIU then communicate
		 * the client's name to the server. 
		 * (CA V4.1 or higher)
		 */
		if(ellCount(&chpiiu->chidlist)==1){
			issue_identify_client(chpiiu);
			issue_identify_client_location(chpiiu);
		}
        }

	/*
	 * set state to cs_conn before caling 
	 * ca_request_event() so their channel 
	 * connect tests wont fail
 	 */
	prev_cs = chan->state;
	chan->state = cs_conn;

	/*
	 * claim the resource in the IOC
	 * over TCP so problems with duplicate UDP port
	 * after reboot go away
	 */
	issue_claim_channel(chpiiu, chan);

	/*
	 * NOTE: monitor and callback reissue must occur prior to calling
	 * their connection routine otherwise they could be requested twice.
	 */
#ifdef CALLBACK_REISSUE
        /* reissue any outstanding get callbacks for this channel */
	if(pend_read_list.count){
          	for(	pevent = (evid) pend_read_list.node.next; 
			pevent; 
			pevent = (evid) pevent->node.next){
            		if(pevent->chan == chan){
	      			issue_get_callback(pevent);
	    		}
		}
        }
#endif

        /* reissue any events (monitors) for this channel */
	if(chan->eventq.count){
          	for( 	pevent = (evid)chan->eventq.node.next; 
			pevent; 
			pevent = (evid)pevent->node.next)
	    		ca_request_event(pevent);
 	}

      	UNLOCK;

      	if(chan->connection_func){
		struct connection_handler_args	args;

		args.chid = chan;
		args.op = CA_OP_CONN_UP;
		LOCKEVENTS;
        	(*chan->connection_func)(args);
		UNLOCKEVENTS;
	}
	else if(prev_cs==cs_never_conn){
          	/* decrement the outstanding IO count */
          	CLRPENDRECV(TRUE);
	}

	UNLOCK;
}


/*
 *
 *	cas_io_done()
 *
 *
 */
#ifdef __STDC__
void cac_io_done(int lock)
#else
void cac_io_done(lock)
int 	lock;
#endif
{
  	struct pending_io_event	*pioe;
	
  	if(ioeventlist.count==0)
		return;

	if(lock){
 		LOCK;
	}

    	while(pioe = (struct pending_io_event *) ellGet(&ioeventlist)){
      		(*pioe->io_done_sub)(pioe->io_done_arg);
      		free(pioe);
	}

	if(lock){
  		UNLOCK;
	}
}
