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
/*	.02 031892 joh	initial broadcast retry delay is now a #define	*/
/*	.03 031992 joh	reset the iiu delay if the current time 	*/
/*			is specified					*/
/*	.04 043092 joh	check to see if the conn is up when setting	*/
/*			for CA_CUURRENT_TIME to be safe			*/
/*	.05 072792 joh	better messages					*/
/*	.06 111892 joh	tuned up cast retries				*/
/*	.07 010493 joh	print retry count when `<Trying>'		*/
/*	.08 010493 joh	removed `<Trying>' message			*/
/*	.09 090293 joh	removed flush from manage_conn			*/
/*			(now handled by the send needed flag)		*/
/*	.10 102093 joh	improved broadcast schedualing for		*/
/*			reconnects					*/
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

static char 	*sccsId = "$Id$\t$Date$";

#include 	"iocinf.h"


/*
 *
 *	MANAGE_CONN
 *
 *	retry disconnected channels
 *
 *
 */
#ifdef __STDC__
void manage_conn(int silent)
#else
void manage_conn(silent)
int	silent;
#endif
{
	struct ioc_in_use	*piiu;
  	chid			chix;
  	unsigned int		retry_cnt = 0;
  	unsigned int		keepalive_cnt = 0;
  	unsigned int		retry_cnt_no_handler = 0;
	ca_time			current;

	current = time(NULL);

	/*
	 * issue connection heartbeat
	 */
	LOCK;
	for(	piiu = (struct ioc_in_use *) iiuList.node.next;
		piiu;
		piiu = (struct ioc_in_use *) piiu->node.next){

		if(piiu == piiuCast || !piiu->conn_up){
			continue;
		}

		if(piiu->next_retry == CA_CURRENT_TIME){
			piiu->next_retry = current + CA_RETRY_PERIOD;
			continue;
		}

		if(piiu->next_retry > current)
			continue;

		/*
		 * periodic keepalive on unused channels
		 */

		/*
		 * reset of delay to the next keepalive
		 * occurs when the message is sent
		 */
		noop_msg(piiu);
		keepalive_cnt++;
	}
	UNLOCK;

	if(!piiuCast){
		return;
	}

	if(piiuCast->next_retry == CA_CURRENT_TIME){
		piiuCast->next_retry = current + piiuCast->retry_delay;
	}

	if(piiuCast->next_retry > current)
		return;

    	if(piiuCast->nconn_tries++ > MAXCONNTRIES)
      		return;

	if(piiuCast->retry_delay<CA_RECAST_PERIOD){
		piiuCast->retry_delay += piiuCast->retry_delay;
	}
	piiuCast->next_retry = current + piiuCast->retry_delay;

	LOCK;
	for(	chix = (chid) piiuCast->chidlist.node.next;
		chix;
		chix = (chid) chix->node.next){

      		build_msg(chix, DONTREPLY);
     		retry_cnt++;

      		if(!(silent || chix->connection_func)){
       			ca_signal(ECA_CHIDNOTFND, (char *)(chix+1));
			retry_cnt_no_handler++;
		}
    	}
 	UNLOCK; 	

  	if(retry_cnt){
#ifdef TRYING_MESSAGE
    		ca_printf("<Trying %d> ", retry_cnt);
#ifdef UNIX
    		fflush(stdout);
#endif /*UNIX*/
#endif /*TRYING_MESSAGE*/

    		if(!silent && retry_cnt_no_handler){
      			sprintf(sprintf_buf, "%d channels outstanding", retry_cnt);
      			ca_signal(ECA_CHIDRETRY, sprintf_buf);
    		}
  	}
}


/*
 *
 *
 *	MARK_SERVER_AVAILABLE
 *
 *
 *
 */
#ifdef __STDC__
void mark_server_available(struct in_addr *pnet_addr)
#else
void mark_server_available(pnet_addr)
struct in_addr  *pnet_addr;
#endif
{
	int		currentPeriod;
	int		currentTime;
	bhe		*pBHE;
	unsigned	index;
	unsigned	port;	

	/*
	 * if timers have expired take care of them
	 * before they are reset
	 */
	manage_conn(TRUE);

	if(!piiuCast){
		return;
	}

	currentTime = time(NULL);

	/*
	 * look for it in the hash table
	 */
	index = ntohl(pnet_addr->s_addr);
	index &= BHT_INET_ADDR_MASK;
	LOCK;
	pBHE = ca_static->ca_beaconHash[index];
	while(pBHE){
		if(pBHE->inetAddr.s_addr ==
			pnet_addr->s_addr){
					
			break;
		}
		pBHE = pBHE->pNext;
	}

	/*
	 * if we have seen the beacon before ignore it
	 * (unless there is an unusual change in its period)
	 */
	if(pBHE){
		int netChange = FALSE;

		/*
		 * update time stamp and average period
		 */
		currentPeriod = currentTime - pBHE->timeStamp;
		pBHE->averagePeriod = 
			(currentPeriod + pBHE->averagePeriod)>>1;
		pBHE->timeStamp = currentTime;

		if((currentPeriod>>2)>=pBHE->averagePeriod){
#ifdef DEBUG
			ca_printf(	
				"net resume seen %x cur=%d avg=%d\n", 
				pnet_addr->s_addr,
				currentPeriod, 
				pBHE->averagePeriod);
#endif
			netChange = TRUE;
		}

		if((pBHE->averagePeriod>>1)>=currentPeriod){
#ifdef DEBUG
			ca_printf(
				"reboot seen %x cur=%d avg=%d\n", 
				pnet_addr->s_addr,
				currentPeriod, 
				pBHE->averagePeriod);
#endif
			netChange = TRUE;
		}

		if(!netChange){
			UNLOCK;
			return;
		}
	}
	else{

		/*
		 * create the hash entry
		 */
		pBHE = (bhe *)calloc(1,sizeof(*pBHE));
		if(!pBHE){
			UNLOCK;
			return;
		}

#ifdef DEBUG
		ca_printf("new IOC %x\n", pnet_addr->s_addr);
#endif
		/*
		 * store the inet address
		 */
		pBHE->inetAddr = *pnet_addr;

		/*
		 * start the average at zero
		 */
		pBHE->averagePeriod = 0;
		pBHE->timeStamp = currentTime;

		/*
		 * install in the hash table
		 */
		pBHE->pNext = ca_static->ca_beaconHash[index];
		ca_static->ca_beaconHash[index] = pBHE;
	}

#ifdef DEBUG
	ca_printf("CAC: <%s> ",host_from_addr(pnet_addr));
#ifdef UNIX
    	fflush(stdout);
#endif
#endif


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
		int			saddr_length = sizeof(saddr);
		int			status;

		status = getsockname(
				piiuCast->sock_chan,
				(struct sockaddr *)&saddr,
				&saddr_length);
		assert(status>=0);
		port = ntohs(saddr.sin_port);
	}

	{
		int	delay;
		int	next;

		delay = port&CA_RECAST_PORT_MASK;

		next = currentTime + delay;

		if(piiuCast->next_retry>next){
			piiuCast->next_retry = next;
		}
		piiuCast->retry_delay = CA_RECAST_DELAY;
	      	piiuCast->nconn_tries = 0;
	}

#ifdef DEBUG
	ca_printf("CAC: <Trying ukn online after pseudo random delay=%d sec> ",
		piiuCast->retry_delay);
#ifdef UNIX
    	fflush(stdout);
#endif
#endif
	UNLOCK;

}
