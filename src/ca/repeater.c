/*
 * $Id$
 *
 * REPEATER.C
 *
 * CA broadcast repeater
 *
 * Author: 	Jeff Hill
 * Date:	3-27-90
 *
 *	Control System Software for the GTA Project
 *
 *	Copyright 1988, 1989, the Regents of the University of California.
 *
 *	This software was produced under a U.S. Government contract
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory, which is
 *	operated by the University of California for the U.S. Department
 *	of Energy.
 *
 *	Developed by the Controls and Automation Group (AT-8)
 *	Accelerator Technology Division
 *	Los Alamos National Laboratory
 *
 *	Direct inqueries to:
 *	Jeff HIll, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 665-1831
 *	E-mail: johill@lanl.gov
 *
 *	PURPOSE:
 *	Broadcasts fan out over the LAN, but old IP kernels do not allow
 *	two processes on the same machine to get the same broadcast
 *	(and modern IP kernels do not allow two processes on the same machine
 *	to receive the same unicast).
 *
 *	This code fans out UDP messages sent to the CA repeater port
 *	to all CA client processes that have subscribed.
 *
 *
 * 	Modification Log:
 * 	-----------------
 *	.01 060691 joh	Took out 4 byte count at message begin to 
 *			in preparation for SPARC alignment
 *	.02 042892 joh	No longer checking the status from free
 *			since it varies from OS to OS
 *	.03 042892 joh	made local routines static
 *	.04 072392 joh	set reuse addr socket option so that
 *			the repeater will restart if it gets killed
 *	.05 072392 joh	no longer needs to loop waiting for the timeout
 *			to expire because of the change introduced 
 *			in .04
 *	.06 120492 joh	removed unnecessary includes
 *	.07 120992 joh	now uses dll list routines
 *	.08 102993 joh	toggle set sock opt to set
 *	.09 070195 joh  discover client has vanished by connecting its
 *			datagram socket (and watching for ECONNREFUSED)
 *
 * $Log$
 * Revision 1.43  1998/04/15 21:58:29  jhill
 * fixed the doc
 *
 * Revision 1.42  1998/02/27 01:04:03  jhill
 * fixed benign WIN32 message from overwritten errno
 *
 * Revision 1.41  1998/02/05 22:34:33  jhill
 * dont delete client if send returns ECONNREFUSED
 *
 * Revision 1.40  1997/08/04 23:37:15  jhill
 * added beacon anomaly flag init/allow ip 255.255.255.255
 *
 * Revision 1.39  1997/04/23 17:05:09  jhill
 * pc port changes
 *
 * Revision 1.38  1996/11/02 00:51:04  jhill
 * many pc port, const in API, and other changes
 *
 * Revision 1.37  1996/09/04 20:02:32  jhill
 * fixed gcc warning
 *
 * Revision 1.36  1996/07/12 00:40:48  jhill
 * fixed client disconnect problem under solaris
 *
 * Revision 1.32.6.1  1996/07/12 00:39:59  jhill
 * fixed client disconnect problem under solaris
 *
 * Revision 1.35  1996/07/10 23:30:11  jhill
 * fixed GNU warnings
 *
 * Revision 1.34  1996/06/19 17:59:24  jhill
 * many 3.13 beta changes
 *
 * Revision 1.33  1995/11/29  19:19:05  jhill
 * Added $log$
 *
 */

/*
 * It would be preferable to avoid using the repeater on multicast enhanced IP kernels, but
 * this is not going to work in all situations because (according to Steven's TCP/IP
 * illustrated volume I) if a broadcast is received it goes to all sockets on the same port,
 * but if a unicast is received it goes to only one of the sockets on the same port
 * (we can only guess at which one it will be).
 *   
 * I have observed this behavior under winsock II:
 * o only one of the sockets on the same port receives the message if we send to the 
 * loop back address
 * o both of the sockets on the same port receives the message if we send to the 
 * broadcast address
 * 
 */

static char *sccsId = "@(#)$Id$";

#include	"iocinf.h"

/*
 * one socket per client so we will get the ECONNREFUSED
 * error code (and then delete the client)
 */
struct one_client{
	ELLNODE			node;
	struct sockaddr_in 	from;
	SOCKET			sock;
};

/* 
 *	these can be external since there is only one instance
 *	per machine so we dont care about reentrancy
 */
static ELLLIST	client_list;

static char	buf[ETHERNET_MAX_UDP]; 

#define PORT_ANY 0U
typedef struct {
	SOCKET sock;
	int errNumber;
	const char *pErrStr;
}makeSocketReturn;

LOCAL void register_new_client(struct sockaddr_in *pLocal, 
					struct sockaddr_in *pFrom);
LOCAL void verifyClients();
LOCAL makeSocketReturn makeSocket (unsigned short port, int reuseAddr);
LOCAL void fanOut(struct sockaddr_in *pFrom, const char *pMsg, unsigned msgSize);


/*
 *
 *	ca_repeater()
 *
 *
 */
void epicsShareAPI ca_repeater()
{
  	int status;
  	int size;
  	SOCKET sock;
	struct sockaddr_in from;
  	struct sockaddr_in local;
  	int from_size = sizeof from;
	unsigned short port;
	makeSocketReturn msr;

	port = caFetchPortConfig(
		&EPICS_CA_REPEATER_PORT, 
		CA_REPEATER_PORT);

	ellInit(&client_list);

	msr = makeSocket(port, TRUE);
	if (msr.sock==INVALID_SOCKET) {
		/*
		 * test for server was already started
		 */
		if (msr.errNumber==SOCK_EADDRINUSE) {
			exit(0);
		}
		ca_printf("%s: Unable to create repeater socket because %d=\"%s\"\n",
			__FILE__,
			msr.errNumber,
			msr.pErrStr);
		exit(0);
	}

	sock = msr.sock;

	status = local_addr(sock, &local);
	if(status != OK){
		ca_printf(
	"CA Repeater: failed during initialization - no local IP address\n");
		exit (0);
	}

#ifdef DEBUG
	ca_printf("CA Repeater: Attached and initialized\n");
#endif

	while(TRUE){
		caHdr	*pMsg;

    		size = recvfrom(	
				sock,
				buf,
				sizeof(buf),
				0,
				(struct sockaddr *)&from, 
				&from_size);

   	 	if(size < 0){
#			ifdef linux
				/*
				 * Avoid spurious ECONNREFUSED bug
				 * in linux
				 */
				if (SOCKERRNO==SOCK_ECONNREFUSED) {
					continue;
				}
#			endif
			ca_printf("CA Repeater: recv err %s\n",
				SOCKERRSTR);
			continue;
		}

		pMsg = (caHdr *) buf;

		/*
		 * both zero length message and a registration message
		 * will register a new client
		 */
		if( ((size_t)size) >= sizeof(*pMsg)){
			if(ntohs(pMsg->m_cmmd) == REPEATER_REGISTER){
				register_new_client(&local, &from);

				/*
				 * strip register client message
				 */
				pMsg++;
				size -= sizeof(*pMsg);
				if (size==0) {
					continue;
				}
			}
		}
		else if(size == 0){
			register_new_client(&local, &from);
			continue;
		}

		fanOut(&from, (char *) pMsg, size);
	}
}


/*
 * fanOut()
 */
LOCAL void fanOut(struct sockaddr_in *pFrom, const char *pMsg, unsigned msgSize)
{
	ELLLIST theClients;
  	struct one_client *pclient;
	int status;
	int verify = FALSE;

	ellInit(&theClients);
	while ( (pclient=(struct one_client *)ellGet(&client_list)) ) {
		ellAdd(&theClients, &pclient->node);

		/*
		 * Dont reflect back to sender
		 */
		if(pFrom->sin_port == pclient->from.sin_port &&
		   pFrom->sin_addr.s_addr == pclient->from.sin_addr.s_addr){
			continue;
		}

		status = send(
			pclient->sock,
			(char *)pMsg,
			msgSize,
			0);
		if (status>=0) {
#ifdef DEBUG
			ca_printf ("Sent to %d\n", 
				ntohs (pclient->from.sin_port));
#endif
		}
		if(status < 0){
			if (SOCKERRNO == SOCK_ECONNREFUSED) {
#ifdef DEBUG
				ca_printf ("Deleted client %d\n",
					ntohs (pclient->from.sin_port));
#endif
				verify = TRUE;
			}
			else {
				ca_printf(
"CA Repeater: fan out err was \"%s\"\n",
					SOCKERRSTR);
			}
		}
	}
	ellConcat(&client_list, &theClients);

	if (verify) {
		verifyClients ();
	}
}


/*
 * verifyClients()
 * (this is required because solaris has a half baked version of sockets)
 */
LOCAL void verifyClients()
{
	ELLLIST theClients;
  	struct one_client *pclient;
	makeSocketReturn msr;

	ellInit(&theClients);
	while ( (pclient=(struct one_client *)ellGet(&client_list)) ) {
		ellAdd(&theClients, &pclient->node);

		msr = makeSocket(ntohs(pclient->from.sin_port), FALSE);
		if (msr.sock!=INVALID_SOCKET) {
#ifdef DEBUG
			ca_printf("Deleted client %d\n",
					ntohs(pclient->from.sin_port));
#endif
			ellDelete(&theClients, &pclient->node);
			socket_close(msr.sock);
			socket_close(pclient->sock);
			free(pclient);
		}
		else {
			/*
			 * win sock does not set SOCKERRNO when this fails
			 */
			if (msr.errNumber!=SOCK_EADDRINUSE) {
				ca_printf(
	"CA Repeater: bind test err was %d=\"%s\"\n", 
					msr.errNumber, msr.pErrStr);
			}
		}
	}
	ellConcat(&client_list, &theClients);
}


/*
 * makeSocket()
 */
LOCAL makeSocketReturn makeSocket(unsigned short port, int reuseAddr)
{
	int status;
  	struct sockaddr_in bd;
	makeSocketReturn msr;
	int true = 1;

	msr.sock = socket(	AF_INET,	/* domain	*/
					SOCK_DGRAM,	/* type		*/
					0);		/* deflt proto	*/
	if (msr.sock == INVALID_SOCKET) {
		msr.errNumber = SOCKERRNO;
		msr.pErrStr = SOCKERRSTR;
		return msr;
	}

	/*
	 * no need to bind if unconstrained
	 */
	if (port != PORT_ANY) {

		memset((char *)&bd, 0, sizeof(bd));
		bd.sin_family = AF_INET;
		bd.sin_addr.s_addr = htonl(INADDR_ANY);	
		bd.sin_port = htons(port);	
		status = bind(msr.sock, (struct sockaddr *)&bd, (int)sizeof(bd));
		if (status<0) {
			msr.errNumber = SOCKERRNO;
			msr.pErrStr = SOCKERRSTR;
			socket_close(msr.sock);
			msr.sock = INVALID_SOCKET;
			return msr;
		}
		if (reuseAddr) {
			status = setsockopt(
						msr.sock,	
						SOL_SOCKET,
						SO_REUSEADDR,
						(char *)&true,
						sizeof(true));
			if (status<0) {
				ca_printf(
			"%s: set socket option failed because %d=\"%s\"\n", 
						__FILE__, SOCKERRNO, SOCKERRSTR);
			}
		}
	}

	msr.errNumber = 0;
	msr.pErrStr = "no error";
	return msr;
}


/*
 * register_new_client()
 */
LOCAL void register_new_client(
struct sockaddr_in 	*pLocal, 
struct sockaddr_in 	*pFrom)
{
  	int status;
  	struct one_client *pclient;
	caHdr confirm;
	caHdr noop;
	int newClient = FALSE;
	makeSocketReturn msr;

	if (pFrom->sin_family != AF_INET) {
		return;
	}

	/*
	 * the repeater and its clients must be on the same host
	 */
	if (htonl(INADDR_LOOPBACK) != pFrom->sin_addr.s_addr) {

		/*
		 * Unfortunately on 3.13 beta 11 and before the
		 * repeater would not always allow the loopback address
		 * as a local client address so all clients must continue to
		 * use the address from the first non-loopback interface
		 * found to communicate with the CA repeater until all
		 * CA repeaters have been restarted.
		 */
		if (pLocal->sin_addr.s_addr != pFrom->sin_addr.s_addr) {
			return;
		}
	}

	for(pclient = (struct one_client *) ellFirst(&client_list);
		pclient; pclient = (struct one_client *) ellNext(&pclient->node)){

		if (pFrom->sin_port == pclient->from.sin_port) {
			break;
		}
	}		

	if (!pclient) {
		pclient = (struct one_client *)calloc (1, sizeof(*pclient));
		if (!pclient) {
			ca_printf("%s: no memory for new client\n",
					__FILE__);
			return;
		}

		msr = makeSocket(PORT_ANY, FALSE);
		if (msr.sock==INVALID_SOCKET) {
			free(pclient);
			ca_printf("%s: no client sock because %d=\"%s\"\n",
					__FILE__,
					msr.errNumber,
					msr.pErrStr);
			return;
		}

		pclient->sock = msr.sock;

		status = connect(pclient->sock, 
				(struct sockaddr *)pFrom, 
				sizeof(*pFrom));
		if (status<0) {
			ca_printf(
			"%s: unable to connect client sock because \"%s\"\n",
				__FILE__, SOCKERRSTR);
			socket_close(pclient->sock);
			free(pclient);
			return;
		}

		pclient->from = *pFrom;

		ellAdd (&client_list, &pclient->node);
		newClient = TRUE;
#ifdef DEBUG
		ca_printf (
			"Added %d\n", 
			ntohs(pFrom->sin_port));
#endif
	}

	memset((char *)&confirm, '\0', sizeof(confirm));
	confirm.m_cmmd = htons(REPEATER_CONFIRM);
	confirm.m_available = pFrom->sin_addr.s_addr;
	status = send(
		pclient->sock,
		(char *)&confirm,
		sizeof(confirm),
		0);
	if (status >= 0) {
		assert(status == sizeof(confirm));
	}
	else if (SOCKERRNO == SOCK_ECONNREFUSED){
#ifdef DEBUG
		ca_printf("Deleted repeater client=%d sending ack\n",
				ntohs(pFrom->sin_port));
#endif
		ellDelete(&client_list, &pclient->node);
		socket_close(pclient->sock);
		free(pclient);
	}
	else {
		ca_printf("CA Repeater: confirm err was \"%s\"\n",
				SOCKERRSTR);
	}

	/*
	 * send a noop message to all other clients so that we dont 
	 * accumulate sockets when there are no beacons
	 */
	memset((char *)&noop, '\0', sizeof(noop));
	confirm.m_cmmd = htons(CA_PROTO_NOOP);
	fanOut(pFrom, (char *)&noop, sizeof(noop));

	if (newClient) {
		/*
		 * on solaris we need to verify that the clients
		 * have not gone away (because ICMP does not
		 * get through to send()
		 *
		 * this is done each time that a new client is 
		 * created
		 *
		 * this is done here in order to avoid deleting
		 * a client prior to sending its confirm message
		 */
		verifyClients();
	}
}

