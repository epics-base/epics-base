/*
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
 *	Andy Kozubal, AT-8, Mail Stop H820
 *	Los Alamos National Laboratory
 *	Los Alamos, New Mexico 87545
 *	Phone: (505) 667-6508
 *	E-mail: kozubal@k2.lanl.gov
 *
 *	PURPOSE:
 *	Broadcasts fan out over the LAN, but UDP does not allow
 *	two processes on the same machine to get the same broadcast.
 *	This code takes extends the broadcast model from the net to within
 *	the OS.
 *
 *	NOTES:
 *
 *	This server does not gaurantee the reliability of 
 *	fanned out broadcasts in keeping with the nature of 
 *	broadcasts. Therefore, CA retransmitts lost msgs.
 *
 *	UDP datagrams delivered between two processes on the same 
 *	machine dont travel over the LAN.
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
 *
 */

static char *sccsId = "@(#)$Id$";

#include	"iocinf.h"

/* 
 *	these can be external since there is only one instance
 *	per machine so we dont care about reentrancy
 */
struct one_client{
	ELLNODE			node;
  	struct sockaddr_in	from;
};

static ELLLIST	client_list;

static char	buf[MAX_UDP]; 

LOCAL int 	clean_client(struct one_client *pclient);
LOCAL void 	register_new_client(SOCKET sock, struct sockaddr_in *pLocal, 
					struct sockaddr_in *pFrom);


/*
 *
 *	ca_repeater()
 *
 *
 */
void ca_repeater()
{
  	int				status;
  	int				size;
  	SOCKET				sock;
	int				true = 1;
	struct sockaddr_in		from;
  	struct sockaddr_in		bd;
  	struct sockaddr_in		local;
  	int				from_size = sizeof from;
  	struct one_client		*pclient;
  	struct one_client		*pnxtclient;
	short				port;

	port = caFetchPortConfig(
		&EPICS_CA_REPEATER_PORT, 
		CA_REPEATER_PORT);

	ellInit(&client_list);

     	/* 	allocate a socket			*/
      	sock = socket(	AF_INET,	/* domain	*/
			SOCK_DGRAM,	/* type		*/
			0);		/* deflt proto	*/
      	assert(sock != INVALID_SOCKET);


      	memset((char *)&bd, 0, sizeof(bd));
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = INADDR_ANY;	
     	bd.sin_port = htons(port);	
      	status = bind(sock, (struct sockaddr *)&bd, sizeof(bd));
     	if(status<0){
		if(MYERRNO != EADDRINUSE){
			ca_printf("CA Repeater: unexpected bind fail %s\n", 
				strerror(MYERRNO));
		}
		socket_close(sock);
		exit(0);
	}

	status = setsockopt(	sock,	
				SOL_SOCKET,
				SO_REUSEADDR,
				(char *)&true,
				sizeof(true));
	if(status<0){
		ca_printf(	"%s: set socket option failed\n", 
				__FILE__);
	}

	status = local_addr(sock, &local);
	if(status != OK){
		ca_printf("CA Repeater: no inet interfaces online?\n");
		assert(0);
	}

#ifdef DEBUG
	ca_printf("CA Repeater: Attached and initialized\n");
#endif

	while(TRUE){
		struct extmsg	*pMsg;

    		size = recvfrom(	
				sock,
				buf,
				sizeof(buf),
				0,
				(struct sockaddr *)&from, 
				&from_size);

   	 	if(size < 0){
			ca_printf("CA Repeater: recv err %s\n",
				strerror(MYERRNO));
		}

		pMsg = (struct extmsg *) buf;

		/*
		 * both zero leng message and a registartion message
		 * will register a new client
		 */
		if(size >= sizeof(*pMsg)){
			if(ntohs(pMsg->m_cmmd) == REPEATER_REGISTER){
				register_new_client(sock, &local, &from);

				/*
				 * strip register client message
				 */
				pMsg++;
				size -= sizeof(*pMsg);
			}
		}
		else if(size == 0){
			register_new_client(sock, &local, &from);
		}

		/*
		 * size may have been adjusted above
		 */
		if(size){
			for(	pclient = (struct one_client *) 
					client_list.node.next;
				pclient;
				pclient = (struct one_client *) 
						pclient->node.next){

				/*
				 * Dont reflect back to sender
				 */
				if(from.sin_port == pclient->from.sin_port &&
				   from.sin_addr.s_addr ==
					pclient->from.sin_addr.s_addr){
					continue;
				}

    				status = sendto(
					sock,
                               		(char *)pMsg,
               				size,
          	                   	0,
            	                    	(struct sockaddr *)&pclient->from, 
               	                 	sizeof pclient->from);
   				if(status < 0){
					ca_printf("CA Repeater: fanout err %s\n",
						strerror(MYERRNO));
				}
#ifdef DEBUG
				ca_printf("Sent\n");
#endif
			}
		}

		/* 
		 * remove any dead wood prior to pending 
		 */
		for(	pclient = (struct one_client *) 
				client_list.node.next;
			pclient;
			pclient = pnxtclient){
			/* do it now in case item deleted */
			pnxtclient = (struct one_client *) 
						pclient->node.next;	
			clean_client(pclient);
		}
	}
}


/*
 * register_new_client()
 */
void register_new_client(
SOCKET			sock, 
struct sockaddr_in 	*pLocal, 
struct sockaddr_in 	*pFrom)
{
  	int			status;
  	struct one_client	*pclient;
	struct extmsg		confirm;

	for(	pclient = (struct one_client *) client_list.node.next;
		pclient;
		pclient = (struct one_client *) pclient->node.next){

		if(	pFrom->sin_port == pclient->from.sin_port &&
			pFrom->sin_addr.s_addr ==  pclient->from.sin_addr.s_addr)
			break;
	}		

	if(!pclient){
		pclient = (struct one_client *)calloc (1, sizeof(*pclient));
		if(pclient){
			pclient->from = *pFrom;
			ellAdd (&client_list, &pclient->node);
#ifdef DEBUG
			ca_printf (
				"Added %d\n", 
				pFrom->sin_port);
#endif
		}
	}

	memset((char *)&confirm, 0, sizeof confirm);
	confirm.m_cmmd = htons(REPEATER_CONFIRM);
	confirm.m_available = pLocal->sin_addr.s_addr;
	status = sendto(
		sock,
		(char *)&confirm,
		sizeof(confirm),
		0,
		(struct sockaddr *)pFrom, /* back to sender */
		sizeof(*pFrom));
	if(status != sizeof(confirm)){
		ca_printf("CA Repeater: confirm err %s\n",
				strerror(MYERRNO));
	}
}


/*
 *
 *	check to see if this client is still around
 *
 *	NOTE:
 *	This closes the socket only whenever we are 
 *	able to bind so that we free the port.
 */
LOCAL int clean_client(struct one_client *pclient)
{
	static int		sockExists;
  	static SOCKET		sock;
	int			port = pclient->from.sin_port;
  	struct sockaddr_in	bd;
	int			status;
	int			present = FALSE;

     	/* 	
	 * allocate a socket			
	 * (no lock required because this is implemented with
	 * a single thread)
	 */
	if(!sockExists){
      		sock = socket(	
				AF_INET,	/* domain	*/
				SOCK_DGRAM,	/* type		*/
				0);		/* deflt proto	*/
		if(sock != INVALID_SOCKET){
			sockExists = TRUE;
		}
		else{
			ca_printf("CA Repeater: no bind test sock err %s\n",
				strerror(MYERRNO));
			return OK;
		}
	}

      	memset((char *)&bd, 0, sizeof(bd));
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = INADDR_ANY;	
     	bd.sin_port = port;	
      	status = bind(sock, (struct sockaddr *)&bd, sizeof(bd));
     	if(status>=0){
		socket_close (sock);
		sockExists = FALSE;
	}
	else{
		if(MYERRNO == EADDRINUSE){
			present = TRUE;
		}
		else{
			ca_printf("CA Repeater: client cleanup err %s\n",
				strerror(MYERRNO));
			ca_printf("CA Repeater: sock=%d family=%d addr=%x port=%d\n",
				sock, bd.sin_family, bd.sin_addr.s_addr,
				bd.sin_port);
		}
	}


	if(!present){
#ifdef DEBUG
		ca_printf("Deleted %d\n", pclient->from.sin_port);
#endif
		ellDelete(&client_list, &pclient->node);
		free(pclient);
	}

	return OK;
}
