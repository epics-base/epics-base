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

static char *sccsId = "@(#) $Id$";


/*	Allocate storage for global variables in this module		*/
#define		CA_GLBLSOURCE
#include	"iocinf.h"
#include	"net_convert.h"

LOCAL void 	tcp_recv_msg(struct ioc_in_use *piiu);
LOCAL void 	cac_tcp_send_msg_piiu(struct ioc_in_use *piiu);
LOCAL void 	cac_udp_send_msg_piiu(struct ioc_in_use *piiu);
LOCAL void 	udp_recv_msg(struct ioc_in_use *piiu);
LOCAL void 	ca_process_tcp(struct ioc_in_use *piiu);
LOCAL void 	ca_process_udp(struct ioc_in_use *piiu);
LOCAL void 	cacRingBufferInit(struct ca_buffer *pBuf, unsigned long size);
LOCAL char	*getToken(char **ppString);





/*
 *	ALLOC_IOC()
 *
 * 	allocate and initialize an IOC info block for unallocated IOC
 *
 */
int alloc_ioc(
struct in_addr			*pnet_addr,
struct ioc_in_use		**ppiiu
)
{
  	int			status;
	bhe			*pBHE;

	/*
	 * look for an existing connection
	 */
	LOCK;
	pBHE = lookupBeaconInetAddr(pnet_addr);
	if(!pBHE){
		pBHE = createBeaconHashEntry(pnet_addr);
		if(!pBHE){
			UNLOCK;
			return ECA_ALLOCMEM;
		}
	}

	if(pBHE->piiu){
		if(pBHE->piiu->conn_up){
			*ppiiu = pBHE->piiu;
			status = ECA_NORMAL;
		}
		else{
			status = ECA_DISCONN;
		}
	}
	else{
  		status = create_net_chan(
				ppiiu, 
				pnet_addr, 
				IPPROTO_TCP);
		if(status == ECA_NORMAL){
			pBHE->piiu = *ppiiu;
		}
	}

	UNLOCK;

  	return status;
}


/*
 *	CREATE_NET_CHANNEL()
 *
 */
int create_net_chan(
struct ioc_in_use 	**ppiiu,
struct in_addr		*pnet_addr,	/* only used by TCP connections */
int			net_proto
)
{
	struct ioc_in_use	*piiu;
  	int			status;
  	SOCKET			sock;
  	int			true = TRUE;
  	struct sockaddr_in	saddr;
	caAddrNode		*pNode;

	LOCK;

	piiu = (IIU *) calloc(1, sizeof(*piiu));
	if(!piiu){
		UNLOCK;
		return ECA_ALLOCMEM;
	}

	piiu->pcas = ca_static;
	ellInit(&piiu->chidlist);
	ellInit(&piiu->destAddr);

  	piiu->sock_proto = net_proto;

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
      		memset((char *)&pNode->destAddr,0,sizeof(pNode->destAddr));
  		pNode->destAddr.inetAddr.sin_family = AF_INET;
		pNode->destAddr.inetAddr.sin_addr = *pnet_addr;
  		pNode->destAddr.inetAddr.sin_port = 
			htons (ca_static->ca_server_port);
		ellAdd(&piiu->destAddr, &pNode->node);
		piiu->recvBytes = tcp_recv_msg; 
		piiu->sendBytes = cac_tcp_send_msg_piiu; 
		piiu->procInput = ca_process_tcp; 
		piiu->minfreespace = 1;	

      		/* 	allocate a socket	*/
      		sock = socket(	AF_INET,	/* domain	*/
				SOCK_STREAM,	/* type		*/
				0);		/* deflt proto	*/
      		if(sock == INVALID_SOCKET){
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
				sizeof(true));
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
		{
			int i;
			int size;

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
				socket_close(sock);
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
				socket_close(sock);
				UNLOCK;
				return ECA_SOCK;
			}

			/* fetch the TCP send buffer size */
			i = sizeof(size); 
			status = getsockopt(
					sock,
					SOL_SOCKET,
					SO_SNDBUF,
					(char *)&size,
					&i);
			if(status < 0 || i != sizeof(size)){
				free(piiu);
				socket_close(sock);
				UNLOCK;
				return ECA_SOCK;
			}
		}
#endif

      		/* connect */
      		status = connect(	
				sock,
				&pNode->destAddr.sockAddr,
				sizeof(pNode->destAddr.sockAddr));
      		if(status < 0){
			ca_printf("CAC: no conn err=\"%s\"\n", strerror(MYERRNO));
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
		 * Save the Host name for efficient access in the
		 * future.
		 */
		caHostFromInetAddr(
			pnet_addr,
			piiu->host_name_str,
			sizeof(piiu->host_name_str));

		cac_gettimeval (&piiu->timeAtLastRecv);

      		break;

    	case	IPPROTO_UDP:
	
		assert(!pnet_addr);
		piiu->recvBytes = udp_recv_msg; 
		piiu->sendBytes = cac_udp_send_msg_piiu; 
		piiu->procInput = ca_process_udp; 
		piiu->minfreespace = MAX_UDP+2*sizeof(struct udpmsglog);	

      		/* 	allocate a socket			*/
      		sock = socket(	AF_INET,	/* domain	*/
				SOCK_DGRAM,	/* type		*/
				0);		/* deflt proto	*/
      		if(sock == INVALID_SOCKET){
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
        		ca_printf("CAC: sso (err=\"%s\")\n",
				strerror(MYERRNO));
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
        		ca_printf("CAC: bind (err=%s)\n",strerror(MYERRNO));
			ca_signal(ECA_INTERNAL,"bind failed");
      		}

		/*
		 * load user and auto configured
		 * broadcast address list
		 */
		caSetupBCastAddrList(
			&piiu->destAddr, 
			sock, 
			ca_static->ca_server_port);


		cacRingBufferInit(&piiu->recv, sizeof(piiu->send.buf));
		cacRingBufferInit(&piiu->send, min(MAX_UDP, 
			sizeof(piiu->send.buf)));

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

	/*	
	 * Set non blocking IO  
	 * to prevent dead locks	
	 */
	status = socket_ioctl(
			piiu->sock_chan,
			FIONBIO,
			&true);
	if(status<0){
		ca_printf(
			"Error setting non-blocking io: %s\n",
			strerror(MYERRNO));
	}

  	if(fd_register_func){
		LOCKEVENTS;
		(*fd_register_func)(fd_register_arg, sock, TRUE);
		UNLOCKEVENTS;
	}

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
 * caSetupBCastAddrList()
 */
void caSetupBCastAddrList (ELLLIST *pList, SOCKET sock, unsigned port)
{
	char			*pstr;
	ENV_PARAM		yesno;
	int			yes;

	/*
	 * dont load the list twice
	 */
	assert (ellCount(pList)==0);

	/*
	 * Check to see if the user has disabled
	 * initializing the search b-cast list
	 * from the interfaces found.
	 */
	yes = TRUE;
	pstr = envGetConfigParam (
			&EPICS_CA_AUTO_ADDR_LIST,		
			sizeof(yesno.dflt),
			yesno.dflt);
	if (pstr) {
			if (strstr(pstr,"no")||strstr(pstr,"NO")) {
			yes = FALSE;
		}
	}

	/*
	 * LOCK is for piiu->destAddr list
	 * (lock outside because this is used by the server also)
	 */
	if (yes) {
		caDiscoverInterfaces(
			pList,
			sock,
			port);
	}

	caAddConfiguredAddr(
		pList, 
		&EPICS_CA_ADDR_LIST,
		sock,
		port);

	if (ellCount(pList)==0) {
		ca_signal (ECA_NOSEARCHADDR, NULL);
	}
}


/*
 *	NOTIFY_CA_REPEATER()
 *
 *	tell the cast repeater that another client needs fan out
 *
 *	NOTES:
 *	1)	local communication only (no LAN traffic)
 *
 */
void notify_ca_repeater()
{
	struct extmsg   	msg;
	struct sockaddr_in	saddr;
	int			status;
	static int		once = FALSE;

	if (ca_static->ca_repeater_contacted) {
		return;
	}

	if (!piiuCast) {
		return;
	}

	if (!piiuCast->conn_up) {
		return;
	}

	if (ca_static->ca_repeater_tries>N_REPEATER_TRIES_PRIOR_TO_MSG){
		if (!once) {
			ca_printf(
		"Unable to contact CA repeater after %d tries\n",
			N_REPEATER_TRIES_PRIOR_TO_MSG);
			ca_printf(
		"Silence this message by starting a CA repeater daemon\n");
			once = TRUE;
		}
	}

	LOCK; /*MULTINET TCP/IP routines are not reentrant*/
     	status = local_addr(piiuCast->sock_chan, &saddr);
	if (status == OK) {
		int	len;

		memset((char *)&msg, 0, sizeof(msg));
		msg.m_cmmd = htons(REPEATER_REGISTER);
		msg.m_available = saddr.sin_addr.s_addr;
      		saddr.sin_port = htons(ca_static->ca_repeater_port);	

		/*
		 * Intentionally sending a zero length message here
		 * until most CA repeater daemons have been restarted
		 * (and only then will accept the above protocol)
		 * (repeaters began accepting this protocol
		 * starting with EPICS 3.12)
		 *
		 * SOLARIS will not accept a zero length message
		 * and we are just porting there for 3.12 so
		 * we will use the new protocol for 3.12
		 */
#		ifdef SOLARIS 
			len = sizeof(msg);
# 		else /* SOLARIS */
			len = 0;
#		endif /* SOLARIS */

      		status = sendto(
			piiuCast->sock_chan,
        		(char *)&msg, /* UCX requires a valid address here */
        		len,  
        		0,
       			(struct sockaddr *)&saddr, 
			sizeof(saddr));
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
		else{
			ca_static->ca_repeater_tries++;
		}
	}
	UNLOCK;
}



/*
 *	CAC_UDP_SEND_MSG_PIIU()
 *
 */
LOCAL void cac_udp_send_msg_piiu(struct ioc_in_use *piiu)
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
		unsigned long	actualSendCnt;

		status = sendto(
				piiu->sock_chan,
				&piiu->send.buf[piiu->send.rdix],	
				sendCnt,
				0,
				&pNode->destAddr.sockAddr,
				sizeof(pNode->destAddr.sockAddr));
		if(status<0){
			int	localErrno;

			localErrno = MYERRNO;

			if(	localErrno == EWOULDBLOCK && 
				localErrno == ENOBUFS && 
				localErrno == EINTR){
				UNLOCK;
				return;
			}
			else {
				ca_printf(
					"CAC: error on socket send() %s\n",
					strerror(localErrno));
			}

			TAG_CONN_DOWN(piiu);
			break;
		}
		actualSendCnt = (unsigned long) status;
		assert (actualSendCnt == sendCnt);
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
LOCAL void cac_tcp_send_msg_piiu(struct ioc_in_use *piiu)
{
	unsigned long	sendCnt;
  	int		status;
	int		localError;

	/*
	 * check for shutdown in progress
	 */
	if(!piiu->conn_up){
		return;
	}

	LOCK;


	/*
	 * Check at least twice to see if there is anything
	 * in the ring buffer (in case the block of messages
	 * isnt continuous). Always return if the send was 
	 * less bytes than requested.
	 */
	while (TRUE) {
		sendCnt = cacRingBufferReadSize(&piiu->send, TRUE);
		assert(sendCnt<=piiu->send.max_msg);

		/*
		 * return if nothing to send
		 */
		if(sendCnt == 0){
			piiu->sendPending = FALSE;
			piiu->send_needed = FALSE;
			UNLOCK;
			return;
		}

		status = send(
				piiu->sock_chan,
				&piiu->send.buf[piiu->send.rdix],	
				sendCnt,
				0);
		if (status<0) {
			break;
		}
		else if (status==0) {
			TAG_CONN_DOWN(piiu);
			UNLOCK;
			return;
		}

		CAC_RING_BUFFER_READ_ADVANCE(&piiu->send, status);

		if (status != sendCnt) {
			UNLOCK;
			return;
		}
	}

	localError = MYERRNO;

	if(	localError == EWOULDBLOCK ||
		localError == ENOBUFS ||
		localError == EINTR){
			UNLOCK;
			return;
	}

	if(	localError != EPIPE && 
		localError != ECONNRESET &&
		localError != ETIMEDOUT){
		ca_printf(	
			"CAC: error on socket send() %s\n",
			strerror(localError));
	}

	TAG_CONN_DOWN(piiu);
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
void cac_flush_internal()
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
 * cac_clean_iiu_list()
 */
void cac_clean_iiu_list()
{
	IIU *piiu;

	LOCK;

	piiu=(IIU *)iiuList.node.next;
	while(piiu){
		if(!piiu->conn_up){
			IIU *pnextiiu;

			pnextiiu = (IIU *)piiu->node.next;
			close_ioc(piiu);
			piiu = pnextiiu;
			continue;
		}
		piiu=(IIU *)piiu->node.next;
	}

	UNLOCK;
}


/*
 * ca_process_input_queue()
 */
void ca_process_input_queue()
{
	struct ioc_in_use 	*piiu;

	LOCK;

	/*
	 * dont allow recursion
	 */
	if(post_msg_active){
		UNLOCK;
		return;
	}

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
LOCAL void tcp_recv_msg(struct ioc_in_use *piiu)
{
	unsigned long	writeSpace;
  	int		status;

	if(!piiu->conn_up){
		return;
	}

	LOCK;

        /*
         * Check at least twice to see if there is ana space left
         * in the ring buffer (in case the messages block
         * isnt continuous). Always return if the send was
         * less bytes than requested.
         */
        while (TRUE) {

		writeSpace = cacRingBufferWriteSize(&piiu->recv, TRUE);
		if(writeSpace == 0){
			break;
		}

		status = recv(	piiu->sock_chan,
				&piiu->recv.buf[piiu->recv.wtix],
				writeSpace,
				0);
		if(status == 0){
			TAG_CONN_DOWN(piiu);
			break;
		}
		else if(status <0){
			/* try again on status of -1 and no luck this time */
			if(MYERRNO == EWOULDBLOCK || MYERRNO == EINTR){
				break;
			}

			if(	MYERRNO != EPIPE && 
				MYERRNO != ECONNRESET &&
				MYERRNO != ETIMEDOUT){
				ca_printf(	
					"CAC: unexpected recv error (err=%s)\n",
					strerror(MYERRNO));
			}
			TAG_CONN_DOWN(piiu);
			break;
		}

		assert (status<=writeSpace);

		CAC_RING_BUFFER_WRITE_ADVANCE(&piiu->recv, status);

		/*
		 * Record the time whenever we receive a message 
		 * from this IOC
		 */
		piiu->timeAtLastRecv = ca_static->currentTime;

		if (status != writeSpace) {
			break;
		}
	}


	UNLOCK;
	return;
}


/*
 * ca_process_tcp()
 *
 */
LOCAL void ca_process_tcp(struct ioc_in_use *piiu)
{
	caAddrNode	*pNode;
	int		status;
	long		bytesToProcess;

	LOCK;

	/*
	 * dont allow recursion
	 */
	if(post_msg_active){
		UNLOCK;
		return;
	}

	post_msg_active = TRUE;

	pNode = (caAddrNode *) piiu->destAddr.node.next;

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
			TAG_CONN_DOWN(piiu);
			post_msg_active = FALSE;
			UNLOCK;
			return;
		}
		CAC_RING_BUFFER_READ_ADVANCE(
				&piiu->recv,
				bytesToProcess);
	}

	post_msg_active = FALSE;
	UNLOCK;

    	flow_control(piiu);

 	return;
}



/*
 *	UDP_RECV_MSG()
 *
 */
LOCAL void udp_recv_msg(struct ioc_in_use *piiu)
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
		ca_printf("Unexpected UDP failure %s\n", strerror(MYERRNO));
		TAG_CONN_DOWN(piiu);
    	}
	else if(status > 0){
		unsigned long		bytesActual;

		/*	
	 	 * log the msg size
		 * and advance the ring index
	 	 */
		pmsglog->nbytes = status;
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
				status);
#		endif
	}

	UNLOCK;

	return;
}



/*
 *	CA_PROCESS_UDP()
 *
 */
LOCAL void ca_process_udp(struct ioc_in_use *piiu)
{
  	int			status;
	struct udpmsglog	*pmsglog;
	char			*pBuf;
	unsigned long		bytesAvailable;

	LOCK;

	/*
	 * dont allow recursion
	 */
	if(post_msg_active){
		UNLOCK;
		return;
	}

	post_msg_active = TRUE;

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
					post_msg_active = FALSE;
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

	post_msg_active = FALSE;
	UNLOCK;

  	return; 
}


/*
 *
 *	CLOSE_IOC()
 *
 *	set an iiu in the disconnected state
 *
 *
 */
void close_ioc (struct ioc_in_use *piiu)
{
	caAddrNode	*pNode;
  	chid		chix;
	int		status;

	/*
	 * dont close twice
	 */
  	assert (piiu->sock_chan!=INVALID_SOCKET);

	LOCK;

	ellDelete (&iiuList, &piiu->node);

	/*
	 * attempt to clear out messages in recv queue
	 */
	(*piiu->procInput) (piiu);


	if (piiu == piiuCast) {
		piiuCast = NULL;
	}
	else {
		/*
		 * remove IOC from the hash table
		 */
		pNode = (caAddrNode *) piiu->destAddr.node.next;
		assert (pNode);
		removeBeaconInetAddr (&pNode->destAddr.inetAddr.sin_addr);

		/*
		 * Mark all of their channels disconnected
		 * prior to calling handlers incase the
		 * handler tries to use a channel before
		 * I mark it disconnected.
		 */
		chix = (chid) &piiu->chidlist.node.next;
		while (chix = (chid) chix->node.next) {
			chix->type = TYPENOTCONN;
			chix->count = 0U;
			chix->state = cs_prev_conn;
			chix->id.sid = ~0U;
			chix->ar.read_access = FALSE;
			chix->ar.write_access = FALSE;
			/*
			 * try to reconnect
			 */
			chix->retry = 0U;
		}

		if (piiu->chidlist.count) {
			ca_signal (ECA_DISCONN,piiu->host_name_str);
		}

		/*
		 * clear outstanding get call backs 
		 */
		caIOBlockListFree (&pend_read_list, chix, TRUE, ECA_DISCONN);

		/*
		 * clear outstanding put call backs 
		 */
		caIOBlockListFree (&pend_write_list, chix, TRUE, ECA_DISCONN);

		/*
		 * call their connection handler as required
		 */
		chix = (chid) &piiu->chidlist.node.next;
		while (chix = (chid) chix->node.next) {
			LOCKEVENTS;
			if (chix->pConnFunc) {
				struct connection_handler_args 	args;

				args.chid = chix;
				args.op = CA_OP_CONN_DOWN;
				(*chix->pConnFunc) (args);
			}
			if (chix->pAccessRightsFunc) {
				struct access_rights_handler_args 	args;

				args.chid = chix;
				args.ar = chix->ar;
				(*chix->pAccessRightsFunc) (args);
			}
			UNLOCKEVENTS;
			chix->piiu = piiuCast;
		}

		/*
		 * move all channels to the broadcast IIU
		 *
		 * if we loose the broadcast IIU its a severe error
		 */
		assert (piiuCast);
		ellConcat (&piiuCast->chidlist, &piiu->chidlist);
  		assert (piiu->chidlist.count==0);
	}

	/*
	 * Try to reconnect
	 */
	ca_static->ca_search_retry = 0;

  	if (fd_register_func) {
		LOCKEVENTS;
		(*fd_register_func) (fd_register_arg, piiu->sock_chan, FALSE);
		UNLOCKEVENTS;
	}

  	status = socket_close (piiu->sock_chan);
	assert (status == 0);

	/*
	 * free message body cache
	 */
	if (piiu->pCurData) {
		free (piiu->pCurData);
		piiu->pCurData = NULL;
		piiu->curDataMax = 0;
	}

  	piiu->sock_chan = INVALID_SOCKET;

	ellFree (&piiu->destAddr);

	free (piiu);

	UNLOCK;
}



/*
 *	REPEATER_INSTALLED()
 *
 *	Test for the repeater already installed
 *
 *	NOTE: potential race condition here can result
 *	in two copies of the repeater being spawned
 *	however the repeater detects this, prints a message,
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
  	int			status;
  	SOCKET			sock;
  	struct sockaddr_in	bd;
	int			true = 1;
	int 			installed = FALSE;

	LOCK;

     	/* 	allocate a socket			*/
      	sock = socket(	AF_INET,	/* domain	*/
			SOCK_DGRAM,	/* type		*/
			0);		/* deflt proto	*/
      	if(sock == INVALID_SOCKET) {
		UNLOCK;
		return installed;
	}

      	memset((char *)&bd,0,sizeof bd);
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = INADDR_ANY;	
     	bd.sin_port = htons(ca_static->ca_repeater_port);	
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
	 * this works on kernels that support multicast
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
unsigned long cacRingBufferRead(
struct ca_buffer	*pRing,
void			*pBuf,
unsigned long		nBytes)
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
unsigned long cacRingBufferWrite(
struct ca_buffer	*pRing,
void			*pBuf,
unsigned long		nBytes)
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
LOCAL void cacRingBufferInit(struct ca_buffer *pBuf, unsigned long size)
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
unsigned long cacRingBufferReadSize(struct ca_buffer *pBuf, int contiguous)
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
unsigned long cacRingBufferWriteSize(struct ca_buffer *pBuf, int contiguous)
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

	return count;
}



/*
 * localHostName()
 *
 * o Indicates failure by setting ptr to nill
 *
 * o Calls non posix gethostbyname() so that we get DNS style names
 *      (gethostbyname() should be available with most BSD sock libs)
 *
 * vxWorks user will need to configure a DNS format name for the
 * host name if they wish to be cnsistent with UNIX and VMS hosts.
 *
 * this needs to attempt to determine if the process is a remote 
 * login - hard to do under UNIX
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
void caAddConfiguredAddr(ELLLIST *pList, ENV_PARAM *pEnv, 
	SOCKET socket, int port)
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
      		memset((char *)&addr,0,sizeof(addr));
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
LOCAL char *getToken(char **ppString)
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
void caPrintAddrList(ELLLIST *pList)
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


/*
 * caFetchPortConfig()
 */
int caFetchPortConfig(ENV_PARAM *pEnv, int defaultPort)
{
	long		longStatus;
	long		epicsParam;
	int 		port;

	longStatus = envGetLongConfigParam(pEnv, &epicsParam);
	if (longStatus!=0) {
		epicsParam = defaultPort;
		ca_printf ("EPICS \"%s\" integer fetch failed\n", pEnv->name);
		ca_printf ("setting \"%s\" = %ld\n", pEnv->name, epicsParam);
	}

	/*
	 * This must be a server port that will fit in a signed
	 * short
	 */
	if (epicsParam<=IPPORT_USERRESERVED || epicsParam>SHRT_MAX) {
		ca_printf ("EPICS \"%s\" out of range\n", pEnv->name);
		/*
		 * Quit if the port is wrong due CA coding error
		 */
		assert (epicsParam != defaultPort);
		epicsParam = defaultPort;
		ca_printf ("Setting \"%s\" = %ld\n", pEnv->name, epicsParam);
	}

	/*
	 * ok to clip to int here because we checked the range
	 */
	port = (int) epicsParam;

	return port;
}

