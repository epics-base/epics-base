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
/*	Date		Programmer	Comments			*/
/*	----		----------	--------			*/
/*	8/87		Jeff Hill	Init Release			*/
/*	1/90		Jeff Hill	Made cmmd for errors which	*/
/*					eliminated the status field	*/
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

#ifdef VMS
#include		<stsdef.h>
#endif

#ifdef UNIX
#include		<stdio.h>
#endif

#include		<vxWorks.h>

#include		<types.h>
#include 		<cadef.h>
#include 		<net_convert.h>
#include		<db_access.h>
#include		<iocmsg.h>
#include 		<iocinf.h>

void reconnect_channel();

void post_msg(hdrptr,bufcnt,net_addr,piiu)
register struct extmsg			*hdrptr;
register long				bufcnt;
struct in_addr				net_addr;
struct ioc_in_use			*piiu;
{
  evid					monix;
  long					msgcnt;
  long					tmp;

  register void *			t_available;
  register unsigned short		t_postsize;
  register unsigned short		t_cmmd;
  register chtype 			t_type;
  register unsigned short 		t_count;
  register unsigned short 		t_size;
  int					status;


# define BUFSTAT\
  printf("expected %d left %d\n",msgcnt,bufcnt);

  post_msg_active++;


  while(bufcnt>0){
#   ifdef DEBUG
      printf(	"processing message- bytes left %d, pending msgcnt %d\n",
		bufcnt,
		pndrecvcnt);
#   endif

    /* byte swap the message up front */
    t_available = (void *) hdrptr->m_available;
    t_postsize 	= ntohs(hdrptr->m_postsize);
    t_cmmd	= ntohs(hdrptr->m_cmmd);
    t_type	= ntohs(hdrptr->m_type);
    t_count 	= ntohs(hdrptr->m_count);
    t_size	= dbr_size[t_type];

#   ifdef DEBUG
      printf(	"MSG: cmd:%d type:%d cnt:%d size:%d npost:%d avail:%x\n",
		t_cmmd,
		t_type,
		t_count,
		t_size,
		t_postsize,
		t_available
		);
#   endif

    msgcnt = sizeof(*hdrptr) + t_postsize; 
    if(bufcnt-msgcnt < 0){
      printf(
	"post_msg(): expected msg size larger than actual msg %d %d\n",
	bufcnt,
	msgcnt);
      post_msg_active--;
      return;
    }

    switch(t_cmmd){

    case	IOC_READ_NOTIFY:
    {
     	/* 	run the user's event handler			*/
      	/*	m_available points to event descriptor		*/
	struct event_handler_args args;

      	monix = (evid) t_available;


	/*
	 * Currently only the VAXs need data conversion
	 */
#     	ifdef VAX
        	(*cvrt[t_type])( hdrptr+1, hdrptr+1, FALSE, t_count);
#    	endif

	/*
	 *	Orig version of CA didnt use this strucure.
	 *	This would run faster if I had decided to
	 *	pass a pointer to this structure rather 
	 *	than the structure itself early on.
	 *
	 *	Pumping the arguments on the stack explicitly
	 *	could cause problems if a small item is in
	 *	the structure and we are on a SPARC processor.
	 *
	 *	call handler, only if they did not clear the chid 
	 *	in the interim
	 */
      	if(*monix->usr_func){
		args.usr = monix->usr_arg;
		args.chid = monix->chan;
		args.type = t_type;
		args.count = t_count;
		args.dbr = (void *) (hdrptr+1);

        	(*monix->usr_func)(args);
	}

     	LOCK;
      	lstDelete(&pend_read_list, monix); 
      	lstAdd(&free_event_list, monix);
      	UNLOCK;

      	break;
    }
    case	IOC_EVENT_ADD:
    {
     	 /* 	run the user's event handler			*/
     	 /*	m_available points to event descriptor		*/
	struct event_handler_args args;

      	monix = (evid) t_available;


      	/*  m_postsize = 0  is a confirmation of a monitor cancel  */
      	if( !t_postsize ){
		LOCK;
		lstDelete(&monix->chan->eventq, monix); 
        	lstAdd(&free_event_list, monix);
		UNLOCK;

       		 break;
      	}

      	/* only call if not disabled */
      	if(!monix->usr_func)
		break;

	/*
	 * Currently only the VAXs need data conversion
	 */
#    	ifdef VAX
       		(*cvrt[t_type])( hdrptr+1, hdrptr+1, FALSE, t_count);
#     	endif



	/*
	 *	Orig version of CA didnt use this strucure.
	 *	This would run faster if I had decided to
	 *	pass a pointer to this structure rather 
	 *	than the structure itself early on.
	 *
	 *	Pumping the arguments on the stack explicitly
	 *	could cause problems if a small item is in
	 *	the structure and we are on a SPARC processor.
	 *	
	 */
	args.usr = monix->usr_arg;
	args.chid = monix->chan;
	args.type = t_type;
	args.count = t_count;
	args.dbr = (void *) (hdrptr+1);

      	/* call their handler */
      	(*monix->usr_func)(args);

      break;
    }
    case	IOC_READ:
    case	IOC_READ_BUILD:
    {
      	chid	chan = (chid) hdrptr->m_pciu;

      	/* only count get returns if from the current read seq */
      	if(!VALID_MSG(piiu))
        	break;

if(t_postsize > (t_count-1) * dbr_value_size[t_type] + dbr_size[t_type])
	SEVCHK(ECA_INTERNAL,"about to violate user's buffer");

	/*
	 * Currently only the VAXs need data conversion
	 */
#     	ifdef VAX
        	(*cvrt[t_type])( hdrptr+1, t_available, FALSE, t_count);
#    	else
		/* in line is a little faster */
		if(t_postsize<=sizeof(int)){
	  		if(t_postsize==sizeof(long))
				*(long *)t_available = *(long *)(hdrptr+1);
	  		else if(t_postsize==sizeof(short))
				*(short *)t_available = *(short *)(hdrptr+1);
	  		else if(t_postsize==sizeof(char))
				*(char *)t_available = *(char *)(hdrptr+1);
		}else
          		memcpy(
				t_available,
				hdrptr+1,
				t_postsize);
#     	endif

	/*
	 * decrement the outstanding IO count
	 * 
	 * This relies on the IOC_READ_BUILD msg returning prior to the
	 * IOC_BUILD msg.
	 */
      	if(	t_cmmd != IOC_READ_BUILD || 
		(chan->connection_func == NULL && chan->state==cs_never_conn))
       		CLRPENDRECV;

      break;
    }
    case	IOC_SEARCH:
    case	IOC_BUILD:
    {
      	chid				chan = (chid) t_available;
      	struct ioc_in_use		*chpiiu = &iiu[chan->iocix];

      	if( chan->paddr ){

		if(chpiiu->sock_addr.sin_addr.s_addr==net_addr.s_addr){
	 		printf("burp ");
#ifdef UNIX
	  		fflush(stdout);
#endif
		}
        	else{
	  		char	msg[256];
	 		char	acc[64];
	  		char	rej[64];

	  		sprintf(acc,"%s",
				host_from_addr(chpiiu->sock_addr.sin_addr));
	  		sprintf(rej,"%s",host_from_addr(net_addr));
	  		sprintf(	
				msg,
				"Channel: %s Accepted: %s Rejected: %s ",
				chan+1,
				acc,
				rej);
          		ca_signal(ECA_DBLCHNL, msg);
		}

		/*
		 * IOC_BUILD messages allways have a IOC_READ msg following.
		 * (IOC_BUILD messages are sometimes followed by error
		 * messages which are ignored on double replies)
		 */
		if(t_cmmd == IOC_BUILD)
	  		msgcnt += sizeof(struct extmsg) + 
				(hdrptr+1)->m_postsize; 
	
		break;
      	}

      	if(!chan->connection_func && chan->state==cs_never_conn){
          	/* decrement the outstanding IO count */
          	CLRPENDRECV;
	}

      	LOCK;
		reconnect_channel(hdrptr,net_addr,piiu);
      	UNLOCK;

      	if(chan->connection_func){
		struct connection_handler_args	args;

		args.chid = chan;
		args.op = CA_OP_CONN_UP;
        	(*chan->connection_func)(args);
	}

      	break;
    }
    case IOC_READ_SYNC:
      	piiu->read_seq++;
     	break;

    case IOC_RSRV_IS_UP:
#	ifdef DEBUG
	printf(	"IOC on line ->[%s]\n",
		host_from_addr(t_available));
#	endif
      	LOCK;
      	mark_server_available(t_available);
      	chid_retry(TRUE);
      	UNLOCK;
      	break;

    case REPEATER_CONFIRM:
	ca_static->ca_repeater_contacted = TRUE;
#	ifdef DEBUG
		printf("repeater confirmation\n");
#	endif
	break;

    case IOC_NOT_FOUND:
    {
      	chid chix = (chid) t_available;
      	struct ioc_in_use	*piiu = &iiu[chix->iocix];

      	LOCK;
      	lstDelete(&piiu->chidlist, chix);
      	lstAdd(&iiu[BROADCAST_IIU].chidlist, chix);
      	chix->iocix = BROADCAST_IIU;
     	if(!piiu->chidlist.count)
        	close_ioc(piiu);
      	iiu[BROADCAST_IIU].nconn_tries = 0;
      	chid_retry(TRUE);
      	UNLOCK;

      	break;
    }

    case IOC_CLEAR_CHANNEL:
    {
      	chid chix = (chid) t_available;
      	struct ioc_in_use	*piiu = &iiu[chix->iocix];
      	register evid monix;


     	 LOCK;
      	/* remove any orphaned get callbacks for this channel */
     	for(	monix = (evid) pend_read_list.node.next; 
		monix; 
		monix = (evid) monix->node.next)
        if(monix->chan == chix){
          	lstDelete(&pend_read_list, monix); 
     	  	lstAdd(&free_event_list, monix);
        }

      	lstConcat(&free_event_list, &chix->eventq);
      	lstDelete(&piiu->chidlist, chix);
      	if(free(chix)<0)
		abort();
      	if(!piiu->chidlist.count)
        	close_ioc(piiu);
      	UNLOCK;
      	break;
    }
    case IOC_ERROR:
    {
      	char 				context[255];
      	char 				*name;
      	struct extmsg			*req = hdrptr+1;
     	int				op;
	struct exception_handler_args	args;


	/*
	 * dont process the message if they have disable notification
	 */
      	if(!ca_static->ca_exception_func)
		break;

      	name = (char *) host_from_addr(net_addr);
      	if(!name)
		name = "an unregistered IOC";

      	if(t_postsize>sizeof(struct extmsg))
        	sprintf(context, "detected by: %s for: %s", name, hdrptr+2);   
      	else
        	sprintf(context, "detected by: %s", name);  
      
	/*
	 * Map internal op id to external op id so I can freely change the
	 * protocol in the future. This is quite wasteful of space however.
	 */
      	switch(ntohs(req->m_cmmd)){
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


	args.usr = ca_static->ca_exception_arg;	/* user arg */
	args.chid = hdrptr->m_pciu;		/* the chid if appropriate */
	args.type = ntohs(req->m_type);		/* req type if approp */
	args.count = ntohs(req->m_count);	/* req count if approp */
	args.addr = (void *) (req->m_available);/* req user addr if approp */
	args.stat = ntohl((int)t_available);	/* the CA message code */
	args.op = op;				/* the CA operation */
	args.ctx = context;			/* context string */
        (*ca_static->ca_exception_func)(args);
      	break;
    }
    default:
      	printf("post_msg(): Corrupt Cmd in msg %x\n",t_cmmd);
      	abort();
    }

    bufcnt -= msgcnt;
    hdrptr  = (struct extmsg *) (msgcnt + (char *) hdrptr);    

  }


  post_msg_active--;

} 


/*
 *
 *	RECONNECT_CHANNEL()
 *	LOCK must be on
 *
 */
void reconnect_channel(hdrptr,net_addr,piiu)
register struct extmsg		*hdrptr;
struct in_addr			net_addr;
struct ioc_in_use		*piiu;
{
      	chid			chan = (chid) hdrptr->m_available;
	unsigned short		newiocix;
      	evid			pevent;
	int			status;

      	void			ca_request_event();

        status = alloc_ioc	(
				net_addr,
				IPPROTO_TCP,		
				&newiocix
				);
	if(status != ECA_NORMAL){
	  	printf("... %s ...\n", ca_message(status));
	 	printf("for %s on %s\n", chan+1, host_from_addr(net_addr));
	 	printf("ignored broadcast reply- proceeding\n");
	  	return;
	}

        /*	Update rmt chid fields from extmsg fields	*/
        chan->type  = ntohs(hdrptr->m_type);      
        chan->count = ntohs(hdrptr->m_count);      
        chan->paddr = hdrptr->m_pciu;

        if(chan->iocix != newiocix){
      		struct ioc_in_use	*chpiiu = &iiu[chan->iocix];
		/*
		 * The address changed (or was found for the first time)
		 */
	  	if(chan->iocix != BROADCAST_IIU)
	   		ca_signal(ECA_NEWADDR, chan+1);
          	lstDelete(&chpiiu->chidlist, chan);
          	chan->iocix = newiocix;
          	lstAdd(&iiu[newiocix].chidlist, chan);
        }


	/*
	 * NOTE: monitor and callback reissue must occur prior to calling
	 * their connection routine otherwise they could be requested twice.
	 */
#ifdef CALLBACK_REISSUE
        /* reissue any outstanding get callbacks for this channel */
	if(pend_read_list.count){
          	for(	pevent = (evid) pend_read_list.node.next; 
			pevent; 
			pevent = (evid) pevent->node.next)
            		if(pevent->chan == chan){
	      			issue_get_callback(pevent);
	      			send_msg();
	    		}
        }
#endif

        /* reissue any events (monitors) for this channel */
	if(chan->eventq.count){
          	for( 	pevent = (evid)chan->eventq.node.next; 
			pevent; 
			pevent = (evid)pevent->node.next)
	    		ca_request_event(pevent);
	 	send_msg();
 	}
	chan->state = cs_conn;
}

