/*
 * $Id$
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

static char *sccsId = "$Id$";

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

LOCAL int 	clean_client(struct one_client *pclient, int sock);


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
  	int 				sock;
  	int 				bindsock;
	int				true = 1;
	struct sockaddr_in		from;
  	struct sockaddr_in		bd;
  	struct sockaddr_in		local;
  	int				from_size = sizeof from;
  	struct one_client		*pclient;
  	struct one_client		*pnxtclient;

	ellInit(&client_list);

     	/* 	allocate a socket			*/
      	sock = socket(	AF_INET,	/* domain	*/
			SOCK_DGRAM,	/* type		*/
			0);		/* deflt proto	*/
      	assert(sock >= 0);

     	/* 	allocate a socket			*/
      	bindsock = socket(	AF_INET,	/* domain	*/
			SOCK_DGRAM,	/* type		*/
			0);		/* deflt proto	*/
      	assert(bindsock >= 0);

      	memset((char *)&bd,0,sizeof(bd));
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = INADDR_ANY;	
     	bd.sin_port = htons(CA_CLIENT_PORT);	
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

    		size = recvfrom(	
					sock,
					buf,
					sizeof buf,
					0,
					(struct sockaddr *)&from, 
					&from_size);

   	 	if(size > 0){
			if(from.sin_addr.s_addr != local.sin_addr.s_addr)
				for(	pclient = (struct one_client *) 
						client_list.node.next;
					pclient;
					pclient = (struct one_client *) 
						pclient->node.next){

    					status = sendto(
						sock,
          	                      		buf,
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
		else if(size == 0){
			struct extmsg	confirm;

			/*
			 * If this is a processor local message then add to
			 * the list of clients to repeat to if not there
			 * already
			 */
			for(	pclient = (struct one_client *) 
					client_list.node.next;
				pclient;
				pclient = (struct one_client *) 
						pclient->node.next)
				if(from.sin_port == pclient->from.sin_port)
					break;
		
			if(!pclient){
				pclient = (struct one_client *) 
					calloc(1,sizeof *pclient);
				if(pclient){
					pclient->from = from;
					ellAdd(	&client_list, 
						&pclient->node);
#ifdef DEBUG
					ca_printf(
						"Added %x %d\n", 
						from.sin_port, size);
#endif
				}
			}

   			memset((char *)&confirm, 0, sizeof confirm);
			confirm.m_cmmd = htons(REPEATER_CONFIRM);
			confirm.m_available = local.sin_addr.s_addr;
        		status = sendto(
				sock,
        			(char *)&confirm,
        			sizeof confirm,
        			0,
       				(struct sockaddr *)&from, /* back to sender */
				sizeof from);
      			if(status != sizeof confirm){
				ca_printf("CA Repeater: confirm err %s\n",
					strerror(MYERRNO));
			}
		}
		else{
			ca_printf("CA Repeater: recv err %s\n",
				strerror(MYERRNO));
		}

		/* remove any dead wood prior to pending */
		for(	pclient = (struct one_client *) 
				client_list.node.next;
			pclient;
			pclient = pnxtclient){
			/* do it now in case item deleted */
			pnxtclient = (struct one_client *) 
						pclient->node.next;	
			clean_client(pclient, bindsock);
		}
	}
}


/*
 *
 *	check to see if this client is still around
 *
 */
LOCAL int clean_client(struct one_client *pclient, int sock)
{
	int			port = pclient->from.sin_port;
  	struct sockaddr_in	bd;
	int			status;
	int			present = FALSE;

      	memset((char *)&bd,0,sizeof bd);
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = INADDR_ANY;	
     	bd.sin_port = port;	
      	status = bind(sock, (struct sockaddr *)&bd, sizeof bd);
     	if(status<0){
		if(MYERRNO == EADDRINUSE)
			present = TRUE;
		else{
			ca_printf("CA Repeater: client cleanup err %s\n",
				strerror(MYERRNO));
		}
	}

	if(!present){
		ellDelete(&client_list, &pclient->node);
		free(pclient);
#ifdef DEBUG
		ca_printf("Deleted\n");
#endif
	}

	return OK;
}
