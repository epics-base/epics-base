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
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC socket interface module				*/
/*	File:	/.../ca/iocinf.c					*/
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


/*	Allocate storage for global variables in this module		*/
#define			CA_GLBLSOURCE

#if defined(VMS)
#	include		<iodef.h>
#	include		<stsdef.h>
#	include		<sys/types.h>
#	define		__TIME /* dont include VMS CC time.h under MULTINET */
#	include		<sys/time.h>
#	include		<vms/inetiodef.h>
#	include		<tcp/errno.h>
#	include		<sys/socket.h>
#	include		<netinet/in.h>
#	include		<netinet/tcp.h>
#	include		<sys/ioctl.h>
#elif defined(UNIX)
#	include		<sys/types.h>
#	include		<sys/errno.h>
#	include		<sys/socket.h>
#	include		<netinet/in.h>
#	include		<netinet/tcp.h>
#	include		<sys/ioctl.h>
#elif defined(vxWorks)
#	include		<vxWorks.h>
#	ifdef V5_vxWorks
#		include		<systime.h>
#	else
#		include		<utime.h>
#	endif
#	include		<ioLib.h>
#	include		<errno.h>
#	include		<socket.h>
#	include		<in.h>
#	include		<tcp.h>
#	include		<ioctl.h>
#	include		<task_params.h>
#endif

#include		<cadef.h>
#include		<net_convert.h>
#include		<iocmsg.h>
#include		<iocinf.h>

static struct timeval notimeout = {0,0};

void 	close_ioc();
void	recv_msg();
void	tcp_recv_msg();
void	udp_recv_msg();
void	notify_ca_repeater();
int	cac_send_msg_piiu();
#ifdef VMS
void   	vms_recv_msg_ast();
#endif


/*
 * used to be that some TCP/IPs did not include this
 */
#ifdef JUNKYARD
typedef long	fd_mask;
typedef	struct fd_set {
	fd_mask	fds_bits[64];
} fd_set;

#ifndef NBBY
# define NBBY 8	/* number of bits per byte */
#endif

#ifndef NFDBITS
#define NFDBITS (sizeof(int) * NBBY) /* bits per mask */
#endif

#ifndef FD_SET
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#endif

#ifndef FD_ISSET
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#endif

#ifndef FD_CLR
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#endif

/* my version is faster since it is in line 	*/
#define FD_ZERO(p)\
{\
  register int i;\
  for(i=0;i<NELEMENTS((p)->fds_bits);i++)\
    (p)->fds_bits[i]=0;\
}
#endif




/*
 *	ALLOC_IOC()
 *
 * 	allocate and initialize an IOC info block for unallocated IOC
 *
 *	LOCK should be on while in this routine
 */
alloc_ioc(pnet_addr, net_proto, iocix)
struct in_addr			*pnet_addr;
int				net_proto;
unsigned short			*iocix;
{
  	int			i;
  	int			status;

  	/****************************************************************/
  	/* 	IOC allready allocated ?				*/
  	/****************************************************************/
  	for(i=0;i<nxtiiu;i++)
    		if(	(pnet_addr->s_addr == iiu[i].sock_addr.sin_addr.s_addr)
			&& (net_proto == iiu[i].sock_proto)){

      		if(!iiu[i].conn_up){
			/* an old connection is resumed */
        		status = create_net_chan(&iiu[i]);
        		if(status != ECA_NORMAL)
          			return status;
        		ca_signal(
				ECA_NEWCONN,
				iiu[i].host_name_str);
      		}

      		*iocix = i;
      		return ECA_NORMAL;
    	}

  	/* 	is there an IOC In Use block available to allocate	*/
  	if(nxtiiu>=MAXIIU)
    		return ECA_MAXIOC;

  	/* 	set network address block	*/
  	iiu[nxtiiu].sock_addr.sin_addr = *pnet_addr;
  	iiu[nxtiiu].sock_proto = net_proto;
  	status = create_net_chan(&iiu[nxtiiu]);
  	if(status != ECA_NORMAL)
    		return status;

  	*iocix = nxtiiu++;

  	return ECA_NORMAL;

}


/*
 *	CREATE_NET_CHANNEL()
 *
 *	LOCK should be on while in this routine
 */
static int
create_net_chan(piiu)
struct ioc_in_use		*piiu;
{
  	int			status;
  	int 			sock;
  	int			true = TRUE;
  	struct sockaddr_in	saddr;
  	int			i;

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
      		if(sock == ERROR)
        		return ECA_SOCK;

      		piiu->sock_chan = sock;

		/*
		 * see TCP(4P) this seems to make unsollicited single events
		 * much faster. I take care of queue up as load increases.
		 */
     		 status = setsockopt(
				sock,
				IPPROTO_TCP,
				TCP_NODELAY,
				&true,
				sizeof true);
      		if(status < 0){
        		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
       			return ECA_SOCK;
      		}

#ifdef KEEPALIVE
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
				&true,
				sizeof true);
      		if(status < 0){
        		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
        		return ECA_SOCK;
    		}
#endif

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
				abort();
      			}
			ca_printf(	"CAC: linger was on:%d linger:%d\n", 
				linger.l_onoff, 
				linger.l_linger);
		}
#endif

      		/* set TCP buffer sizes only if BSD 4.3 sockets */
      		/* temporarily turned off */
#ifdef JUNKYARD  

        	i = (MAX_MSG_SIZE+sizeof(int)) * 2;
        	status = setsockopt(
				sock,
				SOL_SOCKET,
				SO_SNDBUF,
				&i,
				sizeof(i));
        	if(status < 0){
          		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
          		return ECA_SOCK;
        	}
        	status = setsockopt(
				sock,
				SOL_SOCKET,
				SO_RCVBUF,
				&i,
				sizeof(i));
        	if(status < 0){
          		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
          		return ECA_SOCK;
        	}
#endif

      		/* connect */
      		status = connect(	
				sock,
				&piiu->sock_addr,
				sizeof(piiu->sock_addr));
      		if(status < 0){
			ca_printf("CAC: no conn errno %d\n", MYERRNO);
        		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
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

      		break;

    	case	IPPROTO_UDP:
      		/* 	allocate a socket			*/
      		sock = socket(	AF_INET,	/* domain	*/
				SOCK_DGRAM,	/* type		*/
				0);		/* deflt proto	*/
      		if(sock == ERROR)
        		return ECA_SOCK;

      		piiu->sock_chan = sock;


		/*
		 * The following only needed on BSD 4.3 machines
		 */
      		status = setsockopt(
				sock,
				SOL_SOCKET,
				SO_BROADCAST,
				&true,
				sizeof(true));
      		if(status<0){
        		ca_printf("CAC: sso (errno=%d)\n",MYERRNO);
        		status = socket_close(sock);
			if(status < 0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
        		return ECA_CONN;
      		}

      		memset(&saddr,0,sizeof(saddr));
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

      		break;

    		default:
      			ca_signal(ECA_INTERNAL,"alloc_ioc: ukn protocol\n");
  	}
  	/* 	setup cac_send_msg(), recv_msg() buffers	*/
  	if(!piiu->send){
    		if(! (piiu->send = (struct buffer *) 
					malloc(sizeof(struct buffer))) ){
      			status = socket_close(sock);
			if(status < 0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
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
      			return ECA_ALLOCMEM;
    		}
	}

	/*
	 * Save the Host name for efficient access in the
	 * future.
	 */
	{
		char 	*ptmpstr;
		int	len;
	
		ptmpstr = host_from_addr(&piiu->sock_addr.sin_addr);
		strncpy(
			piiu->host_name_str, 
			ptmpstr, 
			sizeof(piiu->host_name_str)-1);
	}

  	piiu->recv->stk = 0;
  	piiu->conn_up = TRUE;
  	if(fd_register_func){
		LOCKEVENTS;
		(*fd_register_func)(fd_register_arg, sock, TRUE);
		UNLOCKEVENTS;
	}


  	/*	Set up recv thread for VMS	*/
#	if defined(VMS)
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
# 	elif defined(vxWorks)
  	{  
      		void	recv_task();
      		int 	pri;
      		char	name[15];

      		status == taskPriorityGet(VXTASKIDSELF, &pri);
      		if(status<0)
			ca_signal(ECA_INTERNAL,NULL);

      		strcpy(name,"RD ");                   
      		strncat(
			name, 
			taskName(VXTHISTASKID), 
			sizeof(name)-strlen(name)-1); 

      		status = taskSpawn(
				name,
				pri-1,
				VX_FP_TASK,
				4096,
				recv_task,
				piiu,
				taskIdSelf());
      		if(status<0)
			ca_signal(ECA_INTERNAL,NULL);

      		piiu->recv_tid = status;

  	}
#	endif

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
static void 
notify_ca_repeater()
{
	struct sockaddr_in	saddr;
	int			status;

	if(!iiu[BROADCAST_IIU].conn_up)
		return;

     	status = local_addr(iiu[BROADCAST_IIU].sock_chan, &saddr);
	if(status == OK){
      		saddr.sin_port = htons(CA_CLIENT_PORT);	
      		status = sendto(
			iiu[BROADCAST_IIU].sock_chan,
        		NULL,
        		0, /* zero length message */
        		0,
       			&saddr, 
			sizeof saddr);
      		if(status < 0){
			ca_printf("CAC: notify_ca_repeater: send to lcl addr failed\n");
			abort();
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
	int 				retry_count;
#	define				RETRY_INIT 100

	retry_count = RETRY_INIT;

  	if(!ca_static->ca_repeater_contacted)
		notify_ca_repeater();

#if 0
	/*
	 * dont call it recursively
	 */
	if(send_msg_active){
		return;
	}
#endif

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
#		ifdef 	UNIX
			if(post_msg_active==0)
				recv_msg_select(&notimeout);
#		endif

		done = TRUE;
  		for(piiu=iiu; piiu<&iiu[nxtiiu]; piiu++){

    			if(!piiu->send->stk)
				continue;

			status = cac_send_msg_piiu(piiu);
			if(status<0){
				done = FALSE;
			}
    		}

#ifndef UNIX
		break;
#else
		if(done){
			/*
			 * allways double check that we
			 * are finished in case somthing was added 
			 * to a send buffer and a recursive 
			 * ca_send_msg() call was refused above
			 */
  			for(piiu=iiu;piiu<&iiu[nxtiiu];piiu++){
    				if(piiu->send->stk){
					done = FALSE;
				}
			}
			if(done)
				break;
		}

		if(retry_count-- <= 0){
			char *iocname;
			struct in_addr *inaddr;
  			for(piiu=iiu; piiu<&iiu[nxtiiu]; piiu++){
    				if(piiu->send->stk){
					inaddr = &piiu->sock_addr.sin_addr;
					iocname = piiu->host_name_str;
#ifdef CLOSE_ON_EXPIRED			
					ca_signal(ECA_DLCKREST, iocname);
					close_ioc(piiu);
#else
					ca_signal(ECA_SERVBEHIND, iocname);
#endif
				}
			}
#ifndef CLOSE_ON_EXPIRED			
			retry_count = RETRY_INIT;
#endif
		}
		TCPDELAY;
#endif
	}

	send_msg_active--;
}



/*
 *	CAC_SEND_MSG_PIIU()
 *
 * 
 */
static int
cac_send_msg_piiu(piiu)
register struct ioc_in_use 	*piiu;
{
  	unsigned 		cnt;
  	void			*pmsg;    
  	int			status;
	struct ioc_in_use 	*piiu_socket;

	cnt = piiu->send->stk;
	pmsg = (void *) piiu->send->buf;	

	while(TRUE){

		if(piiu->conn_up){
			/*
			 * use TCP if connection exists
			 */
			piiu_socket = piiu;
		}
		else{
			/* 
			 * use UDP   
			 *
			 * NOTE: this does not use a broadcast
			 * if the location of the channel is
			 * known from a previous connection.
			 *
			 * (piiu->sock_addr points to the
			 *	known address)
			 */
			if(!iiu[BROADCAST_IIU].conn_up)
				return ERROR;

			piiu_socket = &iiu[BROADCAST_IIU];
		}
		status = sendto(
				piiu_socket->sock_chan,
				pmsg,	
				cnt,
				0,
				&piiu->sock_addr,
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
ca_printf("sent zero\n");
			TCPDELAY;
		}
#ifdef UNIX
		else if(MYERRNO == EWOULDBLOCK){
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
#endif
		else{
			if(	MYERRNO != EPIPE && 
				MYERRNO != ECONNRESET &&
				MYERRNO != ETIMEDOUT){
				ca_printf(	
					"CAC: error on socket send() %d\n",
					MYERRNO);
			}
			close_ioc(piiu_socket);
			return OK;
		}


	}

      	/* reset send stack */
      	piiu->send->stk = 0;
        piiu->send_needed = FALSE;

	/*
	 * reset the delay to the next keepalive
	 */
    	if(piiu != &iiu[BROADCAST_IIU] && piiu->conn_up){
		piiu->next_retry = time(NULL) + CA_RETRY_PERIOD;
	}

	return OK;
}



/*
 *	RECV_MSG_SELECT()
 *
 * 	Asynch notification of incomming messages under UNIX
 *	1) Wait no longer than timeout 
 *	2) Return early if nothing outstanding
 * 
 */
#ifdef UNIX
void			
recv_msg_select(ptimeout)
struct timeval 	*ptimeout;
{
  	long				status;
  	register struct ioc_in_use 	*piiu;
  	struct timeval 			*ptmptimeout;

  	ptmptimeout = ptimeout;
  	while(TRUE){

    		for(piiu=iiu;piiu<&iiu[nxtiiu];piiu++)
      			if(piiu->conn_up){
        			FD_SET(piiu->sock_chan,&readch);
        			FD_SET(piiu->sock_chan,&excepch);
      			}

    		status = select(
				sizeof(fd_set)*NBBY,
				&readch,
				NULL,
				&excepch,
				ptmptimeout);

  		if(status<=0){  
			if(status == 0)
				return;
                                             
    			if(MYERRNO == EINTR){
				ca_printf("cac: select was interrupted\n");
				TCPDELAY;
				continue;
			}
    			else if(MYERRNO == EWOULDBLOCK){
				ca_printf("CAC: blocked at select ?\n");
				return;
    			}                                           
    			else{                                                  					char text[255];                                         
     				sprintf(
					text,
					"CAC: unexpected select fail: %d",
					MYERRNO); 
      				ca_signal(ECA_INTERNAL,text);                       			}                                                         		}                                                         

    		for(piiu=iiu;piiu<&iiu[nxtiiu];piiu++){
      			if(piiu->conn_up){
      				if(FD_ISSET(piiu->sock_chan,&readch) ||
					FD_ISSET(piiu->sock_chan,&excepch)){
          				recv_msg(piiu);
				}
			}
		}

  		/*
   		 * double check to make sure that nothing is left pending
  	 	 */
    		ptmptimeout = &notimeout;
  	}

}
#endif



/*
 *	RECV_MSG()
 *
 *
 */
static void 
recv_msg(piiu)
struct ioc_in_use	*piiu;
{

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
    		abort();
  	}  

        if(piiu->send_needed){
                cac_send_msg_piiu(piiu);
        }

  	return;
}


/*
 * TCP_RECV_MSG()
 *
 */
static void 
tcp_recv_msg(piiu)
struct ioc_in_use	*piiu;
{
  	long			byte_cnt;
  	int			status;
  	int			timeoutcnt;
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
    		/* try again on status of -1 and EWOULDBLOCK */
      		if(MYERRNO == EWOULDBLOCK){
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


	rcvb->stk += byte_cnt;

  	/* post message to the user */
	byte_cnt = rcvb->stk;
  	status = post_msg(
			rcvb->buf, 
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
    	if(piiu != &iiu[BROADCAST_IIU] && piiu->conn_up){
		piiu->next_retry = time(NULL) + CA_RETRY_PERIOD;
	}

 	return;
}



/*
 *	UDP_RECV_MSG()
 *
 */
static void 
udp_recv_msg(piiu)
struct ioc_in_use	*piiu;
{
  	int			status;
  	struct buffer		*rcvb = 	piiu->recv;
  	int			sock = 		piiu->sock_chan;
  	int			reply_size;
  	char			*ptr;
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

  		reply_size = sizeof(struct sockaddr_in);
    		status = recvfrom(	
				sock,
				&rcvb->buf[rcvb->stk],
				MAX_UDP,
				0,
				&pmsglog->addr, 
				&reply_size);
    		if(status < 0){
			/*
			 * op would block which is ok to ignore till ready
			 * later
			 */
      			if(MYERRNO == EWOULDBLOCK)
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
					&nchars);
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
				pmsglog+1, 
				&msgcount,
				&pmsglog->addr.sin_addr,
				piiu);
		if(status != OK || msgcount != 0){
			ca_printf(	"CAC: UDP alignment problem %d\n",
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
static void 
recv_task(piiu, moms_tid)
struct ioc_in_use	*piiu;
int			moms_tid;
{
  	int		status;

  	status = taskVarAdd(VXTHISTASKID, &ca_static); 
  	if(status == ERROR)                            
    		abort();                                     

 	ca_static = (struct ca_static *) 
			taskVarGet(moms_tid, &ca_static);

  	if(ca_static == (struct ca_static*)ERROR)
    		abort();

  	while(piiu->conn_up)
    		recv_msg(piiu);


	/*
	 * Exit recv task
	 * 
	 * NOTE on vxWorks I dont want the global channel access exit handler to
	 * run for this pod of tasks when the recv task exits so I delete the
	 * task variable here. The CA exit handler ignores tasks with out the
	 * ca task var defined.
	 */

  	status = taskVarDelete(VXTHISTASKID, &ca_static); 
  	if(status == ERROR)                            
    		abort();                                     

  	exit();
}
#endif


/*
 *
 *	VMS_RECV_MSG_AST()
 *
 *
 */
#ifdef VMS
static void 
vms_recv_msg_ast(piiu)
struct ioc_in_use	*piiu;
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
void
close_ioc(piiu)
struct ioc_in_use	*piiu;
{
  	register chid			chix;
  	struct connection_handler_args	args;
	int				status;

	if(piiu == &iiu[BROADCAST_IIU]){
		ca_signal(ECA_INTERNAL, "Unable to perform UDP broadcast\n");
	}

  	if(!piiu->conn_up)
		return;

  	/*
  	 * reset send stack- discard pending ops when the conn broke (assume
  	 * use as UDP buffer during disconn)
   	 */
  	piiu->send->stk = 0;
  	piiu->recv->stk = 0;
  	piiu->max_msg = MAX_UDP;
  	piiu->conn_up = FALSE;

	/*
	 * reset the delay to the next retry
	 */
	piiu->next_retry = CA_CURRENT_TIME;
	piiu->nconn_tries = 0;
	piiu->retry_delay = CA_RECAST_DELAY;

# 	ifdef UNIX
  	/* clear unused select bit */
  	FD_CLR(piiu->sock_chan, &readch);
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
    		chix->paddr = NULL;
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
  	if(piiu->chidlist.count){
    		ca_signal(
			ECA_DISCONN, 
			piiu->host_name_str);
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
repeater_installed()
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
        	abort();
	}

	status = setsockopt(	sock,
				SOL_SOCKET,
				SO_REUSEADDR,
				NULL,
				0);
	if(status<0){
		ca_printf(      "%s: set socket option failed\n",
				__FILE__);
	}

      	memset(&bd,0,sizeof bd);
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
			abort();

	status = socket_close(sock);
	if(status<0){
		SEVCHK(ECA_INTERNAL,NULL);
	}
		

	return installed;
}
