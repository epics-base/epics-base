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
#define			GLBLSOURCE
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
#include		<taskParams.h>
#endif

#include		<types.h>
#include		<socket.h>
#include		<in.h>
#include		<ioctl.h>
#include		<cadef.h>
#include		<net_convert.h>
#include		<iocmsg.h>
#include		<iocinf.h>

/* For older versions of berkley UNIX types.h and vxWorks types.h 	*/
/*	64 times sizeof(fd_mask) is the maximum channels on a vax JH 	*/
/*	Yuk - can a maximum channel be determined at run time ?		*/
/*
typedef long	fd_mask;
typedef	struct fd_set {
	fd_mask	fds_bits[64];
} fd_set;
*/

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

/* my version is faster since it is in line 			*/
#define FD_ZERO(p)\
{\
  register int i;\
  for(i=0;i<NELEMENTS((p)->fds_bits);i++)\
    (p)->fds_bits[i]=0;\
}


/* 50 mS delay for TCP to finish transmitting 				*/
/* select wakes you if message is only partly here 			*/
/* so this wait free's up the processor until it completely arrives 	*/
#define DELAYVAL	0.050		/* 50 mS	*/

#ifdef	VMS
# define SYSFREQ		10000000	/* 10 MHz	*/
# define TCPDELAY\
{float delay = DELAYVAL; static int ef=NULL;\
 int status; int systim[2]={-SYSFREQ*DELAYVAL,~0};\
  if(!ef)ef= lib$get_ef(&ef);\
  status = sys$setimr(ef,systim,NULL,MYTIMERID,NULL);\
  if(~status&STS$M_SUCCESS)lib$signal(status);\
  status = sys$waitfr(ef);\
  if(~status&STS$M_SUCCESS)lib$signal(status);\
};
#endif

#ifdef vxWorks
/*
###############
insert sysClkRateGet() here ??? -slows it down but works on all systems ??
###############
*/
# define SYSFREQ		60		/* 60 Hz	*/
# define TCPDELAY taskDelay((unsigned int)DELAYVAL*SYSFREQ);	
#endif

#ifdef UNIX
#  define SYSFREQ		1000000		/* 1 MHz	*/
/*                                                                     
#  define TCPDELAY if(usleep((unsigned int)DELAYVAL*SYSFREQ))abort();  
*/                                                                     
#  define TCPDELAY {if(select(0,NULL,NULL,NULL,&tcpdelayval)<0)abort();}
static struct timeval tcpdelayval = {0,(unsigned int)DELAYVAL*SYSFREQ};
#endif

static struct timeval notimeout = {0,0};


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
  int 				sock;
  int				true = TRUE;
  struct sockaddr_in		src;
  struct sockaddr_in 		*local_addr();
  struct ioc_in_use		*piiu;


  /**********************************************************************/
  /* 	IOC allready allocated ?					*/
  /**********************************************************************/
  for(i=0;i<nxtiiu;i++)
    if(	(net_addr.s_addr == iiu[i].sock_addr.sin_addr.s_addr)
	&&
	(net_proto == iiu[i].sock_proto)){
      *iocix = i;
      return ECA_NORMAL;
    }

  /**********************************************************************/
  /* 	allocate and initialize an IOC info block for unallocated IOC	*/
  /**********************************************************************/

  /* 	is there an IOC In Use block available to allocate		*/
  if(nxtiiu>=MAXIIU)
    return ECA_MAXIOC;

  piiu = &iiu[nxtiiu];

  /* 	set network address block					*/
  piiu->sock_addr.sin_addr = net_addr;
  
  /* 	set socket domain 						*/
  piiu->sock_addr.sin_family = AF_INET;

  piiu->sock_proto = net_proto;


  switch(net_proto)
  {
    case	IPPROTO_TCP:
      /*			set the port 			*/
      piiu->sock_addr.sin_port = htons(SERVER_NUM);

      /* 	allocate a socket					*/
      sock = socket(	AF_INET,	/* domain	*/
			SOCK_STREAM,	/* type		*/
			0);		/* deflt proto	*/
      if(sock == ERROR)
        return ECA_SOCK;

      piiu->sock_chan = sock;

      /* set TCP buffer sizes only if BSD 4.3 sockets */
      /* temporarily turned off */
#     ifdef ZEBRA  

        i = (MAX_MSG_SIZE+sizeof(int)) * 2;
        status = setsockopt(	sock,
				SOL_SOCKET,
				SO_SNDBUF,
				&i,
				sizeof(i));
        if(status == -1){
          socket_close(sock);
          return ECA_ALLOCMEM;
        }
        status = setsockopt(	sock,
				SOL_SOCKET,
				SO_RCVBUF,
				&i,
				sizeof(i));
        if(status == -1){
          socket_close(sock);
          return ECA_ALLOCMEM;
        }
#     endif

      piiu->max_msg = MAX_TCP;


      /*	connect					*/
      status = connect(	
			sock,
			&iiu[nxtiiu].sock_addr,
			sizeof(iiu[nxtiiu].sock_addr));
      if(status == -1){
        socket_close(sock);
        return ECA_CONN;
      }

      /* 
      place the broadcast addr/port in the stream so the
      ioc will know where this is coming from.
      */
      i = sizeof(src);
      status = getsockname(	iiu[BROADCAST_IIU].sock_chan, 
				&src,
				&i);
      if(status == ERROR){
	printf("alloc_ioc: cant get my name %d\n",MYERRNO);
	abort();
      }
      src.sin_addr.s_addr = 
	(local_addr(iiu[BROADCAST_IIU].sock_chan))->sin_addr.s_addr;
      status = send(	piiu->sock_chan,
			&src,
			sizeof(src),
			0);

      break;
    case	IPPROTO_UDP:
      /*			set the port 			*/
      piiu->sock_addr.sin_port = htons(SERVER_NUM);

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


      memset(&src,0,sizeof(src));
      src.sin_family = AF_INET;
      src.sin_addr.s_addr = INADDR_ANY;		/* let TCP pick lcl addr */
      src.sin_port = 0;				/* let TCP pick lcl port */

      status = bind(sock, &src, sizeof(src));
      if(status<0){
        printf("%d\n",MYERRNO);
	SEVCHK(ECA_INTERNAL,"bind failed");
      }

      piiu->max_msg = MAX_UDP - sizeof(iiu[nxtiiu].send->stk);

      break;
    default:
      printf("alloc_ioc: ukn protocol\n");
      abort();
  }

  /*	Set non blocking IO for UNIX to prevent dead locks	*/
# ifdef UNIX
    status = socket_ioctl(	piiu->sock_chan,
				FIONBIO,
				&true);
# endif

  /* 	setup send_msg(), recv_msg() buffers	*/
  if(! (piiu->send = (struct buffer *) malloc(sizeof(struct buffer))) ){
    socket_close(sock);
    return ECA_ALLOCMEM;
  }
  piiu->send->stk = 0;
  if(! (piiu->recv = (struct buffer *) malloc(sizeof(struct buffer))) ){
    socket_close(sock);
    return ECA_ALLOCMEM;
  }
  piiu->recv->stk = 0;

  /*	Set up recv thread for VMS	*/
# ifdef VMS
  {
    void   recv_msg_ast();
    status = sys$qio(	NULL,
			sock,
			IO$_RECEIVE,
			&piiu->iosb,
			recv_msg_ast,
			nxtiiu,
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
# else
#   ifdef vxWorks
    {  
      void	recv_task();
      int 	pri;
      char	name[15];

      status == taskPriorityGet(VXTASKIDSELF, &pri);
      if(status<0)
	SEVCHK(ECA_INTERNAL,NULL);

      name[0]=NULL;                                          
      strncat(name,"RD ", sizeof(name)-1);                   
      strncat(name, taskName(VXTHISTASKID), sizeof(name)-1); 

      status = taskSpawn(	name,
				pri-1,
				VX_FP_TASK,
				4096,
				recv_task,
				(unsigned) nxtiiu,
				taskIdSelf());
      if(status<0)
	SEVCHK(ECA_INTERNAL,NULL);

      iiu[nxtiiu].recv_tid = status;

    }
#   endif
# endif

  *iocix = nxtiiu++;

  return ECA_NORMAL;
}



/*
	NOTE: Wallangong select cant be called in an AST with a timeout
	since they use timer ASTs to implement thier timeout.

	NOTE: Wallangong select does not return early if IO is present prior
	to the timeout expiring.

	LOCK should be on while in this routine
*/

void			
send_msg()
{
  unsigned 		cnt;
  void			*pmsg;    
  unsigned 		iocix;
  int			status;

  /**********************************************************************/
  /*	Note: this routine must not be called at AST level		*/
  /**********************************************************************/
  for(iocix=0;iocix<nxtiiu;iocix++)
    if(iiu[iocix].send->stk){

      /* don't allow UDP recieve messages to que up under UNIX */
#     ifdef UNIX
      {
        void recv_msg_select();
	/* test for recieve allready in progress and NOOP if so */
	if(!post_msg_active)
          recv_msg_select(&notimeout);
      }
#     endif

      cnt = iiu[iocix].send->stk + sizeof(iiu[iocix].send->stk);
      iiu[iocix].send->stk = htonl(cnt);	/* convert for 68000	*/
      pmsg = iiu[iocix].send;			/* byte cnt then buf 	*/

      while(TRUE){
        status = sendto(	iiu[iocix].sock_chan,
				pmsg,	
			  	cnt,
				0,
				&iiu[iocix].sock_addr,
				sizeof(iiu[iocix].sock_addr));
        if(status == cnt)
	  break;

	if(status>=0){
	  if(status>cnt)
	    SEVCHK(ECA_INTERNAL,"more sent than requested");
	  cnt = cnt-status;
	  pmsg = (void *) (status+(char *)pmsg);
	}
        else if(MYERRNO == EWOULDBLOCK){
	}
	else{
	  if(MYERRNO != EPIPE){
            printf("send_msg(): unexpected error on socket send() %d\n",MYERRNO);
	  }
	  SEVCHK(ECA_DISCONN, host_from_addr(iiu[iocix].sock_addr.sin_addr))
	  mark_chids_disconnected(iocix);
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
      iiu[iocix].send->stk = 0;
    }

}



/*
	Recieve incomming messages
	1) Wait no longer than timeout
	2) Return early if nothing outstanding			

*/
#ifdef UNIX
void			
recv_msg_select(ptimeout)
struct timeval 		*ptimeout;
{
  unsigned		iocix;
  long			status;
  void			recv_msg();

  for(iocix=0; iocix<nxtiiu; iocix++)
    FD_SET(iiu[iocix].sock_chan, &readch);

  status = select(	sizeof(fd_set)*NBBY,
			&readch,
			&writech,
			&execch,
			ptimeout);

  while(status > 0){

    for(iocix=0; iocix<nxtiiu; iocix++){
      if(!FD_ISSET(iiu[iocix].sock_chan,&readch))
        continue;
      recv_msg(iocix);
    }

    /* make sure that nothing is left pending */
    for(iocix=0;iocix<nxtiiu;iocix++)
      FD_SET(iiu[iocix].sock_chan,&readch);

    status = select(	sizeof(fd_set)*NBBY,
			&readch,
			NULL,
			NULL,
			&notimeout);
  }

  if(status<0){                                               
    if(MYERRNO == EINTR)                                      
      return;                                                 
    else{                                                     
      char text[255];                                         
      sprintf(text,"Error Returned From Select: %d",MYERRNO); 
      SEVCHK(ECA_INTERNAL,text);                              
    }                                                         
  }                                                           

  return;
}
#endif



void
recv_msg(iocix)
unsigned		iocix;
{
  struct ioc_in_use	*piiu = &iiu[iocix];
  void			tcp_recv_msg();
  void			udp_recv_msg();

  switch(piiu->sock_proto){
  case	IPPROTO_TCP:
    tcp_recv_msg(iocix);
    flow_control(piiu);
    break;
	
  case 	IPPROTO_UDP:
    udp_recv_msg(iocix);
    break;

  default:
    printf("send_msg: ukn protocol\n");
    abort();
  }  

  return;
}



void
tcp_recv_msg(iocix)
unsigned		iocix;
{
  struct ioc_in_use	*piiu = &iiu[iocix];
  unsigned long		byte_cnt;
  unsigned long		byte_sum;
  int			status;
  int			timeoutcnt;
  struct buffer		*rcvb = 	piiu->recv;
  int			sock = 		piiu->sock_chan;



  while(TRUE){
    status = recv(	sock,
			&rcvb->stk,
			sizeof(rcvb->stk),
			0);
    if(status == sizeof(rcvb->stk))
      break;
    if( status > 0)
      SEVCHK(ECA_INTERNAL,"partial recv on request of only 4 bytes");

    if( status < 0){
      if(MYERRNO != EWOULDBLOCK){
        if(MYERRNO != EPIPE && MYERRNO != ECONNRESET)
	  printf("unexpected recv error 1 = %d\n",MYERRNO);

	SEVCHK(ECA_DISCONN, host_from_addr(piiu->sock_addr.sin_addr));
	mark_chids_disconnected(iocix);
	return;
      }
    }

    /* try again on status of 0 or -1 and EWOULDBLOCK */
    TCPDELAY;
  }


  /* switch from 68000 to VAX byte order */
  byte_cnt = (unsigned long) ntohl(rcvb->stk) - sizeof(rcvb->stk);
  if(byte_cnt>MAX_MSG_SIZE){
    printf("recv_msg(): message overflow %u\n",byte_cnt-MAX_MSG_SIZE);
    return;
  }


  timeoutcnt = byte_cnt + 3000;
  for(byte_sum = 0; byte_sum < byte_cnt; byte_sum += status){

#   ifdef DEBUG
      if(byte_sum)
        printf(	"recv_msg(): Warning- reading %d leftover bytes \n",
		byte_cnt-byte_sum); 
#   endif

    status = recv(	sock,
			&rcvb->buf[byte_sum],
		  	byte_cnt - byte_sum,
			0);
    if(status < 0){
      if(MYERRNO != EWOULDBLOCK){
        if(MYERRNO != EPIPE && MYERRNO != ECONNRESET)
	  printf("unexpected recv error 2 = %d\n",MYERRNO);

	SEVCHK(ECA_DISCONN, host_from_addr(piiu->sock_addr.sin_addr));
	mark_chids_disconnected(iocix);
	return;
      }

      status = 0;
      if(--timeoutcnt < 0){
	printf("recv_msg(): TCP message bdy wait timed out\n");
	abort();
      }
    }

    /* wait for TCP/IP to complete the message transfer */
    TCPDELAY;
  }

  /* post message to the user */
  post_msg(rcvb->buf, byte_cnt, piiu->sock_addr.sin_addr, piiu);

  return;
}



void
udp_recv_msg(iocix)
unsigned		iocix;
{
  struct ioc_in_use	*piiu = 	&iiu[iocix];
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
      SEVCHK(ECA_INTERNAL,"unexpected udp recv error");
    }

    /* switch from 68000 to VAX byte order */
    byte_cnt = (unsigned long) ntohl(*(int *)&rcvb->buf[rcvb->stk]);
#   ifdef DEBUG
      printf("recieved a udp reply of %d bytes\n",byte_cnt);
#   endif
    if(byte_cnt != status){
      printf("recv_cast(): corrupt broadcast reply %d\n",MYERRNO);
      printf("recv_cast(): header %d actual %d\n",byte_cnt,status);
      break;
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
      SEVCHK(ECA_INTERNAL,"unexpected udp ioctl err\n");

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
recv_task(iocix,moms_tid)
unsigned		iocix;
int			moms_tid;
{
  int			status;

  status = taskVarAdd(VXTHISTASKID, &ca_static); 
  if(status == ERROR)                            
    abort();                                     

  ca_static = (struct ca_static *) taskVarGet(moms_tid, &ca_static);
  if(ca_static == (struct ca_static*)ERROR)
    abort();

  while(TRUE)
    recv_msg(iocix);
}
#endif

#ifdef VMS
void
recv_msg_ast(iocix)
int iocix;
{
  struct ioc_in_use	*piiu = 	&iiu[iocix];
  short			io_status =	piiu->iosb.status;
  int			io_count =	piiu->iosb.count;
  struct sockaddr_in	*paddr;
  char			*pbuf;
  unsigned int		*pcount=	(unsigned int *) piiu->recv->buf;
  int			bufsize;

  unsigned long		byte_cnt;

  if(io_status != SS$_NORMAL){
    if(io_status == SS$_CANCEL)
      return;
    lib$signal(io_status);
  }
  else{
    /*
    NOTE:	The following is a bug fix since WIN has returned
		the address structure missaligned by 16 bits

		the fix is reliable since this structure happens
		to be padded with zeros at the end by more than 16 bits
    */
    if(piiu->sock_proto == IPPROTO_TCP)
      paddr = (struct sockaddr_in *) &piiu->sock_addr;
    else
#ifdef WINTCP
      paddr = (struct sockaddr_in *) ((char *)&piiu->recvfrom+sizeof(short));
#else
      paddr = (struct sockaddr_in *) (char *)&piiu->recvfrom;
#endif

    piiu->recv->stk += io_count;
    io_count = piiu->recv->stk;

    while(TRUE){
      pbuf = (char *) (pcount+1);

      /* switch from 68000 to VAX byte order */
      byte_cnt = (unsigned long) ntohl(*pcount);
      if(byte_cnt>MAX_MSG_SIZE || byte_cnt<sizeof(*pcount)){
        printf("recv_msg_ast(): msg over/underflow %u\n",byte_cnt);
        break;
      }

      if(io_count == byte_cnt){
        post_msg(pbuf, byte_cnt-sizeof(*pcount), paddr->sin_addr, piiu);
        break;
      }
      else if(io_count > byte_cnt)
        post_msg(pbuf, byte_cnt-sizeof(*pcount), paddr->sin_addr, piiu);
      else{
        if(pcount != &piiu->recv->buf){
          memcpy(piiu->recv->buf, pcount, io_count);  
          piiu->recv->stk = io_count;
        }

        bufsize = sizeof(piiu->recv->buf) - piiu->recv->stk;
        io_status = sys$qio(	NULL,
				piiu->sock_chan,
				IO$_RECEIVE,
				&piiu->iosb,
				recv_msg_ast,
				iocix,
				&piiu->recv->buf[piiu->recv->stk],
				bufsize,
				NULL,
				&piiu->recvfrom,
				sizeof(piiu->recvfrom),
				NULL);
        if(io_status != SS$_NORMAL)
          lib$signal(io_status);
	return;
      }
      io_count -= byte_cnt;
      pcount = (int *) (byte_cnt + (char *)pcount);

    }
  }

  if(piiu->sock_proto == IPPROTO_TCP)  
    flow_control(piiu);

  piiu->recv->stk = 0;
  io_status = sys$qio(	NULL,
			piiu->sock_chan,
			IO$_RECEIVE,
			&piiu->iosb,
			recv_msg_ast,
			iocix,
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



