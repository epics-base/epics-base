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
/*	xx0887	joh	Init Release					*/
/*	021291 	joh	Fixed vxWorks task name creation bug		*/
/*	032291	joh	source cleanup					*/
/*	041591	joh	added channel exsits routine			*/
/*	060591	joh	delinting					*/
/*	060691	joh	removed 4 byte count from the beginning of	*/
/*			each message					*/
/*	071291	joh	no longer sends id at TCP connect		*/
/*	082791	joh	split send_msg() into two subroutines		*/
/*	110491	joh	mark all channels disconnected prior to		*/
/*			calling the first connection handler on		*/
/*			disconnect					*/
/*	110491	joh	allow cac_send_msg() to be called recursively	*/
/*	110691	joh	call recv_msg to free up deadlock prior to 	*/
/*			each send until a better solution is found	*/
/*			(checking only when the socket blocks causes	*/
/*			does not leave enough margin to fix the		*/
/*			problem once it is detected)			*/
/*	120991	joh	better quit when unable to broadcast		*/
/*	022692	joh	better prefix on messages			*/
/*	031892	joh	initial rebroadcast delay is now a #define	*/
/*	042892	joh	made local routines static			*/
/*      050492 	joh  	dont call cac_send_msg() until all messages     */
/*                      have been processed to support batching         */
/*	050492	joh	added new fd array to select			*/
/*	072392	joh	use SO_REUSEADDR when testing to see		*/
/*			if the repeater has been started		*/
/*	072792	joh	better messages					*/
/*	101692	joh	defensive coding against unexpected errno's	*/
/*      120992	GeG	support  VMS/UCX		                */
/*	091493	joh	init send retry count when each recv and at	*/
/*			at connect					*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC socket interface module				*/
/*	File:	/.../ca/$Id$						*/
/*	Environment: VMS. UNIX, vxWorks					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	ioc socket interface module					*/
/*									*/
/*									*/
/*	Special comments						*/
/*	------- --------						*/
/* 	NOTE: Wallangong select does not return early if IO is 		*/
/*	present prior to the timeout expiration.			*/
/*									*/
/************************************************************************/
/*_end									*/

static char *sccsId = "$Id$\t$Date$";


/*	Allocate storage for global variables in this module		*/
#define			CA_GLBLSOURCE

#if defined(VMS)
#	include		<iodef.h>
#	include		<stsdef.h>
#	include		<sys/types.h>
#	define		__TIME /* dont include VMS CC time.h under MULTINET */
#	include		<sys/time.h>
#	include		<tcp/errno.h>
#	include		<sys/socket.h>
#	include		<netinet/in.h>
#	include		<netinet/tcp.h>
#  if defined(UCX)				/* GeG 09-DEC-1992 */
#	include		<sys/ucx$inetdef.h>
#	include		<ucx.h>
#  else
#	include		<vms/inetiodef.h>
#	include		<sys/ioctl.h>
#  endif
#else
#  if defined(UNIX)
#	include		<malloc.h>
#	include		<sys/types.h>
#	include		<sys/errno.h>
#	include		<sys/socket.h>
#	include		<netinet/in.h>
#	include		<netinet/tcp.h>
#	include		<sys/ioctl.h>
#  else
#    if defined(vxWorks)
#	include		<vxWorks.h>
#	include		<systime.h>
#	include		<ioLib.h>
#	include		<errno.h>
#	include		<socket.h>
#	include		<sockLib.h>
#	include		<tickLib.h>
#	include		<taskLib.h>
#	include		<selectLib.h>
#	include		<stdio.h>
#	include		<in.h>
#	include 	<tcp.h>
#	include		<ioctl.h>
#	include		<task_params.h>
#	include		<string.h>
#	include		<errnoLib.h>
#	include		<sysLib.h>
#	include		<taskVarLib.h>
#    endif
#  endif
#endif

#ifdef vxWorks
#include		<taskwd.h>
#endif
#include		<cadef.h>
#include		<net_convert.h>
#include		<iocmsg.h>
#include		<iocinf.h>

static struct timeval notimeout = {0,0};

#ifdef __STDC__
LOCAL void 	tcp_recv_msg(struct ioc_in_use *piiu);
LOCAL void 	cac_flush_internal();
LOCAL int 	cac_send_msg_piiu(struct ioc_in_use *piiu);
LOCAL void 	notify_ca_repeater();
LOCAL void 	recv_msg(struct ioc_in_use *piiu);
LOCAL void 	udp_recv_msg(struct ioc_in_use *piiu);
LOCAL void 	clean_iiu_list();
#ifdef VMS
LOCAL void 	vms_recv_msg_ast(struct ioc_in_use *piiu);
#endif

LOCAL int
create_net_chan(
struct ioc_in_use 	**ppiiu,
struct in_addr		*pnet_addr,
int			net_proto
);

#else

LOCAL void	tcp_recv_msg();
LOCAL void   	cac_flush_internal();
LOCAL int	cac_send_msg_piiu();
LOCAL void 	notify_ca_repeater();
LOCAL void	recv_msg();
LOCAL void	udp_recv_msg();
#ifdef VMS
void   		vms_recv_msg_ast();
#endif
LOCAL int 	create_net_chan();
LOCAL void 	clean_iiu_list();

#endif


/*
 * used to be that some TCP/IPs did not include this
 */
#ifndef NBBY
# define NBBY 8	/* number of bits per byte */
#endif




/*
 *	ALLOC_IOC()
 *
 * 	allocate and initialize an IOC info block for unallocated IOC
 *
 *	LOCK should be on while in this routine
 */
#ifdef __STDC__
int alloc_ioc(
struct in_addr			*pnet_addr,
int				net_proto,
struct ioc_in_use		**ppiiu
)
#else
int alloc_ioc(pnet_addr, net_proto, ppiiu)
struct in_addr			*pnet_addr;
int				net_proto;
struct ioc_in_use		**ppiiu;
#endif
{
	struct ioc_in_use	*piiu;
  	int			status;

	/*
	 * look for an existing connection
	 *
	 * quite a bottle neck with increasing connection count
	 *
	 */
	for(	piiu = (struct ioc_in_use *) iiuList.node.next;
		piiu;
		piiu = (struct ioc_in_use *) piiu->node.next){

    		if(	(pnet_addr->s_addr == piiu->sock_addr.sin_addr.s_addr)
			&& (net_proto == piiu->sock_proto)){

			if(!piiu->conn_up){
				continue;
			}

			*ppiiu = piiu;
      			return ECA_NORMAL;
		}
    	}

  	status = create_net_chan(ppiiu, pnet_addr, net_proto);
  	return status;

}


/*
 *	CREATE_NET_CHANNEL()
 *
 *	LOCK should be on while in this routine
 */
#ifdef __STDC__
LOCAL int create_net_chan(
struct ioc_in_use 	**ppiiu,
struct in_addr		*pnet_addr,
int			net_proto
)
#else
LOCAL int create_net_chan(ppiiu, pnet_addr, net_proto)
struct ioc_in_use 	**ppiiu;
struct in_addr		*pnet_addr;
int			net_proto;
#endif
{
	struct ioc_in_use	*piiu;
  	int			status;
  	int 			sock;
  	int			true = TRUE;
  	struct sockaddr_in	saddr;
  	int			i;

	piiu = (IIU *) calloc(1, sizeof(*piiu));
	if(!piiu){
		return ECA_ALLOCMEM;
	}

  	piiu->sock_addr.sin_addr = *pnet_addr;
  	piiu->sock_proto = net_proto;

   	/* 	set socket domain 	*/
  	piiu->sock_addr.sin_family = AF_INET;

  	/*	set the port 	*/
  	piiu->sock_addr.sin_port = htons(CA_SERVER_PORT);

	/*
	 * reset the delay to the next retry or keepalive
	 */
	piiu->next_retry = CA_CURRENT_TIME;
	piiu->nconn_tries = 0;
	piiu->retry_delay = CA_RECAST_DELAY;

  	switch(piiu->sock_proto)
  	{
    		case	IPPROTO_TCP:

      		/* 	allocate a socket	*/
      		sock = socket(	AF_INET,	/* domain	*/
				SOCK_STREAM,	/* type		*/
				0);		/* deflt proto	*/
      		if(sock == ERROR){
			free(piiu);
        		return ECA_SOCK;
		}

      		piiu->sock_chan = sock;

		/*
		 * see TCP(4P) this seems to make unsollicited single events
		 * much faster. I take care of queue up as load increases.
		 */
     		 status = setsockopt(
				sock,
				IPPROTO_TCP,
				TCP_NODELAY,
				(char *)&true,
				sizeof true);
      		if(status < 0){
			free(piiu);
        		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
       			return ECA_SOCK;
      		}

		/*
		 * This should cause the connection to be checked
		 * periodically and an error to be returned if it is lost?
		 * 
		 * In practice the conn is checked very infrequently.
		 */
      		status = setsockopt(
				sock,
				SOL_SOCKET,
				SO_KEEPALIVE,
				(char *)&true,
				sizeof true);
      		if(status < 0){
			free(piiu);
        		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
        		return ECA_SOCK;
    		}

#ifdef JUNKYARD
		{
			struct linger 	linger;
			int		linger_size = sizeof linger;
      			status = getsockopt(
				sock,
				SOL_SOCKET,
				SO_LINGER,
				&linger,
				&linger_size);
      			if(status < 0){
				abort(0);
      			}
			ca_printf(	"CAC: linger was on:%d linger:%d\n", 
				linger.l_onoff, 
				linger.l_linger);
		}
#endif

#ifdef CA_SET_TCP_BUFFER_SIZES
      		/* set TCP buffer sizes */
        	i = MAX_MSG_SIZE;
        	status = setsockopt(
				sock,
				SOL_SOCKET,
				SO_SNDBUF,
				&i,
				sizeof(i));
        	if(status < 0){
			free(piiu);
          		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
          		return ECA_SOCK;
        	}
        	i = MAX_MSG_SIZE;
        	status = setsockopt(
				sock,
				SOL_SOCKET,
				SO_RCVBUF,
				&i,
				sizeof(i));
        	if(status < 0){
			free(piiu);
          		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
          		return ECA_SOCK;
        	}
#endif

      		/* fetch the TCP send buffer size */
		i = sizeof(piiu->tcp_send_buff_size); 
        	status = getsockopt(
				sock,
				SOL_SOCKET,
				SO_SNDBUF,
				(char *)&piiu->tcp_send_buff_size,
				&i);
        	if(status < 0 || i != sizeof(piiu->tcp_send_buff_size)){
			free(piiu);
          		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
          		return ECA_SOCK;
        	}

      		/* connect */
      		status = connect(	
				sock,
				(struct sockaddr *)&piiu->sock_addr,
				sizeof(piiu->sock_addr));
      		if(status < 0){
			ca_printf("CAC: no conn errno %d\n", MYERRNO);
        		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
			free(piiu);
        		return ECA_CONN;
      		}
      		piiu->max_msg = MAX_TCP;

     	 	/*	
		 * Set non blocking IO for UNIX 
		 * to prevent dead locks	
		 */
#ifdef UNIX
        	status = socket_ioctl(
				piiu->sock_chan,
				FIONBIO,
				&true);
#endif

		/*
		 * Save the Host name for efficient access in the
		 * future.
		 */
		{
			char 	*ptmpstr;
	
			ptmpstr = host_from_addr(&piiu->sock_addr.sin_addr);
			strncpy(
				piiu->host_name_str, 
				ptmpstr, 
				sizeof(piiu->host_name_str)-1);
		}

      		break;

    	case	IPPROTO_UDP:
      		/* 	allocate a socket			*/
      		sock = socket(	AF_INET,	/* domain	*/
				SOCK_DGRAM,	/* type		*/
				0);		/* deflt proto	*/
      		if(sock == ERROR){
			free (piiu);
        		return ECA_SOCK;
		}

      		piiu->sock_chan = sock;


		/*
		 * The following only needed on BSD 4.3 machines
		 */
      		status = setsockopt(
				sock,
				SOL_SOCKET,
				SO_BROADCAST,
				(char *)&true,
				sizeof(true));
      		if(status<0){
			free(piiu);
        		ca_printf("CAC: sso (errno=%d)\n",MYERRNO);
        		status = socket_close(sock);
			if(status < 0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
        		return ECA_CONN;
      		}

      		memset((char *)&saddr,0,sizeof(saddr));
      		saddr.sin_family = AF_INET;
		/* 
		 * let slib pick lcl addr 
		 */
      		saddr.sin_addr.s_addr = htonl(INADDR_ANY); 
      		saddr.sin_port = htons(0);	

      		status = bind(	sock, 
				(struct sockaddr *) &saddr, 
				sizeof(saddr));
      		if(status<0){
        		ca_printf("CAC: bind (errno=%d)\n",MYERRNO);
			ca_signal(ECA_INTERNAL,"bind failed");
      		}

      		piiu->max_msg = MAX_UDP - sizeof(piiu->send->stk);

		/*
		 * Save the Host name for efficient access in the
		 * future.
		 */
		{
			strncpy(
				piiu->host_name_str, 
				"<<unknown host>>", 
				sizeof(piiu->host_name_str)-1);
		}

      		break;

	default:
		free(piiu);
      		ca_signal(ECA_INTERNAL,"alloc_ioc: ukn protocol\n");
		/*
		 * turn off gcc warnings
		 */
		return ECA_INTERNAL;
  	}

  	/* 	setup cac_send_msg(), recv_msg() buffers	*/
  	if(!piiu->send){
    		if(! (piiu->send = (struct buffer *) 
					malloc(sizeof(struct buffer))) ){
      			status = socket_close(sock);
			if(status < 0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
			free(piiu);
      			return ECA_ALLOCMEM;
    		}
	}

  	piiu->send->stk = 0;

  	if(!piiu->recv){
    		if(! (piiu->recv = (struct buffer *) 
					malloc(sizeof(struct buffer))) ){
      			status = socket_close(sock);
			if(status < 0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
			free(piiu);
      			return ECA_ALLOCMEM;
    		}
	}

	piiu->send_retry_count = SEND_RETRY_COUNT_INIT;
  	piiu->recv->stk = 0;
	piiu->active = FALSE;
  	if(fd_register_func){
		LOCKEVENTS;
		(*fd_register_func)(fd_register_arg, sock, TRUE);
		UNLOCKEVENTS;
	}


  	/*	Set up recv thread for VMS	*/
#if defined(VMS)
  	{
		/*
		 * request to be informed of future IO
		 */
    		status = sys$qio(
			NULL,
			sock,
			IO$_RECEIVE,
			&piiu->iosb,
			vms_recv_msg_ast,
			piiu,
			&peek_ast_buf,
			sizeof(peek_ast_buf),
			MSG_PEEK,
			&piiu->recvfrom,
			sizeof(piiu->recvfrom),
			NULL);
    		if(status != SS$_NORMAL){
      			lib$signal(status);
      			exit();
    		}
  	}
#endif

	/*
	 * add to the list of active IOCs
	 */
	ellAdd(&iiuList, &piiu->node);

	piiu->conn_up = TRUE;
	*ppiiu = piiu;

  	return ECA_NORMAL;
}



/*
 *	NOTIFY_CA_REPEATER()
 *
 *	tell the cast repeater that another client has come on line 
 *
 *	NOTES:
 *	1)	local communication only (no LAN traffic)
 *
 * 	LOCK should be on while in this routine
 * 	(MULTINET TCP/IP routines are not reentrant)
 */
LOCAL void notify_ca_repeater()
{
	struct sockaddr_in	saddr;
	int			status;

	if(!piiuCast)
		return;
	if(!piiuCast->conn_up)
		return;

     	status = local_addr(piiuCast->sock_chan, &saddr);
	if(status == OK){
      		saddr.sin_port = htons(CA_CLIENT_PORT);	
      		status = sendto(
			piiuCast->sock_chan,
        		NULL,
        		0, /* zero length message */
        		0,
       			(struct sockaddr *)&saddr, 
			sizeof saddr);
      		if(status < 0){
			if(	MYERRNO == EINTR || 
				MYERRNO == ENOBUFS || 
				MYERRNO == EWOULDBLOCK){
				TCPDELAY;
			}
			else{
				ca_printf(
	"CAC: notify_ca_repeater: send to lcl addr failed\n");
				abort(0);
			}
		}
	}
}


/*
 * CAC_SEND_MSG()
 * 
 * 
 * LOCK should be on while in this routine
 */
void cac_send_msg()
{
  	register struct ioc_in_use 	*piiu;
	int				done;
	int				status;

  	for(	piiu=(IIU *)iiuList.node.next; 
		piiu; 
		piiu=(IIU *)piiu->node.next){

		piiu->send_retry_count = SEND_RETRY_COUNT_INIT;
	}

  	if(!ca_static->ca_repeater_contacted)
		notify_ca_repeater();

	send_msg_active++;

	/*
	 * Dont exit until all connections are flushed
 	 *
	 */
	while(TRUE){
		/*
		 * Ensure we do not accumulate extra recv
		 * messages (for TCP)
		 *
		 * frees up push pull deadlock only
		 * if recv not already in progress
		 */
#		if defined(UNIX)
			if(post_msg_active==0){
				recv_msg_select(&notimeout);
			}
#		endif

		done = TRUE;
  		for(	piiu=(IIU *)iiuList.node.next; 
			piiu; 
			piiu=(IIU *)piiu->node.next){

    			if(!piiu->send->stk || !piiu->conn_up)
				continue;

			status = cac_send_msg_piiu(piiu);
			if(status<0){
				if(piiu->send_retry_count == 0){
					ca_signal(
						ECA_DLCKREST, 	
						piiu->host_name_str);
					close_ioc(piiu);
				}
				else{
					piiu->send_retry_count--;
					done = FALSE;
				}
			}
    		}

		if(done){
			/*
			 * always double check that we
			 * are finished in case somthing was added 
			 * to a send buffer and a recursive 
			 * ca_send_msg() call was refused above
			 */
  			for(	piiu=(IIU *)iiuList.node.next; 
				piiu; 
				piiu=(IIU *)piiu->node.next){

    				if(piiu->send->stk && piiu->conn_up){
					done = FALSE;
				}
			}
			if(done)
				break;
		}

		TCPDELAY;
	}

	send_msg_active--;
}


/*
 *	CAC_SEND_MSG_PIIU()
 *
 * 
 */
#ifdef __STDC__
LOCAL int cac_send_msg_piiu(struct ioc_in_use *piiu)
#else
LOCAL int cac_send_msg_piiu(piiu)
struct ioc_in_use 	*piiu;
#endif
{
  	unsigned 		cnt;
  	void			*pmsg;    
  	int			status;

	cnt = piiu->send->stk;
	pmsg = (void *) piiu->send->buf;	

	while(TRUE){

		status = sendto(
				piiu->sock_chan,
				pmsg,	
				cnt,
				0,
				(struct sockaddr *)&piiu->sock_addr,
				sizeof(piiu->sock_addr));

		/*
		 * normal fast exit
		 */
		if(status == cnt){
			break;
		}
		else if(status>0){
			if(status>cnt){
				ca_signal(
					ECA_INTERNAL,
					"more sent than requested ?");
			}

			cnt = cnt-status;
			pmsg = (void *) (status+(char *)pmsg);
		}
		else if(status == 0){
			ca_printf("CAC: sent zero ?\n");
			TCPDELAY;
		}
		else if(MYERRNO == EWOULDBLOCK ||
			MYERRNO == ENOBUFS ||
			MYERRNO == EINTR){
			if(pmsg !=  piiu->send->buf){
				/*
				 * realign the message if this
				 * was a partial send
				 */
				memcpy(	piiu->send->buf,
					pmsg,
					cnt);
				piiu->send->stk = cnt;
			}

			return ERROR;
		}
		else{
			if(	MYERRNO != EPIPE && 
				MYERRNO != ECONNRESET &&
				MYERRNO != ETIMEDOUT){
				ca_printf(	
					"CAC: error on socket send() %d\n",
					MYERRNO);
			}
			close_ioc(piiu);
			return OK;
		}


	}

      	/* reset send stack */
      	piiu->send->stk = 0;
        piiu->send_needed = FALSE;

	/*
	 * reset the delay to the next keepalive
	 */
    	if(piiu != piiuCast){
		piiu->next_retry = time(NULL) + CA_RETRY_PERIOD;
	}

	return OK;
}


/*
 *
 * cac_flush_internal()
 *
 * Flush the output - but dont block
 *
 */
LOCAL void cac_flush_internal()
{
	register struct ioc_in_use      *piiu;

	LOCK
	for(	piiu = (IIU *)iiuList.node.next; 
		piiu; 
		piiu = (IIU *)piiu->node.next){

		if(piiu->send->stk==0 || !piiu->send_needed || !piiu->conn_up)
			continue;

		cac_send_msg_piiu(piiu);
	}
	UNLOCK
}


/*
 *	RECV_MSG_SELECT()
 *
 * 	Asynch notification of incomming messages under UNIX
 *	1) Wait no longer than timeout 
 *	2) Return early if nothing outstanding
 * 
 */
#if defined(UNIX) || defined(vxWorks)
#ifdef __STDC__
void recv_msg_select(struct timeval  *ptimeout)
#else
void recv_msg_select(ptimeout)
struct timeval 	*ptimeout;
#endif
{
  	long				status;
  	register struct ioc_in_use 	*piiu;
	struct timeval			timeout;

  	while(TRUE){

		LOCK;
    		for(	piiu=(IIU *)iiuList.node.next;
			piiu;
			piiu=(IIU *)piiu->node.next){

			if(!piiu->conn_up){
				continue;
			}

       			FD_SET(piiu->sock_chan,&readch);
       			FD_SET(piiu->sock_chan,&excepch);
		}
		UNLOCK;

    		status = select(
				sizeof(fd_set)*NBBY,
				&readch,
				NULL,
				&excepch,
				ptimeout);

  		if(status<=0){  
			if(status == 0)
				break;
                                             
    			if(MYERRNO == EINTR){
				ca_printf("CAC: select was interrupted\n");
				TCPDELAY;
				continue;
			}
    			else if(MYERRNO == EWOULDBLOCK){
				ca_printf("CAC: blocked at select ?\n");
				break;
    			}                                           
    			else{
				char text[255];                                         
     				sprintf(
					text,
					"CAC: unexpected select fail: %d",
					MYERRNO); 
      				ca_signal(ECA_INTERNAL,text);   
			}
		}                                                         

		/*
		 * I dont want to lock while traversing
		 * this list so that we are free to take 
		 * it at a lower level
		 *
		 * I know that an item can be added to the
		 * end of the list while traversing it
		 * - no lock required here.
		 *
		 * I know that an item is only deleted from 
		 * this list in this routine
		 * - no lock required here.
		 *
		 * LOCK is applied when if a block is added
		 */
    		for(	piiu=(IIU *)iiuList.node.next;
			piiu;
			piiu=(IIU *)piiu->node.next){

			if(!piiu->conn_up){
				continue;
			}

      			if(FD_ISSET(piiu->sock_chan,&readch) ||
				FD_ISSET(piiu->sock_chan,&excepch)){
          			recv_msg(piiu);
			}
		}

  		/*
   		 * double check to make sure that nothing is left pending
  	 	 */
		timeout.tv_sec =0;
		timeout.tv_usec = 0;
		ptimeout = &timeout;
  	}

	LOCK;
	clean_iiu_list();
	UNLOCK;

	cac_flush_internal();
}
#endif


/*
 * clean_iiu_list()
 */
LOCAL void clean_iiu_list()
{
	IIU	*piiu;

	piiu=(IIU *)iiuList.node.next;
    	while(piiu){
		IIU	*pnext;

		/*
		 * iiu block may be deleted here
		 * so I save the pointer to the next
		 * block 
	 	 */
		pnext=(IIU *)piiu->node.next;
		if(!piiu->conn_up){
			ellDelete(&iiuList, &piiu->node);
			free(piiu);
		}
		piiu=pnext;
	}
}


/*
 *	RECV_MSG()
 *
 *
 */
#ifdef __STDC__
LOCAL void recv_msg(struct ioc_in_use *piiu)
#else
LOCAL void recv_msg(piiu)
struct ioc_in_use	*piiu;
#endif
{
	if(!piiu->conn_up){
		return;
	}

	piiu->active = TRUE;

  	switch(piiu->sock_proto){
  	case	IPPROTO_TCP:

    		tcp_recv_msg(piiu);
    		flow_control(piiu);

    		break;
	
  	case 	IPPROTO_UDP:
    		udp_recv_msg(piiu);
    		break;

  	default:
    		ca_printf("CAC: cac_send_msg: ukn protocol\n");
    		abort(0);
  	}  

	piiu->active = FALSE;
}


/*
 * TCP_RECV_MSG()
 *
 */
#ifdef __STDC__
LOCAL void tcp_recv_msg(struct ioc_in_use *piiu)
#else
LOCAL void tcp_recv_msg(piiu)
struct ioc_in_use	*piiu;
#endif
{
  	long			byte_cnt;
  	int			status;
  	struct buffer		*rcvb = 	piiu->recv;
  	int			sock = 		piiu->sock_chan;

  	if(!ca_static->ca_repeater_contacted){
		LOCK;
		notify_ca_repeater();
		UNLOCK;
	}
 	status = recv(	sock,
			&rcvb->buf[rcvb->stk],
			sizeof(rcvb->buf) - rcvb->stk,
			0);
	if(status == 0){
		LOCK;
		close_ioc(piiu);
		UNLOCK;
		return;
	}
    	else if(status <0){
    		/* try again on status of -1 and no luck this time */
      		if(MYERRNO == EWOULDBLOCK || MYERRNO == EINTR){
   			TCPDELAY;
			return;
		}

       	 	if(	MYERRNO != EPIPE && 
			MYERRNO != ECONNRESET &&
			MYERRNO != ETIMEDOUT){
	  		ca_printf(	"CAC: unexpected recv error (errno=%d)\n",
				MYERRNO);
		}
		LOCK;
		close_ioc(piiu);
		UNLOCK;
		return;
    	}


  	byte_cnt = (long) status;
  	if(byte_cnt>MAX_MSG_SIZE){
    		ca_printf(	"CAC: recv_msg(): message overflow %l\n",
			byte_cnt-MAX_MSG_SIZE);
		LOCK;
		close_ioc(piiu);
		UNLOCK;
    		return;
 	}

	piiu->send_retry_count = SEND_RETRY_COUNT_INIT;

	rcvb->stk += byte_cnt;

  	/* post message to the user */
	byte_cnt = rcvb->stk;
  	status = post_msg(
			(struct extmsg *)rcvb->buf, 
			&byte_cnt, 
			&piiu->sock_addr.sin_addr, 
			piiu);
	if(status != OK){
		LOCK;
		close_ioc(piiu);
		UNLOCK;
		return;
	}
	if(byte_cnt>0){
		/*
		 * realign partial message
		 */
		memcpy(	rcvb->buf, 
			rcvb->buf + rcvb->stk - byte_cnt, 
			byte_cnt);
#ifdef DEBUG
		ca_printf(	"CAC: realigned message of %d bytes\n", 
			byte_cnt);
#endif
	}
	rcvb->stk = byte_cnt;

	/*
	 * reset the delay to the next keepalive
	 */
    	if(piiu != piiuCast){
		piiu->next_retry = time(NULL) + CA_RETRY_PERIOD;
	}

 	return;
}



/*
 *	UDP_RECV_MSG()
 *
 */
#ifdef __STDC__
LOCAL void udp_recv_msg(struct ioc_in_use *piiu)
#else
LOCAL void udp_recv_msg(piiu)
struct ioc_in_use	*piiu;
#endif
{
  	int			status;
  	struct buffer		*rcvb = 	piiu->recv;
  	int			sock = 		piiu->sock_chan;
  	int			reply_size;
  	unsigned		nchars;
	struct msglog{
  		long			nbytes;
  		struct sockaddr_in	addr;
	};
	struct msglog	*pmsglog;
	


  	rcvb->stk =0;	
  	while(TRUE){

		pmsglog = (struct msglog *) &rcvb->buf[rcvb->stk];
    		rcvb->stk += sizeof(*pmsglog);

  		reply_size = sizeof(pmsglog->addr);
    		status = recvfrom(	
				sock,
				&rcvb->buf[rcvb->stk],
				MAX_UDP,
				0,
				(struct sockaddr *)&pmsglog->addr, 
				&reply_size);
    		if(status < 0){
			/*
			 * op would block which is ok to ignore till ready
			 * later
			 */
      			if(MYERRNO == EWOULDBLOCK || MYERRNO == EINTR)
        			break;
      			ca_signal(ECA_INTERNAL,"unexpected udp recv error");
    		}
		else if(status == 0){
			break;
		}

		/*	
		 * log the msg size
		 */
		rcvb->stk += status;
		pmsglog->nbytes = (long) status;
#ifdef DEBUG
      		ca_printf("CAC: recieved a udp reply of %d bytes\n",byte_cnt);
#endif

    		if(rcvb->stk + MAX_UDP + sizeof(*pmsglog) > MAX_MSG_SIZE)
      			break;

    		/*
    		 * More comming ?
    		 */
    		status = socket_ioctl(	sock,
					FIONREAD,
					(int) &nchars);
    		if(status<0)
      			ca_signal(ECA_INTERNAL,"unexpected udp ioctl err\n");

		if(nchars==0)
			break;
  	}

	pmsglog = (struct msglog *) rcvb->buf;
 	while(pmsglog < (struct msglog *)&rcvb->buf[rcvb->stk]){	
		long 	msgcount;

  		/* post message to the user */
		msgcount = pmsglog->nbytes;
  		status = post_msg(
				(struct extmsg *)(pmsglog+1), 
				&msgcount,
				&pmsglog->addr.sin_addr,
				piiu);
		if(status != OK || msgcount != 0){
			ca_printf("CAC: bad UDP msg from port=%d addr=%x align=%d\n",
				pmsglog->addr.sin_port,
				pmsglog->addr.sin_addr.s_addr,
				msgcount);
		}

		pmsglog = (struct msglog *)
			(pmsglog->nbytes + (char *) (pmsglog+1)); 
  	}
	  
  	return; 
}




#ifdef vxWorks
/*
 * RECV_TASK()
 *
 */
#ifdef __STDC__
void cac_recv_task(int	tid)
#else
void cac_recv_task(tid)
int		tid;
#endif
{
	struct timeval	timeout;
  	int		status;

	taskwdInsert((int) taskIdCurrent, NULL, NULL);

	status = ca_import(tid);
	SEVCHK(status, NULL);

	/*
	 * detects connection loss and self exits
	 * in close_ioc()
	 */
  	while(TRUE){
		timeout.tv_usec = 0;
		timeout.tv_sec = 20;
		recv_msg_select(&timeout);
	}
}
#endif


/*
 *
 *	VMS_RECV_MSG_AST()
 *
 *
 */
#ifdef VMS
#ifdef __STDC__
LOCAL void vms_recv_msg_ast(struct ioc_in_use *piiu)
#else
LOCAL void vms_recv_msg_ast(piiu)
struct ioc_in_use	*piiu;
#endif
{
  	short		io_status;

	io_status = piiu->iosb.status;

  	if(io_status != SS$_NORMAL){
    		close_ioc(piiu);
    		if(io_status != SS$_CANCEL)
      			lib$signal(io_status);
    		return;
  	}

  	if(!ca_static->ca_repeater_contacted)
		notify_ca_repeater();

	recv_msg(piiu);
	clean_iiu_list();
	cac_flush_internal();
	  
	/*
	 * request to be informed of future IO
	 */
  	io_status = sys$qio(
			NULL,
			piiu->sock_chan,
			IO$_RECEIVE,
			&piiu->iosb,
			vms_recv_msg_ast,
			piiu,
			&peek_ast_buf,
			sizeof(peek_ast_buf),
			MSG_PEEK,
			&piiu->recvfrom,
			sizeof(piiu->recvfrom),
			NULL);
  	if(io_status != SS$_NORMAL)
    		lib$signal(io_status);      
  	return;
}
#endif


/*
 *
 *	CLOSE_IOC()
 *
 *	set an iiu in the disconnected state
 *
 *
 *	NOTES:
 *	Lock must be applied while in this routine
 */
#ifdef __STDC__
void close_ioc(struct ioc_in_use *piiu)
#else
void close_ioc(piiu)
struct ioc_in_use	*piiu;
#endif
{
  	chid				chix;
  	struct connection_handler_args	args;
	int				status;

	if(!piiu->conn_up){
		return;
	}
	piiu->conn_up = FALSE;

	if(piiu == piiuCast){
		piiuCast = NULL;
	}

  	/*
  	 * reset send stack- discard pending ops when the conn broke (assume
  	 * use as UDP buffer during disconn)
   	 */
  	piiu->send->stk = 0;
  	piiu->recv->stk = 0;
  	piiu->max_msg = MAX_UDP;
	piiu->active = FALSE;

	/*
	 * reset the delay to the next retry
	 */
	piiu->next_retry = CA_CURRENT_TIME;
	piiu->nconn_tries = 0;
	piiu->retry_delay = CA_RECAST_DELAY;

# 	if defined(UNIX) || defined(vxWorks)
  	/* clear unused select bit */
  	FD_CLR(piiu->sock_chan, &readch);
       	FD_CLR(piiu->sock_chan, &excepch);
# 	endif 

	/*
	 * Mark all of their channels disconnected
	 * prior to calling handlers incase the
	 * handler tries to use a channel before
	 * I mark it disconnected.
	 */
  	chix = (chid) &piiu->chidlist.node.next;
  	while(chix = (chid) chix->node.next){
    		chix->type = TYPENOTCONN;
    		chix->count = 0;
    		chix->state = cs_prev_conn;
    		chix->id.sid = ~0L;
  	}

  	if(piiu->chidlist.count){
    		ca_signal(
			ECA_DISCONN, 
			piiu->host_name_str);
	}

	/*
	 * call their connection handler as required
	 */
  	chix = (chid) &piiu->chidlist.node.next;
  	while(chix = (chid) chix->node.next){
    		if(chix->connection_func){
			args.chid = chix;
			args.op = CA_OP_CONN_DOWN;
			LOCKEVENTS;
      			(*chix->connection_func)(args);
			UNLOCKEVENTS;
    		}
		chix->piiu = piiuCast;
  	}

	/*
	 * move all channels to the broadcast IIU
	 */
	if(piiuCast){
		ellConcat(&piiuCast->chidlist, &piiu->chidlist);
	}

  	if(fd_register_func){
		LOCKEVENTS;
		(*fd_register_func)(fd_register_arg, piiu->sock_chan, FALSE);
		UNLOCKEVENTS;
	}

  	status = socket_close(piiu->sock_chan);
	if(status < 0){
		SEVCHK(ECA_INTERNAL,NULL);
	}
  	piiu->sock_chan = -1;

	if(!piiuCast){
		ca_printf("Unable to broadcast- channels will not reconnect\n");
	}
}



/*
 *	REPEATER_INSTALLED()
 *
 *	Test for the repeater allready installed
 *
 *	NOTE: potential race condition here can result
 *	in two copies of the repeater being spawned
 *	however the repeater detectes this, prints a message,
 *	and lets the other task start the repeater.
 *
 *	QUESTION: is there a better way to test for a port in use? 
 *	ANSWER: none that I can find.
 *
 *	Problems with checking for the repeater installed
 *	by attempting to bind a socket to its address
 *	and port.
 *
 *	1) Closed socket may not release the bound port
 *	before the repeater wakes up and tries to grab it.
 *	Attempting to bind the open socket to another port
 *	also does not work.
 *
 * 	072392 - problem solved by using SO_REUSEADDR
 */
int repeater_installed()
{
  	int				status;
  	int 				sock;
  	struct sockaddr_in		bd;

	int 				installed = FALSE;

     	/* 	allocate a socket			*/
      	sock = socket(	AF_INET,	/* domain	*/
			SOCK_DGRAM,	/* type		*/
			0);		/* deflt proto	*/
      	if(sock == ERROR){
        	abort(0);
	}

	status = setsockopt(	sock,
				SOL_SOCKET,
				SO_REUSEADDR,
				NULL,
				0);
	if(status<0){
		ca_printf(      "CAC: set socket option reuseaddr failed\n");
	}

      	memset((char *)&bd,0,sizeof bd);
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = htonl(INADDR_ANY);	
     	bd.sin_port = htons(CA_CLIENT_PORT);	
      	status = bind(	sock, 
			(struct sockaddr *) &bd, 
			sizeof bd);
     	if(status<0)
		if(MYERRNO == EADDRINUSE)
			installed = TRUE;
		else
			abort(0);

	status = socket_close(sock);
	if(status<0){
		SEVCHK(ECA_INTERNAL,NULL);
	}
		

	return installed;
}
