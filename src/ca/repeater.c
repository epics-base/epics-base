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
 *	broadcasts. Therefore, CA provides a backup.
 *
 *	UDP datagrams delivered between two processes on the same 
 *	machine dont travel over the LAN.
 *
 *
 * Modification Log:
 * -----------------
 */

#include		<vxWorks.h>
#include		<lstLib.h>

#include		<errno.h>
#include		<types.h>
#include		<socket.h>
#include		<in.h>
#include		<ioctl.h>

#include		<iocmsg.h>
#include		<os_depen.h>
struct sockaddr_in	*local_addr();

/* 
	these can be external since there is only one instance
	per machine so we dont care about reentrancy
*/
struct one_client{
	NODE			node;
  	struct sockaddr_in	from;
};

static
LIST	client_list;

static
char	buf[MAX_UDP]; /* bigger than max TCP */


/*
 *
 *	Fan out broadcasts to several processor local tasks
 *
 *
 */
#ifdef VMS
main()
#else
ca_repeater_task()
#endif
{
  	int				status;
  	int				size;
  	int 				sock;
	struct sockaddr_in		from;
  	struct sockaddr_in		bd;
  	struct sockaddr_in		local;
  	int				from_size = sizeof from;
  	struct one_client		*pclient;
  	struct one_client		*pnxtclient;
 	unsigned 			*pcount = (unsigned *)buf;


	lstInit(&client_list);

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
		if(MYERRNO == EADDRINUSE){
			printf("Only one CA repeater thread per host\n");
			exit();
		}
		else
			abort();

	local = *local_addr(sock);

	while(TRUE){

    		size = recvfrom(	
					sock,
					buf,
					sizeof buf,
					0,
					&from, 
					&from_size);

   	 	if(size > 0){
   	 		if(size != ntohl(*pcount))
				printf("ca repeater: corrupt msg ignored\n");
			else if(from.sin_addr.s_addr != local.sin_addr.s_addr)
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
            	                    		&pclient->from, 
               	                 		sizeof pclient->from);
   					if(status < 0)
        					abort();
#ifdef DEBUG
					printf("Sent\n");
#endif
				}
		}
		else if(size == 0){
			struct {
				unsigned	length;
				struct extmsg	extmsg;
			}confirm;

			/*
			 * If this is a processor local message then add to
			 * the list of clients to repeat to if not there
			 * allready
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
					malloc(sizeof *pclient);
				if(pclient){
					pclient->from = from;
					lstAdd(&client_list, pclient);
#ifdef DEBUG
					printf("Added %x %d\n", from.sin_port, size);
#endif
				}
			}

   			memset(&confirm, NULL, sizeof confirm);
			confirm.length = htonl(sizeof confirm);
			confirm.extmsg.m_cmmd = htons(REPEATER_CONFIRM);
			confirm.extmsg.m_available = local.sin_addr.s_addr;
        		status = sendto(
				sock,
        			&confirm,
        			sizeof confirm,
        			0,
       				&from, /* reflect back to sender */
				sizeof from);
      			if(status != sizeof confirm)
				abort();
		}else
			abort();

		/* remove any dead wood prior to pending */
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
 *
 *	check to see if this client is still around
 *
 */
clean_client(pclient)
struct one_client		*pclient;
{
	int			port = pclient->from.sin_port;
	int 			sock;
  	struct sockaddr_in	bd;
	int			status;
	int			present = FALSE;

     	/* 	allocate a socket			*/
      	sock = socket(	AF_INET,	/* domain	*/
			SOCK_DGRAM,	/* type		*/
			0);		/* deflt proto	*/
      	if(sock == ERROR)
        	abort();

      	memset(&bd,0,sizeof bd);
      	bd.sin_family = AF_INET;
      	bd.sin_addr.s_addr = htonl(INADDR_ANY);	
     	bd.sin_port = port;	
      	status = bind(sock, &bd, sizeof bd);
     	if(status<0){
		if(MYERRNO == EADDRINUSE)
			present = TRUE;
		else
			abort();
	}
	close(sock);

	if(!present){
		lstDelete(&client_list, pclient);
		if(free(pclient)<0)
			abort();
#ifdef DEBUG
		printf("Deleted\n");
#endif
	}

	return OK;
}
