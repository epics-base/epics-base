/* $Id$ */
/************************************************************************/
/* 									*/
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
/*	.10 102093 joh	improved broadcast scheduling for		*/
/*			reconnects					*/
/*	.11 042994 joh	removed client side heart beat			*/
/*	.12 110194 joh	improved search scheduling			*/
/*			(dont send all chans in a block)		*/
/*									*/
/* $Log$
 * Revision 1.34  1995/08/22  00:19:21  jhill
 * use current time var to init time stamp in a beacon hash entry
 *								*/
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

static char 	*sccsId = "@(#) $Id$";

#include 	"iocinf.h"

#ifdef DEBUG
#define LOGRETRYINTERVAL logRetryInterval(__FILE__, __LINE__);
LOCAL void logRetryInterval(char *pFN, unsigned lineno);
#else
#define LOGRETRYINTERVAL 
#endif

LOCAL void retrySearchRequest(int silent);


/*
 *
 *	MANAGE_CONN
 *
 *	retry disconnected channels
 *
 *
 */
void manage_conn(int silent)
{
	IIU		*piiu;
	ca_real		delay;
	long		idelay;

	/*
	 * prevent recursion
	 */
	if(ca_static->ca_manage_conn_active){
		return;
	}

	ca_static->ca_manage_conn_active = TRUE;

        /*
         * issue connection heartbeat
	 * (if we dont see a beacon)
         */
        LOCK;
        for(    piiu = (IIU *) iiuList.node.next;
                piiu;
                piiu = (IIU *) piiu->node.next){

                if (piiu == piiuCast || !piiu->conn_up) {
                        continue;
                }

		/* 
		 * mark connection for shutdown if outgoing messages
		 * are not accepted by TCP/IP for several seconds
		 */
		if (piiu->sendPending) {
			delay = cac_time_diff (
					&ca_static->currentTime, 
					&piiu->timeAtSendBlock); 
			if (delay>ca_static->ca_connectTMO) {
				TAG_CONN_DOWN(piiu);
				continue;
			}
		}

		/*
		 * remain backwards compatible with old servers
		 * ( this isnt an echo request )
		 */
		if(!CA_V43(CA_PROTOCOL_VERSION, piiu->minor_version_number)){
			int	stmo;
			int 	rtmo;

			delay = cac_time_diff (
					&ca_static->currentTime,
					&piiu->timeAtEchoRequest);
			stmo = delay > CA_RETRY_PERIOD;
			delay = cac_time_diff (
					&ca_static->currentTime,
					&piiu->timeAtLastRecv);
			rtmo = delay > CA_RETRY_PERIOD;
			if(stmo && rtmo && !piiu->sendPending){
				piiu->timeAtEchoRequest = ca_static->currentTime;
				noop_msg(piiu);
			}
			continue;
		}

		if(piiu->echoPending){
			delay = cac_time_diff (
					&ca_static->currentTime,
					&piiu->timeAtEchoRequest);
			if (delay > CA_ECHO_TIMEOUT) {
				/* 
				 * mark connection for shutdown
				 */
				TAG_CONN_DOWN(piiu);
			}
		}
		else{
			delay = cac_time_diff (
					&ca_static->currentTime,
					&piiu->timeAtLastRecv);
			if (delay>ca_static->ca_connectTMO) {
				echo_request(piiu, &ca_static->currentTime);
			}
		}

        }
        UNLOCK;

	/*
	 * try to attach to the repeater if we havent yet
	 */
	if (!ca_static->ca_repeater_contacted) {
		delay = cac_time_diff (
				&ca_static->currentTime,
				&ca_static->ca_last_repeater_try);
		if (delay > REPEATER_TRY_PERIOD) {
			ca_static->ca_last_repeater_try = ca_static->currentTime;
			notify_ca_repeater();
		}
	}

	/*
	 * Stop here if there are not any disconnected channels
	 */
	if(!piiuCast) {
		ca_static->ca_manage_conn_active = FALSE;
		return;
	}
	if (piiuCast->chidlist.count == 0) {
		ca_static->ca_manage_conn_active = FALSE;
		return;
	}

	if(ca_static->ca_conn_next_retry.tv_sec == CA_CURRENT_TIME.tv_sec &&
	   ca_static->ca_conn_next_retry.tv_usec == CA_CURRENT_TIME.tv_usec){
		ca_static->ca_conn_next_retry = ca_static->currentTime;
		LOGRETRYINTERVAL 
	}

	delay = cac_time_diff (
			&ca_static->ca_conn_next_retry,
			&ca_static->currentTime);

	if (delay > 0.0) {
		ca_static->ca_manage_conn_active = FALSE;
		return;
	}

	/*
	 * the retry sequence number
	 * (increments once all channels have received this
	 * number of tries)
	 */
    	if (ca_static->ca_search_retry >= MAXCONNTRIES) {
		ca_static->ca_manage_conn_active = FALSE;
      		return;
	}

	retrySearchRequest (silent);

	/*
	 * set the retry interval
	 */
	assert(ca_static->ca_search_retry < CHAR_BIT*sizeof(idelay));
	idelay = 1;
	idelay = idelay << ca_static->ca_search_retry;
	delay = idelay * CA_RECAST_DELAY; /* sec */	
	delay = min (CA_RECAST_PERIOD, delay);
	idelay = (long) delay;
	ca_static->ca_conn_retry_delay.tv_sec = idelay;
	ca_static->ca_conn_retry_delay.tv_usec = 
		(long) ((delay-idelay)*USEC_PER_SEC);
	ca_static->ca_conn_next_retry = 
		cac_time_sum (
			&ca_static->currentTime,
			&ca_static->ca_conn_retry_delay);
	LOGRETRYINTERVAL 

	ca_static->ca_manage_conn_active = FALSE;
}


/*
 * retrySearchRequest ()
 */
LOCAL void retrySearchRequest (int silent)
{
	ELLLIST		channelsSent;
	chid		chix;
	unsigned	min_retry_num;
	unsigned	retry_cnt = 0;
  	unsigned 	retry_cnt_no_handler = 0;
	int		status;

	if (!piiuCast) {
		return;
	}

	ellInit (&channelsSent);

	LOCK;
	min_retry_num = MAXCONNTRIES;
	while (chix = (chid) ellGet (&piiuCast->chidlist)) {

		ellAdd (&channelsSent, &chix->node);

		min_retry_num = min (min_retry_num, chix->retry);

		if (chix->retry <= ca_static->ca_search_retry) {

			status = search_msg (chix, DONTREPLY);
			if (status == ECA_NORMAL) {
				retry_cnt++;
				if (!(silent || chix->pConnFunc)) {
					ca_signal (
						ECA_CHIDNOTFND, 
						(char *)(chix+1));
					retry_cnt_no_handler++;
				}
			}
			else {
				break;
			}
		}
    	}

	/*
	 * if we saw the entire list  
	 */
	if (chix==NULL) {
		/*
		 * increment the retry sequence number
		 * (only if we get no responses during a sequence)
		 */
		if (ca_static->ca_search_retry<MAXCONNTRIES 
			&& ca_static->ca_search_responses==0) {
			ca_static->ca_search_retry++;
		}

		/*
		 * jump to the minimum retry number
		 */
		if (ca_static->ca_search_retry<min_retry_num) {
			ca_static->ca_search_retry = min_retry_num;
			ca_static->ca_search_responses = 0;
		}
	}

	/*
	 * return channels sent to main cast IIU's 
	 * channel prior to removing the lock
	 *
	 * This reorders the list so that each channel
	 * get a fair chance to connect
	 */
	ellConcat (&piiuCast->chidlist, &channelsSent);

	/*
	 * LOCK around use of the sprintf buffer
	 */
  	if (retry_cnt) {
    		if (!silent && retry_cnt_no_handler) {
      			sprintf (
				sprintf_buf, 
				"%d channels outstanding", 
				retry_cnt);
      			ca_signal (ECA_CHIDRETRY, sprintf_buf);
    		}
  	}
 	UNLOCK; 	
}


/* 
 * logRetryInterval()
 */
#ifdef DEBUG
LOCAL void logRetryInterval(char *pFN, unsigned lineno)
{
	ca_time currentTime;
	ca_real	delay;

	assert(ca_static->ca_conn_next_retry.tv_usec<USEC_PER_SEC);
	cac_gettimeval(&currentTime);
	delay = cac_time_diff(
			&ca_static->ca_conn_next_retry,
			&currentTime);
	ca_printf("%s.%d next retry in %f sec\n",
			pFN,
			lineno,
			delay);
	delay = ca_static->ca_conn_retry_delay.tv_sec +
		((double)ca_static->ca_conn_retry_delay.tv_usec)/
			USEC_PER_SEC;
	ca_printf("%s.%d retry interval = %f sec - disconn count = %d\n",
			pFN,
			lineno,
			delay,
			ellCount(&piiuCast->chidlist));
}
#endif


/*
 *	MARK_SERVER_AVAILABLE
 */
void mark_server_available(const struct in_addr *pnet_addr)
{
	chid		chan;
	ca_real		currentPeriod;
	bhe		*pBHE;
	unsigned	port;	
	int 		netChange = FALSE;

	/*
	 * if timers have expired take care of them
	 * before they are reset
	 */
	manage_conn(TRUE);

	if(!piiuCast){
		return;
	}

	LOCK;
	/*
	 * look for it in the hash table
	 */
	pBHE = lookupBeaconInetAddr(pnet_addr);
	if(pBHE){

		/*
		 * if we have seen the beacon before ignore it
		 * (unless there is an unusual change in its period)
		 */

		/*
		 * update time stamp and average period
		 */
		currentPeriod = cac_time_diff (
					&ca_static->currentTime, 
					&pBHE->timeStamp); 
		/*
		 * update the average
		 */
		pBHE->averagePeriod += currentPeriod;
		pBHE->averagePeriod /= 2.0;
		pBHE->timeStamp = ca_static->currentTime;
	
		if ((currentPeriod/4.0)>=pBHE->averagePeriod) {
#ifdef 	DEBUG
			ca_printf(	
				"net resume seen %x cur=%d avg=%d\n", 
				pnet_addr->s_addr,
				currentPeriod, 
				pBHE->averagePeriod);
#endif
			netChange = TRUE;
		}

		if ((pBHE->averagePeriod/2.0)>=currentPeriod) {
#ifdef DEBUG
			ca_printf(
				"reboot seen %x cur=%d avg=%d\n", 
				pnet_addr->s_addr,
				currentPeriod, 
				pBHE->averagePeriod);
#endif
			netChange = TRUE;
		}
		if(pBHE->piiu){
			pBHE->piiu->timeAtLastRecv = ca_static->currentTime;
		}
		if(!netChange){
			UNLOCK;
			return;
		}
	}
	else{
		pBHE = createBeaconHashEntry(pnet_addr);
		if(!pBHE){
			UNLOCK;
			return;
		}
	}
	

	/*
	 * This part is essential since many machines
	 * might have channels in a disconnected state which
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
		assert (status>=0);
		port = ntohs(saddr.sin_port);
	}

	{
		ca_real		diff;
		ca_real		delay;
		long		idelay;
		ca_time		ca_delay;
		ca_time		next;

		delay = (port&CA_RECAST_PORT_MASK);
		delay /= MSEC_PER_SEC;
		delay += CA_RECAST_DELAY;
		idelay = (long) delay;
		ca_delay.tv_sec = idelay;
		ca_delay.tv_usec = (long) ((delay-idelay) * USEC_PER_SEC); 
		next = cac_time_sum(&ca_static->currentTime, &ca_delay);

		diff = cac_time_diff(
				&ca_static->ca_conn_next_retry,
				&next);
		if(diff>0.0){
			ca_static->ca_conn_next_retry = next;
			LOGRETRYINTERVAL 
		}
		idelay = (long) CA_RECAST_DELAY;
		ca_static->ca_conn_retry_delay.tv_sec = idelay;
		ca_static->ca_conn_retry_delay.tv_usec = 
			(long) ((CA_RECAST_DELAY-idelay) * USEC_PER_SEC);
		ca_static->ca_search_retry = 0;
	}

	/*
	 * set retry count of all disconnected channels
	 * to zero
	 */
	chan = (chid) ellFirst(&piiuCast->chidlist);
	while (chan) {
		chan->retry = 0;
		chan = (chid) ellNext (&chan->node);
	}

	UNLOCK;
}


/*
 * createBeaconHashEntry()
 *
 * LOCK must be applied
 */
bhe *createBeaconHashEntry(const struct in_addr *pnet_addr)
{
	bhe		*pBHE;
	unsigned	index;

	pBHE = lookupBeaconInetAddr(pnet_addr);
	if(pBHE){
		return pBHE;
	}

	index = ntohl(pnet_addr->s_addr);
	index &= BHT_INET_ADDR_MASK;

	assert(index<NELEMENTS(ca_static->ca_beaconHash));

	pBHE = (bhe *)calloc(1,sizeof(*pBHE));
	if(!pBHE){
		return NULL;
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
	pBHE->averagePeriod = 0.0;
	pBHE->timeStamp = ca_static->currentTime;

	/*
	 * install in the hash table
	 */
	pBHE->pNext = ca_static->ca_beaconHash[index];
	ca_static->ca_beaconHash[index] = pBHE;

	return pBHE;
}


/*
 * lookupBeaconInetAddr()
 *
 * LOCK must be applied
 */
bhe *lookupBeaconInetAddr (const struct in_addr *pnet_addr)
{
	bhe		*pBHE;
	unsigned	index;

	index = ntohl(pnet_addr->s_addr);
	index &= BHT_INET_ADDR_MASK;

	assert(index<NELEMENTS(ca_static->ca_beaconHash));

	pBHE = ca_static->ca_beaconHash[index];
	while(pBHE){
		if(pBHE->inetAddr.s_addr == pnet_addr->s_addr){
			break;
		}
		pBHE = pBHE->pNext;
	}
	return pBHE;
}



/*
 * removeBeaconInetAddr()
 *
 * LOCK must be applied
 */
void removeBeaconInetAddr (const struct in_addr *pnet_addr)
{
	bhe		*pBHE;
	bhe		**ppBHE;
	unsigned	index;

	index = ntohl (pnet_addr->s_addr);
	index &= BHT_INET_ADDR_MASK;

	assert (index<NELEMENTS(ca_static->ca_beaconHash));

	ppBHE = &ca_static->ca_beaconHash[index];
	pBHE = *ppBHE;
	while (pBHE) {
		if (pBHE->inetAddr.s_addr == pnet_addr->s_addr) {
			*ppBHE = pBHE->pNext;
			free (pBHE);
			return;
		}
		ppBHE = &pBHE->pNext;
		pBHE = *ppBHE;
	}
	assert (0);
}


/*
 * freeBeaconHash()
 *
 * LOCK must be applied
 */
void freeBeaconHash(struct ca_static *ca_temp)
{
	bhe		*pBHE;
	bhe		**ppBHE;
	int		len;

	len = NELEMENTS(ca_temp->ca_beaconHash);
	for(    ppBHE = ca_temp->ca_beaconHash;
		ppBHE < &ca_temp->ca_beaconHash[len];
		ppBHE++){

		pBHE = *ppBHE;
		while(pBHE){
			bhe     *pOld;

			pOld = pBHE;
			pBHE = pBHE->pNext;
			free(pOld);
		}
	}
}

