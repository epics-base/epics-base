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

#if defined(VMS)
#	include		<sys/types.h>
#	include		<stsdef.h>
#elif defined(UNIX)
#	include		<sys/types.h>
#	include		<stdio.h>
#elif defined(vxWorks)
#	include		<vxWorks.h>
#	ifdef V5_vxWorks
#		include	<vxTypes.h>
#	else
#		include	<types.h>
#	endif
#else
	@@@@ dont compile @@@@
#endif

#include	<os_depen.h>
#include 	<cadef.h>
#include 	<net_convert.h>
#include	<db_access.h>
#include	<iocmsg.h>
#include 	<iocinf.h>

void 		reconnect_channel();
void		ca_request_event();
int		client_channel_exists();

#define BUFSTAT 	ca_printf("CAC: expected %d left %d\n",msgcnt,*pbufcnt);



/*
 * post_msg()
 * 
 * 
 * 
 */
int
post_msg(hdrptr, pbufcnt, pnet_addr, piiu)
	register struct extmsg 		*hdrptr;
	register long   		*pbufcnt;
	struct in_addr  		*pnet_addr;
	struct ioc_in_use 		*piiu;
{
	evid            monix;
	long            msgcnt;

	register void  *t_available;
	register unsigned short t_postsize;
	register unsigned short t_cmmd;
	register chtype t_type;
	register unsigned short t_count;
	int             status;


	post_msg_active++;


	while (*pbufcnt >= sizeof(*hdrptr)) {
#ifdef DEBUG
		ca_printf("CAC: bytes left %d, pending msgcnt %d\n",
		       *pbufcnt,
		       pndrecvcnt);
#endif

		/* byte swap the message up front */
		t_available = (void *) hdrptr->m_available;
		t_postsize = ntohs(hdrptr->m_postsize);
		t_cmmd = ntohs(hdrptr->m_cmmd);
		t_type = ntohs(hdrptr->m_type);
		t_count = ntohs(hdrptr->m_count);

#ifdef DEBUG
		ca_printf("CAC: MSG=> cmd:%d type:%d cnt:%d npost:%d avail:%x\n",
		       t_cmmd,
		       t_type,
		       t_count,
		       t_postsize,
		       t_available
			);
#endif

		msgcnt = sizeof(*hdrptr) + t_postsize;
		if (*pbufcnt < msgcnt) {
			post_msg_active--;
			return OK;
		}

		switch (t_cmmd) {

		case IOC_READ_NOTIFY:
		{
			/*
			 * run the user's event handler
			 * m_available points to event descriptor
			 */
			struct event_handler_args args;

			monix = (evid) t_available;


			/*
			 * Currently only the VAXs need data
			 * conversion
			 */
#ifdef VAX
			(*cvrt[t_type]) (
				hdrptr + 1, 
				hdrptr + 1, 
				FALSE, 
				t_count);
#endif

			/*
			 * 
			 * call handler, only if they did not clear the
			 * chid in the interim
			 */
			if (*monix->usr_func) {
				args.usr = monix->usr_arg;
				args.chid = monix->chan;
				args.type = t_type;
				args.count = t_count;
				args.dbr = (void *) (hdrptr + 1);

				LOCKEVENTS;
				(*monix->usr_func) (args);
				UNLOCKEVENTS;
			}
			LOCK;
			lstDelete(&pend_read_list, monix);
			lstAdd(&free_event_list, monix);
			UNLOCK;

			break;
		}
		case IOC_EVENT_ADD:
		{
			struct event_handler_args args;

			/*
			 * run the user's event handler m_available
			 * points to event descriptor
			 */
			monix = (evid) t_available;


			/*
			 * m_postsize = 0  is a confirmation of a
			 * monitor cancel
			 */
			if (!t_postsize) {
				LOCK;
				lstDelete(&monix->chan->eventq, monix);
				lstAdd(&free_event_list, monix);
				UNLOCK;

				break;
			}
			/* only call if not disabled */
			if (!monix->usr_func)
				break;

			/*
			 * Currently only the VAXs need data
			 * conversion
			 */
#ifdef VAX
			(*cvrt[t_type]) (
				hdrptr + 1,
				hdrptr + 1, 
				FALSE, 
				t_count);
#endif

			/*
			 * Orig version of CA didnt use this
			 * strucure. This would run faster if I had
			 * decided to pass a pointer to this
			 * structure rather than the structure itself
			 * early on.
			 * 
			 * Pumping the arguments on the stack explicitly
			 * could cause problems if a small item is in
			 * the structure.
			 * 
			 */
			args.usr = monix->usr_arg;
			args.chid = monix->chan;
			args.type = t_type;
			args.count = t_count;
			args.dbr = (void *) (hdrptr + 1);

			/* call their handler */
			LOCKEVENTS;
			(*monix->usr_func) (args);
			UNLOCKEVENTS;

			break;
		}
		case IOC_READ:
		case IOC_READ_BUILD:
		{
			chid            chan = (chid) hdrptr->m_pciu;
			unsigned	size;

			/*
			 * ignore IOC_READ_BUILDS after 
			 * connection occurs
			 */
			if(t_cmmd == IOC_READ_BUILD){
				if(chan->state == cs_conn){
					break;
				}
			}

			/*
			 * only count get returns if from the current
			 * read seq
			 */
			if (!VALID_MSG(piiu))
				break;

			size = dbr_size_n(t_type, t_count);

			/*
			 * Currently only the VAXs need data
			 * conversion
			 */
#ifdef VAX
			(*cvrt[t_type]) (
				hdrptr + 1, 
				t_available, 
				FALSE, 
				t_count);
#else
			memcpy(
				       t_available,
				       hdrptr + 1,
				       size);
#endif

			/*
			 * decrement the outstanding IO count
			 * 
			 * This relies on the IOC_READ_BUILD msg
			 * returning prior to the IOC_BUILD msg.
			 */
			if (t_cmmd != IOC_READ_BUILD ||
			    (chan->connection_func == NULL && 
					chan->state == cs_never_conn))
				CLRPENDRECV(TRUE);

			break;
		}
		case IOC_SEARCH:
		case IOC_BUILD:
		{
			chid            	chan = (chid) t_available;
			struct ioc_in_use 	*chpiiu;

			/*
			 * ignore broadcast replies for deleted channels
			 * 
			 * lock required for client_channel_exists()
			 * lock required around use of the sprintf buffer
			 */
			LOCK;
			status = client_channel_exists(chan);
			if (!status) {
				sprintf(
					sprintf_buf,
					"Chid %x Search reply from %s",
					chan,
					host_from_addr(pnet_addr));
				ca_signal(ECA_NOCHANMSG, sprintf_buf);
				break;
			}
			UNLOCK;

			chpiiu = &iiu[chan->iocix];

			if (chan->paddr) {

				if (chpiiu->sock_addr.sin_addr.s_addr == 
						pnet_addr->s_addr) {
					ca_printf("<Extra> ");
#					ifdef UNIX
						fflush(stdout);
#					endif
				} else {
					char	acc[128];
					char	rej[128];

					sprintf(acc, 
						"%s",
						chpiiu->host_name_str);
					sprintf(rej, 
						"%s", 
						host_from_addr(pnet_addr));
					LOCK;
					sprintf(
						sprintf_buf,
				"Channel: %s Accepted: %s Rejected: %s ",
						chan + 1,
						acc,
						rej);
					ca_signal(ECA_DBLCHNL, sprintf_buf);
					UNLOCK;
				}
#				ifdef IOC_READ_FOLLOWING_BUILD
				/*
				 * IOC_BUILD messages always have a
				 * IOC_READ msg following. (IOC_BUILD
				 * messages are sometimes followed by
				 * error messages which are ignored
				 * on double replies)
				 */
				if (t_cmmd == IOC_BUILD){
					msgcnt += sizeof(struct extmsg) +
					ntohs((hdrptr + 1)->m_postsize);
				}
#				endif
				break;
			}
			reconnect_channel(hdrptr, pnet_addr);


			break;
		}
		case IOC_READ_SYNC:
			piiu->read_seq++;
			break;

		case IOC_RSRV_IS_UP:
			LOCK;
			{
				struct in_addr ina;
				
				ina.s_addr = (long) t_available;
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
		{
			chid            chix = (chid) t_available;
			struct ioc_in_use *piiu = &iiu[chix->iocix];

			LOCK;
			lstDelete(&piiu->chidlist, chix);
			lstAdd(&iiu[BROADCAST_IIU].chidlist, chix);
			chix->iocix = BROADCAST_IIU;
			if (!piiu->chidlist.count)
				close_ioc(piiu);

			/*
			 * reset the delay to the next retry or keepalive
			 */
			piiu->next_retry = CA_CURRENT_TIME;
			piiu->nconn_tries = 0;

			manage_conn(TRUE);
			UNLOCK;

			break;
		}

		case IOC_CLEAR_CHANNEL:
		{
			chid            chix = (chid) t_available;
			struct ioc_in_use *piiu = &iiu[chix->iocix];
			register evid   monix;


			LOCK;
			/*
			 * remove any orphaned get callbacks for this
			 * channel
			 */
			for (monix = (evid) pend_read_list.node.next;
			     monix;
			     monix = (evid) monix->node.next)
				if (monix->chan == chix) {
					lstDelete(&pend_read_list, monix);
					lstAdd(&free_event_list, monix);
				}
			lstConcat(&free_event_list, &chix->eventq);
			lstDelete(&piiu->chidlist, chix);
			free(chix);
			if (!piiu->chidlist.count)
				close_ioc(piiu);
			UNLOCK;
			break;
		}
		case IOC_ERROR:
		{
			char            context[255];
			char           *name;
			struct extmsg  *req = hdrptr + 1;
			int             op;
			struct exception_handler_args args;


			/*
			 * dont process the message if they have
			 * disable notification
			 */
			if (!ca_static->ca_exception_func){
				break;
			}

			name = (char *) host_from_addr(pnet_addr);
			if (!name){
				name = "an unregistered IOC";
			}

			if (t_postsize > sizeof(struct extmsg)){
				sprintf(context, 
					"detected by: %s for: %s", 
					name, 
					hdrptr + 2);
			}
			else{
				sprintf(context, "detected by: %s", name);
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

			args.usr = ca_static->ca_exception_arg;
			args.chid = (chid) hdrptr->m_pciu; 
			args.type = ntohs(req->m_type);	
			args.count = ntohs(req->m_count);
			args.addr = (void *) (req->m_available);
			args.stat = ntohl((int) t_available);							args.op = op;
			args.ctx = context;

			LOCKEVENTS;
			(*ca_static->ca_exception_func) (args);
			UNLOCKEVENTS;
			break;
		}
		default:
			ca_printf("CAC: post_msg(): Corrupt cmd in msg %x\n", 
				t_cmmd);

			*pbufcnt = 0;
			post_msg_active--;
			return ERROR;
		}

		*pbufcnt -= msgcnt;
		hdrptr = (struct extmsg *) (msgcnt + (char *) hdrptr);

	}


	post_msg_active--;

	return OK;
}


/*
 *
 *	RECONNECT_CHANNEL()
 *	LOCK must be on
 *
 */
static void reconnect_channel(hdrptr,pnet_addr)
register struct extmsg		*hdrptr;
struct in_addr			*pnet_addr;
{
      	chid			chan = (chid) hdrptr->m_available;
	unsigned short		newiocix;
      	evid			pevent;
	int			status;
	enum channel_state	prev_cs;


      	LOCK;
        status = alloc_ioc	(
				pnet_addr,
				IPPROTO_TCP,		
				&newiocix
				);
	if(status != ECA_NORMAL){
	  	ca_printf("CAC: ... %s ...\n", ca_message(status));
	 	ca_printf("CAC: for %s on %s\n", chan+1, host_from_addr(pnet_addr));
	 	ca_printf("CAC: ignored search reply- proceeding\n");
	  	return;
	}

        /*	Update rmt chid fields from extmsg fields	*/
        chan->type  = ntohs(hdrptr->m_type);      
        chan->count = ntohs(hdrptr->m_count);      
        chan->paddr = hdrptr->m_pciu;

        if(chan->iocix != newiocix){
      		struct ioc_in_use	*chpiiu;

		/*
		 * The address changed (or was found for the first time)
		 */
	  	if(chan->iocix != BROADCAST_IIU)
	   		ca_signal(ECA_NEWADDR, chan+1);
		chpiiu = &iiu[chan->iocix];
          	lstDelete(&chpiiu->chidlist, chan);
          	chan->iocix = newiocix;
          	lstAdd(&iiu[newiocix].chidlist, chan);
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
	issue_claim_channel(&iiu[chan->iocix], chan);

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

}


/*
 *
 *	cas_io_done()
 *
 *
 */
void
cac_io_done(lock)
int 	lock;
{
  	register struct pending_io_event	*pioe;
	
  	if(ioeventlist.count==0)
		return;

	if(lock){
 		LOCK;
	}

    	while(pioe = (struct pending_io_event *) lstGet(&ioeventlist)){
      		(*pioe->io_done_sub)(pioe->io_done_arg);
      		free(pioe);
	}

	if(lock){
  		UNLOCK;
	}
}




/*
 *      client_channel_exists()
 *      (usually will find it in the first piiu)
 *
 *      LOCK should be on while in this routine
 *
 *      iocix field in the chid block not used here because
 *      I dont trust the chid ptr yet.
 */
static int
client_channel_exists(chan)
        chid            chan;
{
        register struct ioc_in_use      *piiu;
        register struct ioc_in_use      *pnext_iiu = &iiu[nxtiiu];
        int                             status;

        for (piiu = iiu; piiu < pnext_iiu; piiu++) {
                /*
                 * lstFind returns the node number or ERROR
                 */
                status = lstFind(&piiu->chidlist, chan);
                if (status != ERROR) {
                        return TRUE;
                }
        }
        return FALSE;
}


