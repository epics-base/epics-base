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
/*	102993	joh	toggle set sock opt to set			*/
/*	021794	joh	turn on SO_REUSEADDR only after the test for	*/
/*			address in use so that test works on UNIX	*/
/*			kernels that support multicast			*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC socket interface module				*/
/*	File:	share/src/ca/iocinf.c					*/
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

static char *sccsId = "$Id$";


/*	Allocate storage for global variables in this module		*/
#define		CA_GLBLSOURCE
#include	"iocinf.h"
#include	"net_convert.h"
#include	<netinet/tcp.h>

#ifdef __STDC__
LOCAL void 	tcp_recv_msg(struct ioc_in_use *piiu);
LOCAL void 	cac_flush_internal();
LOCAL void 	cac_tcp_send_msg_piiu(struct ioc_in_use *piiu);
LOCAL void 	cac_udp_send_msg_piiu(struct ioc_in_use *piiu);
LOCAL void 	notify_ca_repeater();
LOCAL void 	udp_recv_msg(struct ioc_in_use *piiu);
#ifdef VMS
LOCAL void 	vms_recv_msg_ast(struct ioc_in_use *piiu);
#endif /*VMS*/
LOCAL void 	ca_process_tcp(struct ioc_in_use *piiu);
LOCAL void 	ca_process_udp(struct ioc_in_use *piiu);
LOCAL void 	ca_process_input_queue();
LOCAL void	close_ioc(struct ioc_in_use *piiu);
LOCAL void 	cacRingBufferInit(struct ca_buffer *pBuf, unsigned long size);
LOCAL char	*getToken(char **ppString);

#else /*__STDC__*/

LOCAL void	tcp_recv_msg();
LOCAL void   	cac_flush_internal();
LOCAL void 	cac_tcp_send_msg_piiu();
LOCAL void 	cac_udp_send_msg_piiu();
LOCAL void 	notify_ca_repeater();
LOCAL void	udp_recv_msg();
#ifdef VMS
void   		vms_recv_msg_ast();
#endif /*VMS*/
LOCAL void 	ca_process_tcp();
LOCAL void 	ca_process_udp();
LOCAL void 	ca_process_input_queue();
LOCAL void	close_ioc();
LOCAL void 	cacRingBufferInit();
LOCAL char 	*getToken();

#endif /*__STDC__*/




/*
 *	ALLOC_IOC()
 *
 * 	allocate and initialize an IOC info block for unallocated IOC
 *
 */
#ifdef __STDC__
int alloc_ioc(
struct in_addr			*pnet_addr,
struct ioc_in_use		**ppiiu
)
#else
int alloc_ioc(pnet_addr, ppiiu)
struct in_addr			*pnet_addr;
struct ioc_in_use		**ppiiu;
#endif
{
	caAddrNode		*pNode;
	struct ioc_in_use	*piiu;
  	int			status;

	/*
	 * look for an existing connection
	 *
	 * quite a bottle neck with increasing connection count
	 *
	 */
	LOCK;
	for(	piiu = (struct ioc_in_use *) iiuList.node.next;
		piiu;
		piiu = (struct ioc_in_use *) piiu->node.next){

		if(piiu->sock_proto!=IPPROTO_TCP){
			continue;
		}

		pNode = (caAddrNode *)piiu->destAddr.node.next;
		assert(pNode);
		assert(pNode->destAddr.sockAddr.sa_family == AF_INET);
    		if(pnet_addr->s_addr == 
			pNode->destAddr.inetAddr.sin_addr.s_addr){

			if(!piiu->conn_up){
				continue;
			}

			*ppiiu = piiu;
			UNLOCK;
      			return ECA_NORMAL;
		}
    	}
	UNLOCK;

  	status = create_net_chan(ppiiu, pnet_addr, IPPROTO_TCP);
  	return status;

}


/*
 *	CREATE_NET_CHANNEL()
 *
 */
#ifdef __STDC__
int create_net_chan(
struct ioc_in_use 	**ppiiu,
struct in_addr		*pnet_addr,	/* only used by TCP connections */
int			net_proto
)
#else
int create_net_chan(ppiiu, pnet_addr, net_proto)
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
	caAddrNode		*pNode;

	LOCK;

	piiu = (IIU *) calloc(1, sizeof(*piiu));
	if(!piiu){
		UNLOCK;
		return ECA_ALLOCMEM;
	}

	piiu->send_retry_count = SEND_RETRY_COUNT_INIT;

	piiu->active = FALSE;

	piiu->pcas = ca_static;
	ellInit(&piiu->chidlist);
	ellInit(&piiu->destAddr);

  	piiu->sock_proto = net_proto;

	/*
	 * reset the delay to the next retry or keepalive
	 */
	piiu->next_retry = CA_CURRENT_TIME;
	piiu->retry_delay = CA_RECAST_DELAY;
	piiu->nconn_tries = 0;

	/*
	 * set the minor version ukn until the server
	 * updates the client 
	 */
	piiu->minor_version_number = CA_UKN_MINOR_VERSION;

  	switch(piiu->sock_proto)
  	{
    		case	IPPROTO_TCP:

		assert(pnet_addr);
		pNode = (caAddrNode *)calloc(1,sizeof(*pNode));
		if(!pNode){
			free(pNode);
			UNLOCK;
			return ECA_ALLOCMEM;
		}
  		pNode->destAddr.inetAddr.sin_family = AF_INET;
		pNode->destAddr.inetAddr.sin_addr = *pnet_addr;
  		pNode->destAddr.inetAddr.sin_port = htons(CA_SERVER_PORT);
		ellAdd(&piiu->destAddr, &pNode->node);
		piiu->recvBytes = tcp_recv_msg; 
		piiu->sendBytes = cac_tcp_send_msg_piiu; 
		piiu->procInput = ca_process_tcp; 

      		/* 	allocate a socket	*/
      		sock = socket(	AF_INET,	/* domain	*/
				SOCK_STREAM,	/* type		*/
				0);		/* deflt proto	*/
      		if(sock == ERROR){
			free(piiu);
			UNLOCK;
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
			UNLOCK;
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
			UNLOCK;
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
      			assert(status >= 0);
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
			UNLOCK;
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
			UNLOCK;
          		return ECA_SOCK;
        	}

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
			UNLOCK;
          		return ECA_SOCK;
        	}
#endif

      		/* connect */
      		status = connect(	
				sock,
				&pNode->destAddr.sockAddr,
				sizeof(pNode->destAddr.sockAddr));
      		if(status < 0){
			ca_printf("CAC: no conn errno %d\n", MYERRNO);
        		status = socket_close(sock);
			if(status<0){
				SEVCHK(ECA_INTERNAL,NULL);
			}
			free(piiu);
			UNLOCK;
        		return ECA_CONN;
      		}
		cacRingBufferInit(&piiu->recv, sizeof(piiu->send.buf));
		cacRingBufferInit(&piiu->send, sizeof(piiu->send.buf));

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
		caHostFromInetAddr(
			pnet_addr,
			piiu->host_name_str,
			sizeof(piiu->host_name_str));
      		break;

    	case	IPPROTO_UDP:
	
		assert(!pnet_addr);
		piiu->recvBytes = udp_recv_msg; 
		piiu->sendBytes = cac_udp_send_msg_piiu; 
		piiu->procInput = ca_process_udp; 

      		/* 	allocate a socket			*/
      		sock = socket(	AF_INET,	/* domain	*/
				SOCK_DGRAM,	/* type		*/
				0);		/* deflt proto	*/
      		if(sock == ERROR){
			free (piiu);
			UNLOCK;
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
			UNLOCK;
        		return ECA_CONN;
      		}

      		memset((char *)&saddr,0,sizeof(saddr));
      		saddr.sin_family = AF_INET;
		/* 
		 * let slib pick lcl addr 
		 */
      		saddr.sin_addr.s_addr = INADDR_ANY; 
      		saddr.sin_port = htons(0);	

      		status = bind(	sock, 
				(struct sockaddr *) &saddr, 
				sizeof(saddr));
      		if(status<0){
        		ca_printf("CAC: bind (errno=%d)\n",MYERRNO);
			ca_signal(ECA_INTERNAL,"bind failed");
      		}

		caDiscoverInterfaces(
			&piiu->destAddr,
			sock,
			CA_SERVER_PORT);

		caAddConfiguredAddr(
			&piiu->destAddr, 
			&EPICS_CA_SEARCH_ADDR_LIST,
			sock,
			CA_SERVER_PORT);

		cacRingBufferInit(&piiu->recv, sizeof(piiu->send.buf));
		cacRingBufferInit(&piiu->send, min(MAX_UDP, sizeof(piiu->send.buf)));

		strncpy(
			piiu->host_name_str, 
			"<<unknown host>>", 
			sizeof(piiu->host_name_str)-1);

      		break;

	default:
		free(piiu);
      		ca_signal(ECA_INTERNAL,"alloc_ioc: ukn protocol\n");
		/*
		 * turn off gcc warnings
		 */
		UNLOCK;
		return ECA_INTERNAL;
  	}

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

	UNLOCK;

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
 */
LOCAL void notify_ca_repeater()
{
	struct sockaddr_in	saddr;
	int			status;

	if(!piiuCast)
		return;
	if(!piiuCast->conn_up)
		return;

	LOCK; /*MULTINET TCP/IP routines are not reentrant*/
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
			if(	MYERRNO != EINTR && 
				MYERRNO != ENOBUFS && 
				MYERRNO != EWOULDBLOCK){
				ca_printf(
					"send error => %s\n", 
					strerror(MYERRNO));
				assert(0);	
			}
		}
	}
	UNLOCK;
}



/*
 *	CAC_UDP_SEND_MSG_PIIU()
 *
 */
#ifdef __STDC__
LOCAL void cac_udp_send_msg_piiu(struct ioc_in_use *piiu)
#else
LOCAL void cac_udp_send_msg_piiu(piiu)
struct ioc_in_use 	*piiu;
#endif
{
	caAddrNode	*pNode;
	unsigned long	sendCnt;
  	int		status;

	/*
	 * check for shutdown in progress
	 */
	if(!piiu->conn_up){
		return;
	}

	LOCK;

	sendCnt = cacRingBufferReadSize(&piiu->send, TRUE);

	assert(sendCnt<=piiu->send.max_msg);

	/*
	 * return if nothing to send
	 */
	if(sendCnt == 0){
		UNLOCK;
		return;
	}

	pNode = (caAddrNode *) piiu->destAddr.node.next;
	while(pNode){
		status = sendto(
				piiu->sock_chan,
				&piiu->send.buf[piiu->send.rdix],	
				sendCnt,
				0,
				&pNode->destAddr.sockAddr,
				sizeof(pNode->destAddr.sockAddr));
		if(status<0){
			if(	MYERRNO != EWOULDBLOCK && 
				MYERRNO != ENOBUFS && 
				MYERRNO != EINTR){
				ca_printf(
					"CAC: error on socket send() %d\n",
					MYERRNO);
			}

			piiu->conn_up = FALSE;
			break;
		}
		assert(status == sendCnt);
		pNode = (caAddrNode *) pNode->node.next;
	}

	/*
	 * forces UDP send buffer to be a 
	 * queue instead of a ring buffer
	 * (solves ring boundary problems)
	 */
	cacRingBufferInit(
			&piiu->send, 
			min(MAX_UDP, sizeof(piiu->send.buf)));
	piiu->send_needed = FALSE;

	UNLOCK;
	return;
}


/*
 *	CAC_TCP_SEND_MSG_PIIU()
 *
 */
#ifdef __STDC__
LOCAL void cac_tcp_send_msg_piiu(struct ioc_in_use *piiu)
#else
LOCAL void cac_tcp_send_msg_piiu(piiu)
struct ioc_in_use 	*piiu;
#endif
{
	unsigned long	sendCnt;
  	int		status;

	/*
	 * check for shutdown in progress
	 */
	if(!piiu->conn_up){
		return;
	}

	LOCK;

	sendCnt = cacRingBufferReadSize(&piiu->send, TRUE);

	assert(sendCnt<=piiu->send.max_msg);

	/*
	 * return if nothing to send
	 */
	if(sendCnt == 0){
		UNLOCK;
		return;
	}

	status = send(
			piiu->sock_chan,
			&piiu->send.buf[piiu->send.rdix],	
			sendCnt,
			0);
	if(status>=0){
		assert(status<=sendCnt);

		/*
		 * reset the delay to the next keepalive
		 */
    		if(status){
			piiu->next_retry = time(NULL) + CA_RETRY_PERIOD;
		}

		CAC_RING_BUFFER_READ_ADVANCE(&piiu->send, status);
		
		sendCnt = cacRingBufferReadSize(&piiu->send, FALSE);
		if(sendCnt==0){
			piiu->send_needed = FALSE;
		}

		UNLOCK;
		return;
	}

	if(	MYERRNO == EWOULDBLOCK ||
		MYERRNO == ENOBUFS ||
		MYERRNO == EINTR){
			UNLOCK;
			return;
	}

	if(	MYERRNO != EPIPE && 
		MYERRNO != ECONNRESET &&
		MYERRNO != ETIMEDOUT){
		ca_printf(	
			"CAC: error on socket send() %d\n",
			MYERRNO);
	}

	piiu->conn_up = FALSE;
	UNLOCK;
	return;
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

	LOCK;
	for(	piiu = (IIU *)iiuList.node.next; 
		piiu; 
		piiu = (IIU *)piiu->node.next){

		if(!piiu->conn_up){
			continue;
		}
		if(!piiu->send_needed){
			continue;
		}
		piiu->sendBytes(piiu);
	}
	UNLOCK;
}


/*
 *	CA_MUX_IO()
 *
 * 	Asynch notification of incomming messages under UNIX
 *	1) Wait no longer than timeout 
 *	2) Return early if nothing outstanding
 *
 * 
 */
#if defined(UNIX) || defined(vxWorks)
#ifdef __STDC__
void ca_mux_io(struct timeval  *ptimeout, int flags)
#else
void ca_mux_io(ptimeout, flags)
struct timeval 	*ptimeout;
int		flags;
#endif
{
	int			count;
	int			newInput;
	struct timeval		timeout;

  	if(!ca_static->ca_repeater_contacted){
		notify_ca_repeater();
	}

	timeout = *ptimeout;
	do{
		newInput = FALSE;
		do{
			count = ca_select_io(&timeout, flags);
			if(count>0){
				newInput = TRUE;
			}
			timeout.tv_usec = 0;
			timeout.tv_sec = 0;
		}
		while(count>0);

		ca_process_input_queue();
	}
	while(newInput);
}
#endif


/*
 * ca_select_io()
 */
#if defined(UNIX) || defined(vxWorks)
#ifdef __STDC__
int ca_select_io(struct timeval  *ptimeout, int flags)
#else
int ca_select_io(ptimeout, flags)
struct timeval 	*ptimeout;
int		flags;
#endif
{
  	long		status;
  	IIU		*piiu;
	unsigned long	minfreespace;
	unsigned long	freespace;

	LOCK;
	piiu=(IIU *)iiuList.node.next;
	while(piiu){

		/*
		 * clean out dead wood
		 */
		if(!piiu->conn_up){
			IIU *pnextiiu;

			pnextiiu = (IIU *)piiu->node.next;
			close_ioc(piiu);
			piiu = pnextiiu;
			continue;
		}

		/*
		 * Dont bother receiving if we have insufficient
		 * space for the maximum UDP message
		 */
		if(flags&CA_DO_RECVS){
			freespace = cacRingBufferWriteSize(&piiu->recv, TRUE);
			if(piiu->sock_proto == IPPROTO_UDP){
				minfreespace = 
					MAX_UDP+2*sizeof(struct udpmsglog);
			}
			else{
				minfreespace = 1;
			}

			if(freespace>=minfreespace){
       				FD_SET(piiu->sock_chan,&readch);
			}
		}

		if(flags&CA_DO_SENDS){
			if(cacRingBufferReadSize(&piiu->send, FALSE)>0){
				FD_SET(piiu->sock_chan,&writech);
			}
		}

		piiu=(IIU *)piiu->node.next;
	}
	UNLOCK;

    	status = select(
			sizeof(fd_set)*NBBY,
			&readch,
			&writech,
			NULL,
			ptimeout);

	if(status<0){
    		if(MYERRNO == EINTR){
		}
    		else if(MYERRNO == EWOULDBLOCK){
			ca_printf("CAC: blocked at select ?\n");
    		}                                           
    		else{
			char text[255];                                         
     			sprintf(
				text,
				"CAC: unexpected select fail: %d",
				MYERRNO); 
      			SEVCHK(ECA_INTERNAL,text);   
		}
	}

	LOCK;
	if(status>0){
    		for(	piiu=(IIU *)iiuList.node.next;
			piiu;
			piiu=(IIU *)piiu->node.next){

			if(!piiu->conn_up){
				continue;
			}

      			if(flags&CA_DO_SENDS && 
				FD_ISSET(piiu->sock_chan,&writech)){ 
				(*piiu->sendBytes)(piiu);
			}

      			if(flags&CA_DO_RECVS && 
				FD_ISSET(piiu->sock_chan,&readch)){
				(*piiu->recvBytes)(piiu);
			}

			FD_CLR(piiu->sock_chan,&readch);
			FD_CLR(piiu->sock_chan,&writech);
		}
	}
	UNLOCK;

	return status;
}
#endif


/*
 * ca_process_input_queue()
 */
LOCAL void ca_process_input_queue()
{
	struct ioc_in_use 	*piiu;

	/*
	 * dont allow recursion 
	 */
	if(post_msg_active){
		return;
	}

	LOCK;
    	for(	piiu=(IIU *)iiuList.node.next;
		piiu;
		piiu=(IIU *)piiu->node.next){

		if(!piiu->conn_up){
			continue;
		}

		(*piiu->procInput)(piiu);
	}
	UNLOCK;

	cac_flush_internal();
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
	unsigned long	writeSpace;
  	int		status;

	if(!piiu->conn_up){
		return;
	}

	LOCK;

	writeSpace = cacRingBufferWriteSize(&piiu->recv, TRUE);
	if(writeSpace == 0){
		UNLOCK;
		return;
	}

 	status = recv(	piiu->sock_chan,
			&piiu->recv.buf[piiu->recv.wtix],
			writeSpace,
			0);
	if(status == 0){
		piiu->conn_up = FALSE;
		UNLOCK;
		return;
	}
    	else if(status <0){
    		/* try again on status of -1 and no luck this time */
      		if(MYERRNO == EWOULDBLOCK || MYERRNO == EINTR){
			UNLOCK;
			return;
		}

       	 	if(	MYERRNO != EPIPE && 
			MYERRNO != ECONNRESET &&
			MYERRNO != ETIMEDOUT){
	  		ca_printf(	"CAC: unexpected recv error (errno=%d)\n",
				MYERRNO);
		}
		piiu->conn_up = FALSE;
		UNLOCK;
		return;
    	}


  	if(status>MAX_MSG_SIZE){
    		ca_printf(	"CAC: recv_msg(): message overflow %l\n",
				status-MAX_MSG_SIZE);
		piiu->conn_up = FALSE;
		UNLOCK;
    		return;
 	}

	CAC_RING_BUFFER_WRITE_ADVANCE(&piiu->recv, status);

	/*
	 * reset the delay to the next keepalive
	 */
    	if(piiu != piiuCast){
		piiu->next_retry = time(NULL) + CA_RETRY_PERIOD;
	}

	UNLOCK;
	return;
}


/*
 * ca_process_tcp()
 *
 */
#ifdef __STDC__
LOCAL void ca_process_tcp(struct ioc_in_use *piiu)
#else
LOCAL void ca_process_tcp(piiu)
struct ioc_in_use	*piiu;
#endif
{
	caAddrNode	*pNode;
	int		status;
	long		bytesToProcess;

	/*
	 * dont allow recursion
	 */
	if(post_msg_active){
		return;
	}

	pNode = (caAddrNode *) piiu->destAddr.node.next;

	post_msg_active++;

	LOCK;
	while(TRUE){
		bytesToProcess = cacRingBufferReadSize(&piiu->recv, TRUE);
		if(bytesToProcess == 0){
			break;
		}

  		/* post message to the user */
  		status = post_msg(
				piiu, 
				&pNode->destAddr.inetAddr.sin_addr,
				&piiu->recv.buf[piiu->recv.rdix],
				bytesToProcess);
		if(status != OK){
			piiu->conn_up = FALSE;
			post_msg_active--;
			return;
		}
		CAC_RING_BUFFER_READ_ADVANCE(
				&piiu->recv,
				bytesToProcess);
	}
	UNLOCK;

	post_msg_active--;

    	flow_control(piiu);

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
  	int			reply_size;
	struct udpmsglog	*pmsglog;
	unsigned long		bytesAvailable;
	
	if(!piiu->conn_up){
		return;
	}

	LOCK;

	bytesAvailable = cacRingBufferWriteSize(&piiu->recv, TRUE);
	assert(bytesAvailable >= MAX_UDP+2*sizeof(*pmsglog));
	pmsglog = (struct udpmsglog *) &piiu->recv.buf[piiu->recv.wtix];

  	reply_size = sizeof(pmsglog->addr);
    	status = recvfrom(	
			piiu->sock_chan,
			(char *)(pmsglog+1),
			MAX_UDP,
			0,
			(struct sockaddr *)&pmsglog->addr, 
			&reply_size);
    	if(status < 0){
		/*
		 * op would block which is ok to ignore till ready
		 * later
		 */
      		if(MYERRNO == EWOULDBLOCK || MYERRNO == EINTR){
			UNLOCK;
       			return;
		}
		ca_printf("Unexpected UDP failure %d\n", MYERRNO);
		piiu->conn_up = FALSE;
    	}
	else if(status > 0){
		unsigned long		bytesActual;

		/*	
	 	 * log the msg size
		 * and advance the ring index
	 	 */
		pmsglog->nbytes = (long) status;
		pmsglog->valid = TRUE;
		bytesActual = status + sizeof(*pmsglog);
		CAC_RING_BUFFER_WRITE_ADVANCE(&piiu->recv, bytesActual);
		/*
		 * if there isnt enough room at the end advance
		 * to the beginning of the ring
		 */
		bytesAvailable = cacRingBufferWriteSize(&piiu->recv, TRUE);
		if( bytesAvailable < MAX_UDP+2*sizeof(*pmsglog) ){
			assert(bytesAvailable>=sizeof(*pmsglog));
			pmsglog = (struct udpmsglog *) 
				&piiu->recv.buf[piiu->recv.wtix];
			pmsglog->valid = FALSE;	
			pmsglog->nbytes = bytesAvailable - sizeof(*pmsglog); 
			CAC_RING_BUFFER_WRITE_ADVANCE(
				&piiu->recv, bytesAvailable);
		}
#		ifdef DEBUG
      			ca_printf(
				"%s: udp reply of %d bytes\n",
				__FILE__,
				byte_cnt);
#		endif
	}

	UNLOCK;

	return;
}



/*
 *	CA_PROCESS_UDP()
 *
 */
#ifdef __STDC__
LOCAL void ca_process_udp(struct ioc_in_use *piiu)
#else
LOCAL void ca_process_udp(piiu)
struct ioc_in_use	*piiu;
#endif
{
  	int			status;
	struct udpmsglog	*pmsglog;
	char			*pBuf;
	unsigned long		bytesAvailable;

	/*
	 * dont allow recursion
	 */
	if(post_msg_active){
		return;
	}


	post_msg_active++;

	LOCK;
	while(TRUE){

		bytesAvailable = cacRingBufferReadSize(&piiu->recv, TRUE);
		if(bytesAvailable == 0){
			break;
		}

		assert(bytesAvailable>=sizeof(*pmsglog));

		pBuf = &piiu->recv.buf[piiu->recv.rdix];
		while(pBuf<&piiu->recv.buf[piiu->recv.rdix]+bytesAvailable){
			pmsglog = (struct udpmsglog *) pBuf;

  			/* post message to the user */
			if(pmsglog->valid){
  				status = post_msg(
					piiu,
					&pmsglog->addr.sin_addr,
					(char *)(pmsglog+1),
					pmsglog->nbytes);
				if(status != OK || piiu->curMsgBytes){
					ca_printf(
						"%s: bad UDP msg from port=%d addr=%x\n",
						__FILE__,
						pmsglog->addr.sin_port,
						pmsglog->addr.sin_addr.s_addr);
					/*
					 * resync the ring buffer
					 * (discard existing messages)
					 */
					cacRingBufferInit(
						&piiu->recv, 
						sizeof(piiu->recv.buf));
					piiu->curMsgBytes = 0;
					piiu->curDataBytes = 0;
					post_msg_active--;
					UNLOCK;
					return;
				}
			}
			pBuf += sizeof(*pmsglog)+pmsglog->nbytes;
		}
		CAC_RING_BUFFER_READ_ADVANCE(
				&piiu->recv,
				bytesAvailable);
  	}

	UNLOCK;

	post_msg_active--;

  	return; 
}




#ifdef vxWorks
/*
 * RECV_TASK()
 *
 */
#ifdef __STDC__
void cac_recv_task(int	tid)
#else /*__STDC__*/
void cac_recv_task(tid)
int		tid;
#endif /*__STDC__*/
{
	struct timeval	timeout;
  	int		status;

	taskwdInsert((int) taskIdCurrent, NULL, NULL);

	status = ca_import(tid);
	SEVCHK(status, NULL);

	/*
	 * once started, does not exit until
	 * ca_task_exit() is called.
	 */
  	while(TRUE){
		timeout.tv_usec = 0;
		timeout.tv_sec = 20;
		ca_mux_io(&timeout, CA_DO_RECVS);
	}
}
#endif /*vxWorks*/


/*
 *
 *	VMS_RECV_MSG_AST()
 *
 *
 */
#ifdef VMS
#ifdef __STDC__
LOCAL void vms_recv_msg_ast(struct ioc_in_use *piiu)
#else /*__STDC__*/
LOCAL void vms_recv_msg_ast(piiu)
struct ioc_in_use	*piiu;
#endif /*__STDC__*/
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

	if(piiu->conn_up){
		(*piiu->recvBytes)(piiu);
		ca_process_input_queue();
	}
	else{
		close_ioc(piiu);
		return;
	}
	  
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
#endif /*VMS*/


/*
 *
 *	CLOSE_IOC()
 *
 *	set an iiu in the disconnected state
 *
 *
 */
#ifdef __STDC__
LOCAL void close_ioc(struct ioc_in_use *piiu)
#else /*__STDC__*/
LOCAL void close_ioc(piiu)
struct ioc_in_use	*piiu;
#endif /*__STDC__*/
{
  	chid				chix;
	int				status;


	/*
	 * dont close twice
	 */
  	if(piiu->sock_chan == -1){
		return;
	}

	LOCK;

	ellDelete(&iiuList, &piiu->node);

	/*
	 * attempt to clear out messages in recv queue
	 */
	(*piiu->procInput)(piiu);

	if(piiu == piiuCast){
		piiuCast = NULL;
	}

	/*
	 * Mark all of their channels disconnected
	 * prior to calling handlers incase the
	 * handler tries to use a channel before
	 * I mark it disconnected.
	 *
	 */
  	chix = (chid) &piiu->chidlist.node.next;
  	while(chix = (chid) chix->node.next){
    		chix->type = TYPENOTCONN;
    		chix->count = 0;
    		chix->state = cs_prev_conn;
    		chix->id.sid = ~0L;
		chix->ar.read_access = FALSE;
		chix->ar.write_access = FALSE;
  	}

	/*
	 * remove IOC from the hash table
	 */
	{
		caAddrNode	*pNode;
		bhe		*pBHE;
		bhe		**ppBHE;
		unsigned 	index;

		pNode = (caAddrNode *) piiu->destAddr.node.next;
        	index = ntohl(pNode->destAddr.inetAddr.sin_addr.s_addr);
        	index &= BHT_INET_ADDR_MASK;
		ppBHE = &ca_static->ca_beaconHash[index];
		pBHE = *ppBHE;
        	while(pBHE){
                	if(pBHE->inetAddr.s_addr == 
				pNode->destAddr.inetAddr.sin_addr.s_addr){
				*ppBHE = pBHE->pNext;
				free(pBHE);
                       		break;
                	}
			ppBHE = &pBHE->pNext;
			pBHE = *ppBHE;
		}
	}

	/*
	 * call their connection handler as required
	 */
  	chix = (chid) &piiu->chidlist.node.next;
  	while(chix = (chid) chix->node.next){
		LOCKEVENTS;
    		if(chix->connection_func){
  			struct connection_handler_args args;
			args.chid = chix;
			args.op = CA_OP_CONN_DOWN;
      			(*chix->connection_func)(args);
    		}
    		if(chix->access_rights_func){
  			struct access_rights_handler_args args;
			args.chid = chix;
			args.ar = chix->ar;
      			(*chix->access_rights_func)(args);
		}
		UNLOCKEVENTS;
		chix->piiu = piiuCast;
  	}

	/*
	 * move all channels to the broadcast IIU
	 *
	 * if we loose the broadcast IIU its a severe error
	 */
	assert(piiuCast);
	ellConcat(&piiuCast->chidlist, &piiu->chidlist);

  	assert(piiu->chidlist.count==0);

  	if(fd_register_func){
		LOCKEVENTS;
		(*fd_register_func)(fd_register_arg, piiu->sock_chan, FALSE);
		UNLOCKEVENTS;
	}

  	status = socket_close(piiu->sock_chan);
	assert(status == 0);

	/*
	 * free message body cache
	 */
	if(piiu->pCurData){
		free(piiu->pCurData);
		piiu->pCurData = NULL;
		piiu->curDataMax = 0;
	}

  	piiu->sock_chan = -1;

	ellFree(&piiu->destAddr);

	free(piiu);
	UNLOCK;
}



/*
 *	REPEATER_INSTALLED()
 *
 *	Test for the repeater already installed
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
	int				true = 1;

	int 				installed = FALSE;

	LOCK;

     	/* 	allocate a socket			*/
      	sock = socket(	AF_INET,	/* domain	*/
			SOCK_DGRAM,	/* type		*/
			0);		/* deflt proto	*/
      	if(sock == ERROR){
		UNLOCK;
		return installed;
	}

      	memset((char *)&bd,0,sizeof bd);
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = INADDR_ANY;	
     	bd.sin_port = htons(CA_CLIENT_PORT);	
      	status = bind(	sock, 
			(struct sockaddr *) &bd, 
			sizeof bd);
     	if(status<0){
		if(MYERRNO == EADDRINUSE){
			installed = TRUE;
		}
	}

	/*
	 * turn on reuse only after the test so that
	 * this works on UNIX kernels that support multicast
	 */
	status = setsockopt(	sock,
				SOL_SOCKET,
				SO_REUSEADDR,
				(char *)&true,
				sizeof true);
	if(status<0){
		ca_printf(      "CAC: set socket option reuseaddr failed\n");
	}

	status = socket_close(sock);
	if(status<0){
		SEVCHK(ECA_INTERNAL,NULL);
	}
		
	UNLOCK;

	return installed;
}



/*
 * cacRingBufferRead()
 *
 * returns the number of bytes read which may be less than
 * the number requested.
 */
#ifdef __STDC__
unsigned long cacRingBufferRead(
struct ca_buffer	*pRing,
void			*pBuf,
unsigned long		nBytes)
#else /*__STDC__*/
unsigned long cacRingBufferRead(pRing, pBuf, nBytes)
struct ca_buffer	*pRing;
void			*pBuf;
unsigned long		nBytes;
#endif /*__STDC__*/
{
	unsigned long	potentialBytes;
	unsigned long	actualBytes;
	char		*pCharBuf;

	actualBytes = 0;
	pCharBuf = pBuf;
	while(TRUE){
		potentialBytes = cacRingBufferReadSize(pRing, TRUE);
		if(potentialBytes == 0){
			return actualBytes;
		}
		potentialBytes = min(potentialBytes, nBytes-actualBytes);
		memcpy(pCharBuf, &pRing->buf[pRing->rdix], potentialBytes);
		CAC_RING_BUFFER_READ_ADVANCE(pRing, potentialBytes);
		pCharBuf += potentialBytes;
		actualBytes += potentialBytes;	
		if(nBytes <= actualBytes){
			return actualBytes;
		}
	}
}


/*
 * cacRingBufferWrite()
 *
 * returns the number of bytes written which may be less than
 * the number requested.
 */
#ifdef __STDC__
unsigned long cacRingBufferWrite(
struct ca_buffer	*pRing,
void			*pBuf,
unsigned long		nBytes)
#else /*__STDC__*/
unsigned long cacRingBufferWrite(pRing,pBuf,nBytes)
struct ca_buffer	*pRing;
void			*pBuf;
unsigned long		nBytes;
#endif /*__STDC__*/
{
	unsigned long	potentialBytes;
	unsigned long	actualBytes;
	char		*pCharBuf;

	actualBytes = 0;
	pCharBuf = pBuf;
	while(TRUE){
		potentialBytes = cacRingBufferWriteSize(pRing, TRUE);
		if(potentialBytes == 0){
			return actualBytes;
		}
		potentialBytes = min(potentialBytes, nBytes-actualBytes);
		memcpy(pRing->buf+pRing->wtix, pCharBuf, potentialBytes);
		CAC_RING_BUFFER_WRITE_ADVANCE(pRing, potentialBytes);
		pCharBuf += potentialBytes;
		actualBytes += potentialBytes;	
		if(nBytes <= actualBytes){
			return actualBytes;
		}
	}
}



/*
 * cacRingBufferInit()
 *
 */
#ifdef __STDC__
LOCAL void cacRingBufferInit(struct ca_buffer *pBuf, unsigned long size)
#else /*__STDC__*/
LOCAL void cacRingBufferInit(pBuf, size)
struct ca_buffer *pBuf;
#endif /*__STDC__*/
{
	assert(size<=sizeof(pBuf->buf));
	pBuf->max_msg = size;
	pBuf->rdix = 0;
	pBuf->wtix = 0;
	pBuf->readLast = TRUE;
}


/*
 * cacRingBufferReadSize()
 *
 * returns N bytes available
 * (not nec contiguous)
 */
#ifdef __STDC__
unsigned long cacRingBufferReadSize(struct ca_buffer *pBuf, int contiguous)
#else /*__STDC__*/
unsigned long cacRingBufferReadSize(pBuf, contiguous)
struct ca_buffer *pBuf;
int contiguous;
#endif /*__STDC__*/
{
	unsigned long 	count;
	
	if(pBuf->wtix < pBuf->rdix){
		count = pBuf->max_msg - pBuf->rdix;
		if(!contiguous){
			count += pBuf->wtix;
		}
	}
	else if(pBuf->wtix > pBuf->rdix){
		count = pBuf->wtix - pBuf->rdix;
	}
	else if(pBuf->readLast){
		count = 0;
	}
	else{
		if(contiguous){
			count = pBuf->max_msg - pBuf->rdix;
		}
		else{
			count = pBuf->max_msg;
		}
	}

#if 0
	printf("%d bytes available for reading \n", count);
#endif

	return count;
}


/*
 * cacRingBufferWriteSize()
 *
 * returns N bytes available
 * (not nec contiguous)
 */
#ifdef __STDC__
unsigned long cacRingBufferWriteSize(struct ca_buffer *pBuf, int contiguous)
#else /*__STDC__*/
unsigned long cacRingBufferWriteSize(pBuf, contiguous)
struct ca_buffer *pBuf;
int contiguous;
#endif /*__STDC__*/
{
	unsigned long 	count;

	if(pBuf->wtix < pBuf->rdix){
		count = pBuf->rdix - pBuf->wtix;
	}
	else if(pBuf->wtix > pBuf->rdix){
		count = pBuf->max_msg - pBuf->wtix;
		if(!contiguous){
			count += pBuf->rdix;
		}
	}
	else if(pBuf->readLast){
		if(contiguous){
			count = pBuf->max_msg - pBuf->wtix;
		}
		else{
			count = pBuf->max_msg;
		}
	}
	else{
		count = 0;
	}

#if 0
	printf("%d bytes available for writing\n", count);
#endif
	return count;
}



/*
 * localHostName()
 *
 * o Indicates failure by setting ptr to nill
 *
 * o Calls non posix gethostbyname() so that we get DNS style names
 *      (gethostbyname() should be available will most BSD sock libs)
 *
 * vxWorks user will need to configure a DNS format name for the
 * host name if they wish to be cnsistent UNIX and VMS hosts.
 *
 */
char *localHostName()
{
        int     size;
        int     status;
        char    pName[MAXHOSTNAMELEN];
	char	*pTmp;

        status = gethostname(pName, sizeof(pName));
        if(status){
                return NULL;
        }

        size = strlen(pName)+1;
        pTmp = malloc(size);
        if(!pTmp){
                return pTmp;
        }

        strncpy(pTmp, pName, size-1);
        pTmp[size-1] = '\0';

	return pTmp;
}


/*
 * caAddConfiguredAddr()
 */
#ifdef __STDC__
void caAddConfiguredAddr(ELLLIST *pList, ENV_PARAM *pEnv, 
	int socket, int port)
#else /*__STDC__*/
void caAddConfiguredAddr(pList, pEnv, socket, port)
ELLLIST         *pList;
ENV_PARAM       *pEnv;
int		socket;
int		port;
#endif /*__STDC__*/
{
        caAddrNode              *pNode;
        ENV_PARAM               list;
        char                    *pStr;
        char                    *pToken;
	union caAddr		addr;
	union caAddr		localAddr;
	int			status;

        pStr = envGetConfigParam(
                        pEnv,
                        sizeof(list.dflt),
                        list.dflt);
        if(!pStr){
                return;
        }

	/*
	 * obtain a local address
	 */
	status = local_addr(socket, &localAddr.inetAddr);
	if(status){
		return;
	}

        while(pToken = getToken(&pStr)){
		addr.inetAddr.sin_family = AF_INET;
  		addr.inetAddr.sin_port = htons(port);
                addr.inetAddr.sin_addr.s_addr = inet_addr(pToken);
                if(addr.inetAddr.sin_addr.s_addr == -1){
                        ca_printf( 
				"%s: Parsing '%s'\n",
                                __FILE__,
                                pEnv->name);
                        ca_printf( 
				"\tBad internet address format: '%s'\n",
                                pToken);
			continue;
		}

                pNode = (caAddrNode *) calloc(1,sizeof(*pNode));
                if(pNode){
                	pNode->destAddr.inetAddr = addr.inetAddr;
                	pNode->srcAddr.inetAddr = localAddr.inetAddr;
                	LOCK;
                	ellAdd(pList, &pNode->node);
                	UNLOCK;
                }
        }

	return;
}



/*
 * getToken()
 */
#ifdef __STDC__
LOCAL char *getToken(char **ppString)
#else /*__STDC__*/
LOCAL char *getToken(ppString)
char **ppString;
#endif /*__STDC__*/
{
        char *pToken;
        char *pStr;

        pToken = *ppString;
        while(isspace(*pToken)&&*pToken){
                pToken++;
        }

        pStr = pToken;
        while(!isspace(*pStr)&&*pStr){
                pStr++;
        }

        if(isspace(*pStr)){
                *pStr = '\0';
                *ppString = pStr+1;
        }
        else{
                *ppString = pStr;
                assert(*pStr == '\0');
        }

        if(*pToken){
                return pToken;
        }
        else{
                return NULL;
        }
}


/*
 * caPrintAddrList()
 */
#ifdef __STDC__
void caPrintAddrList(ELLLIST *pList)
#else /*__STDC__*/
void caPrintAddrList(pList)
ELLLIST *pList;
#endif /*__STDC__*/
{
        caAddrNode              *pNode;

        printf("Channel Access Address List\n");
        pNode = (caAddrNode *) ellFirst(pList);
        while(pNode){
                if(pNode->destAddr.sockAddr.sa_family != AF_INET){
                        printf("<addr entry not in internet format>");
                        continue;
                }
                printf(	"%s\n", 
			inet_ntoa(pNode->destAddr.inetAddr.sin_addr));

                pNode = (caAddrNode *) ellNext(&pNode->node);
        }
}

