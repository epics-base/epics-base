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
/*	8/87	joh	Init Release					*/
/*	021291 	joh	Fixed vxWorks task name creation bug		*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC socket interface module				*/
/*	File:	atcs:[ca]iocinf.c					*/
/*	Environment: VMS. UNIX, VRTX					*/
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
/*									*/
/************************************************************************/
/*_end									*/


/*	Allocate storage for global variables in this module		*/
#define			CA_GLBLSOURCE
#ifdef VMS
#include		<iodef.h>
#include		<inetiodef.h>
#include		<stsdef.h>
#else
#endif

#include		<errno.h>
#include		<vxWorks.h>

#ifdef vxWorks
#include		<ioLib.h>
#include		<task_params.h>
#endif

#include		<types.h>
#include		<socket.h>
#include		<in.h>
#include		<tcp.h>
#include		<ioctl.h>
#include		<cadef.h>
#include		<net_convert.h>
#include		<iocmsg.h>
#include		<iocinf.h>

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


static struct timeval notimeout = {0,0};

void close_ioc();



/*
	LOCK should be on while in this routine
*/
				alloc_ioc(net_addr, net_proto, iocix)
struct in_addr			net_addr;
int				net_proto;
unsigned short			*iocix;
{
  int				i;
  int				status;

  /**********************************************************************/
  /* 	IOC allready allocated ?					*/
  /**********************************************************************/
  for(i=0;i<nxtiiu;i++)
    if(		(net_addr.s_addr == iiu[i].sock_addr.sin_addr.s_addr)
	&&	(net_proto == iiu[i].sock_proto)){

      if(!iiu[i].conn_up){
	/* an old connection is resumed */
        status = create_net_chan(&iiu[i]);
        if(status != ECA_NORMAL)
          return status;
        ca_signal(ECA_NEWCONN,host_from_addr(net_addr));
      }

      *iocix = i;
      return ECA_NORMAL;
    }

  /* 	is there an IOC In Use block available to allocate	*/
  if(nxtiiu>=MAXIIU)
    return ECA_MAXIOC;

  /* 	set network address block	*/
  iiu[nxtiiu].sock_addr.sin_addr = net_addr;
  iiu[nxtiiu].sock_proto = net_proto;
  status = create_net_chan(&iiu[nxtiiu]);
  if(status != ECA_NORMAL)
    return status;

  *iocix = nxtiiu++;

  return ECA_NORMAL;

}



/**********************************************************************/
/* 	allocate and initialize an IOC info block for unallocated IOC */
/**********************************************************************/
/*
	LOCK should be on while in this routine
*/
create_net_chan(piiu)
struct ioc_in_use		*piiu;
{
  int				status;
  int 				sock;
  int				true = TRUE;
  struct sockaddr_in		saddr;
  int				i;

  struct sockaddr_in 		*local_addr();

 
  /* 	set socket domain 	*/
  piiu->sock_addr.sin_family = AF_INET;

  /*	set the port 	*/
  piiu->sock_addr.sin_port = htons(CA_SERVER_PORT);

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
  	see TCP(4P) this seems to make unsollicited single 
	events much faster. I take care of queue up as load 
	increases.
      */
      status = setsockopt(	sock,
				IPPROTO_TCP,
				TCP_NODELAY,
				&true,
				sizeof true);
      if(status < 0){
        socket_close(sock);
        return ECA_ALLOCMEM;
      }

#ifdef KEEPALIVE
      /*
  	This should cause the connection to be checked periodically
	and an error to be returned if it is lost.

	In practice it is not much use since the conn is checked very
	infrequently.
      */
      status = setsockopt(	sock,
				SOL_SOCKET,
				SO_KEEPALIVE,
				&true,
				sizeof true);
      if(status < 0){
        socket_close(sock);
        return ECA_ALLOCMEM;
      }
#endif

#ifdef JUNKYARD
{
struct linger 	linger;
int		linger_size = sizeof linger;
      status = getsockopt(	sock,
				SOL_SOCKET,
				SO_LINGER,
				&linger,
				&linger_size);
      if(status < 0){
	abort();
      }
printf("linger was on:%d linger:%d\n", linger.l_onoff, linger.l_linger);
}
#endif

      /* set TCP buffer sizes only if BSD 4.3 sockets */
      /* temporarily turned off */
#     ifdef JUNKYARD  

        i = (MAX_MSG_SIZE+sizeof(int)) * 2;
        status = setsockopt(	sock,
				SOL_SOCKET,
				SO_SNDBUF,
				&i,
				sizeof(i));
        if(status < 0){
          socket_close(sock);
          return ECA_ALLOCMEM;
        }
        status = setsockopt(	sock,
				SOL_SOCKET,
				SO_RCVBUF,
				&i,
				sizeof(i));
        if(status < 0){
          socket_close(sock);
          return ECA_ALLOCMEM;
        }
#     endif



      /*	connect					*/
      status = connect(	
			sock,
			&piiu->sock_addr,
			sizeof(piiu->sock_addr));
      if(status < 0){
	printf("no conn errno %d\n", MYERRNO);
        socket_close(sock);
        return ECA_CONN;
      }

      piiu->max_msg = MAX_TCP;

      /* 
      place the broadcast addr/port in the stream so the
      ioc will know where this is coming from.
      */
      i = sizeof(saddr);
      status = getsockname(	iiu[BROADCAST_IIU].sock_chan, 
				&saddr,
				&i);
      if(status == ERROR){
	printf("alloc_ioc: cant get my name %d\n",MYERRNO);
	abort();
      }
      saddr.sin_addr.s_addr = 
	(local_addr(iiu[BROADCAST_IIU].sock_chan))->sin_addr.s_addr;
      status = send(	piiu->sock_chan,
			&saddr,
			sizeof(saddr),
			0);

      /*	Set non blocking IO for UNIX to prevent dead locks	*/
#     ifdef UNIX
        status = socket_ioctl(	piiu->sock_chan,
				FIONBIO,
				&true);
#     endif

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
	The following only needed on BSD 4.3 machines
      */
      status = setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&true,sizeof(true));
      if(status<0){
        printf("%d\n",MYERRNO);
        socket_close(sock);
        return ECA_CONN;
      }

      memset(&saddr,0,sizeof(saddr));
      saddr.sin_family = AF_INET;
      saddr.sin_addr.s_addr = htonl(INADDR_ANY); /* let slib pick lcl addr */
      saddr.sin_port = htons(0);	

      status = bind(sock, &saddr, sizeof(saddr));
      if(status<0){
        printf("%d\n",MYERRNO);
	ca_signal(ECA_INTERNAL,"bind failed");
      }

      piiu->max_msg = MAX_UDP - sizeof(piiu->send->stk);

      break;

    default:
      ca_signal(ECA_INTERNAL,"alloc_ioc: ukn protocol\n");
  }
  /* 	setup send_msg(), recv_msg() buffers	*/
  if(!piiu->send)
    if(! (piiu->send = (struct buffer *) malloc(sizeof(struct buffer))) ){
      socket_close(sock);
      return ECA_ALLOCMEM;
    }
  piiu->send->stk = 0;

  if(!piiu->recv)
    if(! (piiu->recv = (struct buffer *) malloc(sizeof(struct buffer))) ){
      socket_close(sock);
      return ECA_ALLOCMEM;
    }

  piiu->recv->stk = 0;
  piiu->conn_up = TRUE;
  if(fd_register_func)
	(*fd_register_func)(fd_register_arg, sock, TRUE);


  /*	Set up recv thread for VMS	*/
# ifdef VMS
  {
    void   recv_msg_ast();
    status = sys$qio(	NULL,
			sock,
			IO$_RECEIVE,
			&piiu->iosb,
			recv_msg_ast,
			piiu,
			piiu->recv->buf,
			sizeof(piiu->recv->buf),
			NULL,
			&piiu->recvfrom,
			sizeof(piiu->recvfrom),
			NULL);
    if(status != SS$_NORMAL){
      lib$signal(status);
      exit();
    }
  }
# endif
# ifdef vxWorks
  {  
      void	recv_task();
      int 	pri;
      char	name[15];

      status == taskPriorityGet(VXTASKIDSELF, &pri);
      if(status<0)
	ca_signal(ECA_INTERNAL,NULL);

      strcpy(name,"RD ");                   
      strncat(name, taskName(VXTHISTASKID), sizeof(name)-strlen(name)-1); 

      status = taskSpawn(	name,
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
# endif

/*
	testing testing
*/
#ifdef ZEBRA /* vw does not have getsockopt */
{
int timeo;
int timeolen = sizeof(timeo);

      status = getsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&timeo,&timeolen);
      if(status<0){
        printf("%d\n",MYERRNO);
      }
if(timeolen != sizeof(timeo))
printf("bomb\n");

printf("send %d\n",timeo);
      status = getsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeo,&timeolen);
      if(status<0){
        printf("%d\n",MYERRNO);
      }
if(timeolen != sizeof(timeo))
printf("bomb\n");

printf("recv %d\n",timeo);
}
#endif



  return ECA_NORMAL;
}



/*
 *	NOTIFY_CA_REPEATER
 *	tell the cast repeater that another client has come on line 
 *
 *	NOTES:
 *	1)	local communication only (no LAN traffic)
 *
 */
notify_ca_repeater()
{
	struct sockaddr_in	saddr;
	int			status;
  	struct sockaddr_in 	*local_addr();

	if(!iiu[BROADCAST_IIU].conn_up)
		return;

     	saddr = *( local_addr(iiu[BROADCAST_IIU].sock_chan) );
      	saddr.sin_port = htons(CA_CLIENT_PORT);	
      	status = sendto(
		iiu[BROADCAST_IIU].sock_chan,
        	NULL,
        	0, /* zero length message */
        	0,
       		&saddr, 
		sizeof saddr);
      	if(status < 0)
		abort();
}



/*
 * SEND_MSG
 * 
 * NOTE: Wallangong select cant be called in an AST with a timeout since they
 * use timer ASTs to implement thier timeout.
 * 
 * NOTE: Wallangong select does not return early if IO is present prior to the
 * timeout expiring.
 * 
 * LOCK should be on while in this routine
 */
void			
send_msg()
{
  unsigned 			cnt;
  void				*pmsg;    
  int				status;
  register struct ioc_in_use 	*piiu;

  /**********************************************************************/
  /*	Note: this routine must not be called at AST level		*/
  /**********************************************************************/
  if(!ca_static->ca_repeater_contacted)
	notify_ca_repeater();


  /* don't allow UDP recieve messages to que up under UNIX */
# ifdef UNIX
  {
	void recv_msg_select();
	/* test for recieve allready in progress and NOOP if so */
	if(!post_msg_active)
        	recv_msg_select(&notimeout);
  }
# endif

  for(piiu=iiu;piiu<&iiu[nxtiiu];piiu++)
    if(piiu->send->stk){


      cnt = piiu->send->stk + sizeof(piiu->send->stk);
      piiu->send->stk = htonl(cnt);	/* convert for 68000	*/
      pmsg = piiu->send;		/* byte cnt then buf 	*/

      while(TRUE){
	if(piiu->conn_up){
          status = sendto(	piiu->sock_chan,
				pmsg,	
			  	cnt,
				0,
				&piiu->sock_addr,
				sizeof(piiu->sock_addr));
        }
        else{
	  /* send a directed UDP message (for search retries) */
          status = sendto(	iiu[BROADCAST_IIU].sock_chan,
				pmsg,	
			  	cnt,
				0,
				&piiu->sock_addr,
				sizeof(piiu->sock_addr));
	}
        if(status == cnt)
	  break;

	if(status>=0){
	  if(status>cnt)
	    ca_signal(ECA_INTERNAL,"more sent than requested");
	  cnt = cnt-status;
	  pmsg = (void *) (status+(char *)pmsg);
	}
        else if(MYERRNO == EWOULDBLOCK){
	}
        else{
	  if(MYERRNO != EPIPE && MYERRNO != ECONNRESET)
            printf("send_msg(): error on socket send() %d\n",MYERRNO);
	  close_ioc(piiu);
	  break;
	}

  	/*	
	Ensure we do not accumulate extra recv messages	(for TCP)	
	*/
#	ifdef UNIX
        {
          void recv_msg_select();
  	  recv_msg_select(&notimeout);
	}
#	endif
	TCPDELAY;
      }

      /* reset send stack */
      piiu->send->stk = 0;
    }

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
  	void				recv_msg();

  	ptmptimeout = ptimeout;
  	while(TRUE){

    		for(piiu=iiu;piiu<&iiu[nxtiiu];piiu++)
      			if(piiu->conn_up){
        			FD_SET(piiu->sock_chan,&readch);
      			}

    		status = select(
				sizeof(fd_set)*NBBY,
				&readch,
				NULL,
				NULL,
				ptmptimeout);

  		if(status<=0){  
			if(status == 0)
				return;
                                             
    			if(MYERRNO == EINTR)
				return;      
    			else if(MYERRNO == EWOULDBLOCK){
				printf("CA: blocked at select ?\n");
				return;
    			}                                           
    			else{                                                  					char text[255];                                         
     				sprintf(
					text,
					"unexpected select fail: %d",
					MYERRNO); 
      				ca_signal(ECA_INTERNAL,text);                       			}                                                         		}                                                         

    		for(piiu=iiu;piiu<&iiu[nxtiiu];piiu++)
      			if(piiu->conn_up)
      				if(FD_ISSET(piiu->sock_chan,&readch) )
          				recv_msg(piiu);

  		/*
   		 * double check to make sure that nothing is left pending
  	 	 */
    		ptmptimeout = &notimeout;
  	}

}
#endif



void
recv_msg(piiu)
struct ioc_in_use	*piiu;
{
  void			tcp_recv_msg();
  void			udp_recv_msg();

  switch(piiu->sock_proto){
  case	IPPROTO_TCP:

    /* #### remove this statement after debug is complete #### */
    if(!piiu->conn_up)
      ca_signal(ECA_INTERNAL,"TCP burp at conn close\n");

    tcp_recv_msg(piiu);
    flow_control(piiu);

    break;
	
  case 	IPPROTO_UDP:
    udp_recv_msg(piiu);
    break;

  default:
    printf("send_msg: ukn protocol\n");
    abort();
  }  

  return;
}



void
tcp_recv_msg(piiu)
struct ioc_in_use	*piiu;
{
  	unsigned long		byte_cnt;
  	unsigned long		byte_sum;
  	int			status;
  	int			timeoutcnt;
  	struct buffer		*rcvb = 	piiu->recv;
  	int			sock = 		piiu->sock_chan;



 	status = recv(	sock,
			&rcvb->stk,
			sizeof(rcvb->stk),
			0);
  	if(status != sizeof(rcvb->stk)){

    		if( status > 0)
      			ca_signal(	
				ECA_INTERNAL,
				"partial recv on request of only 4 bytes\n");
		else if(status == 0){
			printf("CA: recv of zero length?\n");
			LOCK;
			close_ioc(piiu);
			UNLOCK;
			return;
		}
    		else{
    			/* try again on status of 0 or -1 and EWOULDBLOCK */
      			if(MYERRNO == EWOULDBLOCK)
				return;

       	 		if(MYERRNO != EPIPE && MYERRNO != ECONNRESET)
	  			printf(	"unexpected recv error 1 = %d %d\n",
					MYERRNO, 
					status);

			LOCK;
			close_ioc(piiu);
			UNLOCK;
			return;
    		}
  	}


  	/* switch from 68000 to VAX byte order */
  	byte_cnt = (unsigned long) ntohl(rcvb->stk) - sizeof(rcvb->stk);
  	if(byte_cnt>MAX_MSG_SIZE){
    		printf(	"recv_msg(): message overflow %u\n",
			byte_cnt-MAX_MSG_SIZE);
		LOCK;
		close_ioc(piiu);
		UNLOCK;
    		return;
 	}


  	timeoutcnt = byte_cnt + 3000;
  	byte_sum = 0;
  	while(TRUE){

#   		ifdef DEBUG
      		if(byte_sum)
        		printf(	"recv_msg(): Warning- %d leftover bytes \n",
			byte_cnt-byte_sum); 
#   		endif

   	 	status = recv(	sock,
			&rcvb->buf[byte_sum],
		  	byte_cnt - byte_sum,
			0);
    		if(status < 0){
      			if(MYERRNO != EWOULDBLOCK){
        			if(MYERRNO != EPIPE && MYERRNO != ECONNRESET)
	  				printf("recv error 2 = %d\n",MYERRNO);

				LOCK;
				close_ioc(piiu);
				UNLOCK;
				return;
      			}

      			if(--timeoutcnt < 0){
				printf("recv_msg(): message bdy wait tmo\n");
				LOCK;
				close_ioc(piiu);
				UNLOCK;
				abort();
      			}
    		}
    		else{
      			byte_sum += status;
      			if(byte_sum >= byte_cnt)
			break;
    		}

    		/* wait for TCP/IP to complete the message transfer */
    		TCPDELAY;
  	}

  	/* post message to the user */
  	post_msg(rcvb->buf, byte_cnt, piiu->sock_addr.sin_addr, piiu);

 	return;
}



void
udp_recv_msg(piiu)
struct ioc_in_use	*piiu;
{
  unsigned long		byte_cnt;
  int			status;
  struct buffer		*rcvb = 	piiu->recv;
  int			sock = 		piiu->sock_chan;
  struct sockaddr_in	reply;
  int			reply_size = 	sizeof(reply);
  char			*ptr;
  unsigned		nchars;


  rcvb->stk =0;	
  do{

    status = recvfrom(	
				sock,
				&rcvb->buf[rcvb->stk],
				MAX_UDP,
				0,
				&reply, 
				&reply_size);
    if(status < sizeof(int)){
      /* op would block which is ok to ignore till ready later */
      if(MYERRNO == EWOULDBLOCK)
        break;
      ca_signal(ECA_INTERNAL,"unexpected udp recv error");
    }

    /* switch from 68000 to VAX byte order */
    byte_cnt = (unsigned long) ntohl(*(int *)&rcvb->buf[rcvb->stk]);
#   ifdef DEBUG
      printf("recieved a udp reply of %d bytes\n",byte_cnt);
#   endif
    if(byte_cnt != status){
      printf("recv_cast(): corrupt UDP recv %d\n",MYERRNO);
      printf("recv_cast(): header %d actual %d\n",byte_cnt,status);
      return;
    }
    rcvb->stk += byte_cnt;

    *(struct in_addr *) &rcvb->buf[rcvb->stk] = reply.sin_addr;
    rcvb->stk += sizeof(reply.sin_addr);

    if(rcvb->stk + MAX_UDP > MAX_MSG_SIZE)
      break;
	
    /*
    More comming ?
    */
    status = socket_ioctl(	sock,
				FIONREAD,
				&nchars);
    if(status<0)
      ca_signal(ECA_INTERNAL,"unexpected udp ioctl err\n");

  }while(nchars);

  for(	ptr = rcvb->buf;
	ptr < &rcvb->buf[rcvb->stk]; 
	ptr += byte_cnt + sizeof(reply.sin_addr)){ 
  byte_cnt = (unsigned long) ntohl(*(int *)ptr);

  /* post message to the user */
  post_msg(	ptr + sizeof(int), 
		byte_cnt - sizeof(int),
		*(struct in_addr *)(ptr+byte_cnt),
		piiu);
  }
	  
  return; 
}




#ifdef vxWorks
void
recv_task(piiu, moms_tid)
struct ioc_in_use	*piiu;
int			moms_tid;
{
  int			status;

  status = taskVarAdd(VXTHISTASKID, &ca_static); 
  if(status == ERROR)                            
    abort();                                     

  ca_static = (struct ca_static *) taskVarGet(moms_tid, &ca_static);
  if(ca_static == (struct ca_static*)ERROR)
    abort();

  while(piiu->conn_up)
    recv_msg(piiu);

  /*
	Exit recv task

	NOTE on vxWorks I dont want the global channel access
	exit handler to run for this pod of tasks when the recv
	task exits so I delete the  task variable here.
	The CA exit handler ignores tasks with out the ca task
	var defined.
  */

  status = taskVarDelete(VXTHISTASKID, &ca_static); 
  if(status == ERROR)                            
    abort();                                     

  exit();
}
#endif


/*
 *
 *	RECV_MSG_AST()
 *
 *
 */
#ifdef VMS
void
recv_msg_ast(piiu)
struct ioc_in_use	*piiu;
{
  	short			io_status = piiu->iosb.status;
  	int			io_count = piiu->iosb.count;
  	struct sockaddr_in	*paddr;
  	char			*pbuf;
  	unsigned int		*pcount= (unsigned int *) piiu->recv->buf;
  	int			bufsize;
  	unsigned long		byte_cnt;

  	if(io_status != SS$_NORMAL){
    		close_ioc(piiu);
    		if(io_status != SS$_CANCEL)
      			lib$signal(io_status);
    		return;
  	}
	/*
	 * NOTE: The following is a bug fix since WIN has returned the
	 * address structure missaligned by 16 bits
	 * 
	 * the fix is reliable since this structure happens to be padded with
	 * zeros at the end by more than 16 bits
	 */
    	if(piiu->sock_proto == IPPROTO_TCP)
      		paddr = (struct sockaddr_in *) &piiu->sock_addr;
    	else
#ifdef WINTCP
      		paddr = (struct sockaddr_in *) 
			((char *)&piiu->recvfrom+sizeof(short));
#else
      		paddr = (struct sockaddr_in *) 
			(char *)&piiu->recvfrom;
#endif

    	piiu->recv->stk += io_count;
    	io_count = piiu->recv->stk;

    	while(TRUE){
      		pbuf = (char *) (pcount+1);

      		/* switch from 68000 to VAX byte order */
     		 byte_cnt = (unsigned long) ntohl(*pcount);
      		if(byte_cnt>MAX_MSG_SIZE || byte_cnt<sizeof(*pcount)){
			if(byte_cnt)
        			printf(	"CA: msg over/underflow %u\n", 
					byte_cnt);
        		close_ioc(piiu);
			return;
      		}

      		if(io_count == byte_cnt){
        		post_msg(
				pbuf, 
				byte_cnt-sizeof(*pcount), 
				paddr->sin_addr, 
				piiu);
        		break;
      		}
      		else if(io_count > byte_cnt)
        		post_msg(
				pbuf, 
				byte_cnt-sizeof(*pcount), 
				paddr->sin_addr, 
				piiu);
      		else{
        		if(pcount != piiu->recv->buf){
          			memcpy(piiu->recv->buf, pcount, io_count);  
          			piiu->recv->stk = io_count;
        		}

        		bufsize = sizeof(piiu->recv->buf) - piiu->recv->stk;
        		io_status = sys$qio(
				NULL,
				piiu->sock_chan,
				IO$_RECEIVE,
				&piiu->iosb,
				recv_msg_ast,
				piiu,
				&piiu->recv->buf[piiu->recv->stk],
				bufsize,
				NULL,
				&piiu->recvfrom,
				sizeof(piiu->recvfrom),
				NULL);
        		if(io_status != SS$_NORMAL){
	  			if(io_status == SS$_IVCHAN)
	    				printf("CA: Unable to requeue AST?\n");
	  			else
            				lib$signal(io_status);
			}
			return;
     		 }
      		io_count -= byte_cnt;
      		pcount = (int *) (byte_cnt + (char *)pcount);

    	}

  	if(piiu->sock_proto == IPPROTO_TCP)  
    		flow_control(piiu);

  	piiu->recv->stk = 0;
  	io_status = sys$qio(
			NULL,
			piiu->sock_chan,
			IO$_RECEIVE,
			&piiu->iosb,
			recv_msg_ast,
			piiu,
			piiu->recv->buf,
			sizeof(piiu->recv->buf),
			NULL,
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
 *	CLOSE_IOC
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
  register chid				chix;
  int					status;
  register evid 			monix;
  struct connection_handler_args	args;

  if(!piiu->conn_up)
	return;

  /*
   * reset send stack- discard pending ops when the conn broke (assume
   * use as UDP buffer during disconn)
   */
  piiu->send->stk = 0;
  piiu->max_msg = MAX_UDP;
  piiu->conn_up = FALSE;

# ifdef UNIX
  /* clear unused select bit */
  FD_CLR(piiu->sock_chan, &readch);
# endif 

  chix = (chid) &piiu->chidlist.node.next;
  while(chix = (chid) chix->node.next){
    chix->type = TYPENOTCONN;
    chix->count = 0;
    chix->state = cs_prev_conn;
    chix->paddr = NULL;
    if(chix->connection_func){
	args.chid = chix;
	args.op = CA_OP_CONN_DOWN;
      	(*chix->connection_func)(args);
    }
  }

  if(fd_register_func)
	(*fd_register_func)(fd_register_arg, piiu->sock_chan, FALSE);

  close(piiu->sock_chan);
  piiu->sock_chan = -1;
  if(piiu->chidlist.count)
    ca_signal(ECA_DISCONN, host_from_addr(piiu->sock_addr.sin_addr));

}



/*
 *
 *	Test for the repeater allready installed
 *
 *	NOTE: potential race condition here can result
 *	in two copies of the repeater being spawned
 *	however the repeater detectes this prints a message
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
      	if(sock == ERROR)
        	abort();

      	memset(&bd,0,sizeof bd);
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = htonl(INADDR_ANY);	
     	bd.sin_port = htons(CA_CLIENT_PORT);	
      	status = bind(sock, &bd, sizeof bd);
     	if(status<0)
		if(MYERRNO == EADDRINUSE)
			installed = TRUE;
		else
			abort();


	close(sock);

	return installed;
}
