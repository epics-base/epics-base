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
/*	.00 06xx89 joh	Init Release					*/
/*	.01 060591 joh	delinting					*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC connection automation				*/
/*	File:	atcs:[ca]conn.c						*/
/*	Environment: VMS, UNIX, vxWorks					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*									*/
/************************************************************************/
/*_end									*/
#ifdef UNIX
#include		<stdio.h>
#endif

#include		<vxWorks.h>
#include 		<cadef.h>
#include		<db_access.h>
#include		<iocmsg.h>
#include 		<iocinf.h>



/*
 *
 *	MANAGE_CONN
 *
 *	retry disconnected channels
 *
 *
 *	NOTES:
 *	Lock must be applied while in this routine
 */
void manage_conn(silent)
char			silent;
{
  	register chid		chix;
  	register unsigned int	retry_cnt = 0;
  	register unsigned int	keepalive_cnt = 0;
  	unsigned int		retry_cnt_no_handler = 0;
  	char			string[128];
	ca_time			current;
  	int			i;

	current = time(NULL);

  	for(i=0; i< nxtiiu; i++){
  		int	search_type;

		if(iiu[i].next_retry > current)
			continue;

		/*
		 * periodic keepalive on unused channels
		 */
    		if(i != BROADCAST_IIU && iiu[i].conn_up){

			/*
			 * reset of delay to the next keepalive
			 * occurs when the message is sent
			 */
			noop_msg(&iiu[i]);
			keepalive_cnt++;
      			continue;
		}

    		if(iiu[i].nconn_tries++ > MAXCONNTRIES)
      			continue;

		iiu[i].retry_delay += iiu[i].retry_delay;
		iiu[i].next_retry = current + iiu[i].retry_delay;

    		search_type = (i==BROADCAST_IIU? DONTREPLY: DOREPLY);

    		chix = (chid) &iiu[i].chidlist;
    		while(chix = (chid) chix->node.next){
      			if(chix->type != TYPENOTCONN)
				continue;
      			build_msg(chix,search_type);
     			retry_cnt++;

      			if(!(silent || chix->connection_func)){
       				ca_signal(ECA_CHIDNOTFND, chix+1);
				retry_cnt_no_handler++;
			}
    		}
  	}

  	if(retry_cnt){
    		printf("<Trying> ");
#ifdef UNIX
    		fflush(stdout);
#endif

    		if(!silent && retry_cnt_no_handler){
      			sprintf(string, "%d channels outstanding", retry_cnt);
      			ca_signal(ECA_CHIDRETRY, string);
    		}
  	}

	if(keepalive_cnt|retry_cnt){
    		cac_send_msg();
	}

}



/*
 *
 *
 *	MARK_SERVER_AVAILABLE
 *
 *
 *	NOTES:
 *	Lock must be applied while in this routine
 *
 */
void
mark_server_available(pnet_addr)
struct in_addr  *pnet_addr;
{
	unsigned		port;	
  	int 			i;

	/*
	 * if timers have expired take care of them
	 * before they are reset
	 */
	manage_conn(TRUE);

#ifdef DEBUG
	printf("<%s> ",host_from_addr(pnet_addr));
#ifdef UNIX
    	fflush(stdout);
#endif
#endif

 	for(i=0;i<nxtiiu;i++)
    		if(	(pnet_addr->s_addr == 
			iiu[i].sock_addr.sin_addr.s_addr)){

      			if(iiu[i].conn_up){
				/*
				 * Check if the conn is down but TCP 
				 * has not informed me by sending a NULL msg
				 */
        			noop_msg(&iiu[i]);
        			cac_send_msg();
      			}
			else{
				/*
				 * reset the delay to the next retry 
				 */
				iiu[i].next_retry = CA_CURRENT_TIME;
      				iiu[i].nconn_tries = 0;
				iiu[i].retry_delay = 1;
				manage_conn(TRUE);
			}
      			return;
    		}

	/*
	 * never connected to this IOC before
	 *
	 * We end up here when the client starts before the server
	 *
	 * It would be best if this used a directed UDP
	 *		reply rather than a broadcast
	 */

	/*
	 * reset the retry cnt to 3
	 */
      	iiu[BROADCAST_IIU].nconn_tries = MAXCONNTRIES-3;

	/*
	 * This part is very important since many machines
	 * could have channels in a disconnected state which
	 * dont exist anywhere on the network. This insures
	 * that we dont have many CA clients synchronously
	 * flooding the network with broadcasts.
	 *
	 * I fetch the local port number and use the low order bits
	 * as a pseudo random delay to prevent every one 
	 * from replying at once.
	 */
	{
  	  	struct sockaddr_in	saddr;
		unsigned		saddr_length = sizeof(saddr);
		int			status;

		status = getsockname(
				iiu[BROADCAST_IIU].sock_chan,
				&saddr,
				&saddr_length);
		if(status<0)
			abort();
		port = saddr.sin_port;
	}

	iiu[BROADCAST_IIU].retry_delay = (port&0xf) + 1;
	iiu[BROADCAST_IIU].next_retry = time(NULL) + iiu[BROADCAST_IIU].retry_delay;
#ifdef DEBUG
	printf("<Trying ukn online after pseudo random delay=%d sec> ",
		iiu[BROADCAST_IIU].retry_delay);
#ifdef UNIX
    	fflush(stdout);
#endif
#endif

}
