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
/*
 * $Log$
 * Revision 1.42  1998/10/07 14:30:41  jba
 * Modified log message.
 *
 * Revision 1.41  1998/09/24 21:22:51  jhill
 * detect reconnect faster when IOC reboots quickly
 *
 * Revision 1.40  1997/08/04 23:30:53  jhill
 * detect IOC reboot faster that EPICS_CA_CONN_TMO
 *
 * Revision 1.39  1997/06/13 09:14:15  jhill
 * connect/search proto changes
 *
 * Revision 1.38  1997/04/10 19:26:09  jhill
 * asynch connect, faster connect, ...
 *
 * Revision 1.37  1996/11/02 00:50:46  jhill
 * many pc port, const in API, and other changes
 *
 * Revision 1.36  1996/09/16 16:35:22  jhill
 * local exceptions => exception handler
 *
 * Revision 1.35  1996/06/19 17:59:04  jhill
 * many 3.13 beta changes
 *
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

#include "iocinf.h"
#include "bsdSocketResource.h"

#ifdef DEBUG
#define LOGRETRYINTERVAL logRetryInterval(__FILE__, __LINE__);
LOCAL void logRetryInterval(char *pFN, unsigned lineno);
#else
#define LOGRETRYINTERVAL 
#endif

LOCAL void retrySearchRequest();
LOCAL unsigned bhtHashIP(const struct sockaddr_in *pina);
LOCAL int updateBeaconPeriod (bhe *pBHE);

/*
 *	checkConnWatchdogs()
 */
void checkConnWatchdogs()
{
	IIU *piiu;
	ca_real delay;

	LOCK;

	piiu = (IIU *) ellFirst (&iiuList);
	while (piiu) {
		IIU *pnextiiu = (IIU *) ellNext (&piiu->node);

		if (piiu!=piiuCast) {
			/* 
			 * mark connection for shutdown if outgoing messages
			 * are not accepted by TCP/IP for several seconds
			 */
			if (piiu->sendPending) {
				delay = cac_time_diff (
						&ca_static->currentTime, 
						&piiu->timeAtSendBlock); 
				if (delay>ca_static->ca_connectTMO) {
					TAG_CONN_DOWN (piiu);
				}
			}

			/* 
			 * mark connection for shutdown if an echo response
			 * times out
			 */
			if (piiu->echoPending) {
				delay = cac_time_diff (
						&ca_static->currentTime,
						&piiu->timeAtEchoRequest);
				if (delay > CA_ECHO_TIMEOUT) {
					TAG_CONN_DOWN (piiu);
				}
			}
		}

		if (piiu->state==iiu_disconnected) {
			cac_close_ioc (piiu);
		}

		piiu = pnextiiu;
	}

	UNLOCK;
}


/*
 *
 *	MANAGE_CONN
 *
 * 	manages
 *	o retry of disconnected channels
 *	o connection heart beats
 *
 *
 */
void manage_conn()
{
	IIU *piiu;
	ca_real delay;

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
	for(piiu = (IIU *) iiuList.node.next;
		    piiu; piiu = (IIU *) piiu->node.next){

		if (piiu==piiuCast || piiu->state!=iiu_connected) {
			continue;
		}

		/*
		 * remain backwards compatible with old servers
		 * ( this isnt an echo request )
		 */
		if(!CA_V43(CA_PROTOCOL_VERSION, piiu->minor_version_number)){
			int	stmo;
			int rtmo;

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

		if (!piiu->echoPending) {
			delay = cac_time_diff (
					&ca_static->currentTime,
					&piiu->timeAtLastRecv);
			if (delay>ca_static->ca_connectTMO) {
				echo_request (piiu, &ca_static->currentTime);
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
	if (!piiuCast) {
		ca_static->ca_manage_conn_active = FALSE;
		return;
	}
	if (piiuCast->chidlist.count == 0) {
		ca_static->ca_manage_conn_active = FALSE;
		return;
	}

	delay = cac_time_diff (
			&ca_static->ca_conn_next_retry,
			&ca_static->currentTime);

	/*
	 * the retry sequence number if we have tried a reasonable 
	 * number of times and if the retry delay has expired
	 *
	 * (search_retry increments once all channels have received this
	 * number of tries)
	 */
	if (delay <= 0.0 && ca_static->ca_search_retry < MAXCONNTRIES) {
		retrySearchRequest ();
	}

	ca_static->ca_manage_conn_active = FALSE;
}


/*
 * retrySearchRequest ()
 */
LOCAL void retrySearchRequest ()
{
	ciu		chan;
	ciu		firstChan;
	int		status;
	unsigned	nSent=0u;

	if (!piiuCast) {
		return;
	}

	/*
	 * check to see if there is nothing to do here 
	 */
	if (ellCount(&piiuCast->chidlist)==0) {
		return;
	}

	LOCK;
	
	/*
	 * increment the retry sequence number
	 */
	ca_static->ca_search_retry_seq_no++; /* allowed to roll over */

	/*
	 * dynamically adjust the number of UDP frames per 
	 * try depending how many search requests are not 
	 * replied to
	 *
	 * This determines how many search request can be 
	 * sent together (at the same instant in time).
	 *
	 * The variable ca_static->ca_frames_per_try
	 * determines the number of UDP frames to be sent
	 * each time that retrySearchRequest() is called.
	 * If this value is too high we will waste some
	 * network bandwidth. If it is too low we will
	 * use very little of the incoming UDP message
	 * buffer associated with the server's port and
	 * will therefore take longer to connect. We 
	 * initialize ca_static->ca_frames_per_try
	 * to a prime number so that it is less likely that the
	 * same channel is in the last UDP frame
	 * sent every time that this is called (and
	 * potentially discarded by a CA server with
	 * a small UDP input queue). 
	 */

	/*
	 * increase rapidly if we see better than a 75% success rate 
	 */
	if (ca_static->ca_search_responses > 
		(ca_static->ca_search_tries-(ca_static->ca_search_tries/4u)) ) {
		/*
		 * double UDP frames per try if we have a good score
		 */
		if (ca_static->ca_frames_per_try < (UINT_MAX/4u) ) {
			ca_static->ca_frames_per_try += ca_static->ca_frames_per_try;
#ifdef DEBUG
			printf ("Increasing frame count to %u t=%u r=%u\n", 
				ca_static->ca_frames_per_try, ca_static->ca_search_tries, 
				ca_static->ca_search_responses);
#endif
		}
	}
	/*
	 * if we have less than a 50% success rate then reduce the count gradually
	 */
	else if (ca_static->ca_search_responses < (ca_static->ca_search_tries/2u) ) {
		if (ca_static->ca_frames_per_try>1u) {
			ca_static->ca_frames_per_try--;
#ifdef DEBUG
			printf ("Decreasing frame count to %u t=%u r=%u\n", 
				ca_static->ca_frames_per_try, ca_static->ca_search_tries, 
				ca_static->ca_search_responses);
#endif
		}
	}

	/*
	 * a successful search_msg() sends channel to
	 * the end of the list
	 */
	firstChan = chan = (ciu) ellFirst (&piiuCast->chidlist);
	while (chan) {

		ca_static->ca_min_retry = 
			min(ca_static->ca_min_retry, chan->retry);

		/*
		 * clear counter when we reach the end of the list
		 *
		 * if we are making some progress then
		 * dont increase the delay between search
		 * requests
		 */
		if (ca_static->ca_pEndOfBCastList == chan) {
			if (ca_static->ca_search_responses==0u) {
				cacSetRetryInterval(ca_static->ca_min_retry+1u);
			}

			ca_static->ca_min_retry = UINT_MAX;

			/*
			 * increment the retry sequence number
			 * (this prevents the time of the next search
			 * try from being set to the current time if
			 * we are handling a response from an old
			 * search message)
			 */
			ca_static->ca_search_retry_seq_no++; /* allowed to roll over */

			/*
			 * so that old search tries will not update the counters
			 */
			ca_static->ca_seq_no_at_list_begin = ca_static->ca_search_retry_seq_no;
			/*
			 * keeps the search try/response counters in bounds
			 * (but keep some of the info from the previous iteration)
			 */
			ca_static->ca_search_responses = ca_static->ca_search_responses/16u;
			ca_static->ca_search_tries = ca_static->ca_search_tries/16u;
#ifdef DEBUG
			printf ("saw end of list\n");
#endif	
		}

		/*
 		 * this moves the channel to the end of the
		 * list (if successful)
		 */
		status = search_msg (chan, DONTREPLY);
		if (status != ECA_NORMAL) {
			nSent++;

			if (nSent>=ca_static->ca_frames_per_try) {
				break;
			}

			/*
			 * flush out the search request buffer
			 */
			(*piiuCast->sendBytes)(piiuCast);

			/*
			 * try again
			 */
			status = search_msg (chan, DONTREPLY);
			if (status != ECA_NORMAL) {
				break;
			}
		}
		chan->retrySeqNo = ca_static->ca_search_retry_seq_no;
		chan = (ciu) ellFirst (&piiuCast->chidlist);

		/*
		 * dont send any of the channels twice within one try
		 */
		if (chan==firstChan) {
			/*
			 * add one to nSent because there may be 
			 * one more partial frame to be sent
			 */
			nSent++;

			/* 
			 * cap ca_static->ca_frames_per_try to
			 * the number of frames required for all of 
			 * the unresolved channels
			 */
			if (ca_static->ca_frames_per_try>nSent) {
				ca_static->ca_frames_per_try = nSent;
			}

			break;
		}
    	}

 	UNLOCK; 	

	ca_static->ca_conn_next_retry = 
		cac_time_sum (
			&ca_static->currentTime,
			&ca_static->ca_conn_retry_delay);
	LOGRETRYINTERVAL 
#ifdef DEBUG
printf("sent %u at cur sec=%u cur usec=%u delay sec=%u delay usec = %u\n",
	nSent, ca_static->currentTime.tv_sec,
	ca_static->currentTime.tv_usec,
	ca_static->ca_conn_retry_delay.tv_sec,
	ca_static->ca_conn_retry_delay.tv_usec);
#endif
}

/* 
 * cacSetRetryInterval()
 * (sets the interval between search tries)
 */
void cacSetRetryInterval(unsigned retryNo)
{
	unsigned idelay;
	ca_real	delay;

	/*
	 * set the retry number
	 */
	ca_static->ca_search_retry = min(retryNo, MAXCONNTRIES+1u);

	/*
	 * set the retry interval
	 */
	idelay = 1u << min(ca_static->ca_search_retry, 
					CHAR_BIT*sizeof(idelay)-1u);
	delay = idelay * CA_RECAST_DELAY; /* sec */	
	/*
	 * place upper limit on the retry delay
	 */
	delay = min (CA_RECAST_PERIOD, delay);
	ca_static->ca_conn_retry_delay.tv_sec = (long) delay;
	ca_static->ca_conn_retry_delay.tv_usec = 
		(long) ((delay-ca_static->ca_conn_retry_delay.tv_sec)
			* USEC_PER_SEC);
#if 0 
	printf ("new search period is %u sec %u usec\n",
			ca_static->ca_conn_retry_delay.tv_sec,
			ca_static->ca_conn_retry_delay.tv_usec);
#endif	
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
void mark_server_available (const struct sockaddr_in *pnet_addr)
{
	ciu chan;
	bhe *pBHE;
	unsigned port;	
	int netChange;

	if(!piiuCast){
		return;
	}

	LOCK;
	/*
	 * look for it in the hash table
	 */
	pBHE = lookupBeaconInetAddr(pnet_addr);
	if (pBHE) {

		netChange = updateBeaconPeriod (pBHE);

		/*
		 * update state of health for active virtual circuits 
		 * (only if we are not suspicious about past beacon changes
		 * until the next echo reply)
		 */
		if (pBHE->piiu) {
			if (!pBHE->piiu->beaconAnomaly) {
					pBHE->piiu->timeAtLastRecv = ca_static->currentTime;
			}
		}
	}
	else {
		/*
		 * This is the first beacon seen from this server.
		 * Wait until 2nd beacon is seen before deciding
		 * if it is a new server (or just the first
		 * time that we have seen a server's beacon
		 * shortly after the program started up)
		 */
		netChange = FALSE;
		createBeaconHashEntry (pnet_addr, TRUE);
	}

	if(!netChange){
		UNLOCK;
		return;
	}

	/*
	 * This part is needed when many machines
	 * have channels in a disconnected state that 
	 * dont exist anywhere on the network. This insures
	 * that we dont have many CA clients synchronously
	 * flooding the network with broadcasts (and swamping
	 * out requests for valid channels).
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
	}

	/*
	 * set retry count of all disconnected channels
	 * to zero
	 */
	cacSetRetryInterval(0u);
	chan = (ciu) ellFirst(&piiuCast->chidlist);
	while (chan) {
		chan->retry = 0u;
		chan = (ciu) ellNext (&chan->node);
	}

	UNLOCK;
}

/*
 * update beacon period
 *
 * updates beacon period, and looks for beacon anomalies
 */
LOCAL int updateBeaconPeriod (bhe *pBHE)
{
	ca_real currentPeriod;
	int netChange = FALSE;

	if (pBHE->timeStamp.tv_sec==0 && pBHE->timeStamp.tv_usec==0) {
		/* 
		 * this is the 1st beacon seen - the beacon time stamp
		 * was not initialized during BHE create because
		 * a TCP/IP connection created the beacon.
		 * (nothing to do but set the beacon time stamp and return)
		 */
		pBHE->timeStamp = ca_static->currentTime;

		/*
		 * be careful about using beacons to reset the connection
		 * time out watchdog until we have received a ping response 
		 * from the IOC (this makes the software detect reconnects
		 * faster when the server is rebooted twice in rapid 
		 * succession before a 1st or 2nd beacon has been received)
		 */
		if (pBHE->piiu) {
			pBHE->piiu->beaconAnomaly = TRUE;
		}

		return netChange;
	}

	/*
	 * compute the beacon period (if we have seen at least two beacons)
	 */
	currentPeriod = cac_time_diff (
						&ca_static->currentTime, &pBHE->timeStamp); 

	if (pBHE->averagePeriod<0.0) {
		ca_real totalRunningTime;

		/*
		 * this is the 2nd beacon seen. We cant tell about
		 * the change in period at this point so we just
		 * initialize the average period and return.
		 */
		pBHE->averagePeriod = currentPeriod;

		/*
		 * be careful about using beacons to reset the connection
		 * time out watchdog until we have received a ping response 
		 * from the IOC (this makes the software detect reconnects
		 * faster when the server is rebooted twice in rapid 
		 * succession before a 2nd beacon has been received)
		 */
		if (pBHE->piiu) {
			pBHE->piiu->beaconAnomaly = TRUE;
		}

		/*
		 * ignore beacons seen for the first time shortly after
		 * init, but do not ignore beacons arriving with a short
		 * period because the IOC was rebooted soon after the 
		 * client starts up.
		 */
		totalRunningTime = cac_time_diff (
						&pBHE->timeStamp, &ca_static->programBeginTime); 
		if (currentPeriod<=totalRunningTime) {
			netChange = TRUE;
#			ifdef DEBUG
			{
				char name[64];

				ipAddrToA (&pBHE->inetAddr, name, sizeof(name));
				ca_printf(	
					"new beacon from %s with period=%f running time to first beacon=%f\n", 
						name, currentPeriod, totalRunningTime);
			}
#			endif
		}
	}
	else {

		/*
		 * Is this an IOC seen because of a restored
		 * network segment? 
		 *
		 * It may be possible to get false triggers here 
		 * if the client is busy, but this does not cause
		 * problems because the echo response will tell us 
		 * that the server is available
		 */
		if (currentPeriod >= pBHE->averagePeriod*1.25) {

			if (pBHE->piiu) {
				/* 
				 * trigger on any missing beacon 
				 * if connected to this server
				 */
				pBHE->piiu->beaconAnomaly = TRUE;
			}

			if (currentPeriod >= pBHE->averagePeriod*3.25) {
				/* 
				 * trigger on any 3 contiguous missing beacons 
				 * if not connected to this server
				 */
				netChange = TRUE;
			}
		}


#		ifdef 	DEBUG
			if (netChange) {
				char name[64];

				ipAddrToA (&pBHE->inetAddr, name, sizeof(name));
				ca_printf(	
					"net resume seen %s cur=%f avg=%f\n", 
					name, currentPeriod, pBHE->averagePeriod);
			}
#		endif

		/*
		 * Is this an IOC seen because of an IOC reboot
		 * (beacon come at a higher rate just after the
		 * IOC reboots). Lower tolarance here because we
		 * dont have to worry about lost beacons.
		 *
		 * It may be possible to get false triggers here 
		 * if the client is busy, but this does not cause
		 * problems because the echo response will tell us 
		 * that the server is available
		 */
		if (currentPeriod <= pBHE->averagePeriod * 0.80) {
#			ifdef DEBUG
			{
				char name[64];

				ipAddrToA (&pBHE->inetAddr, name, sizeof(name));
				ca_printf(
					"reboot seen %s cur=%f avg=%f\n", 
					name, currentPeriod, pBHE->averagePeriod);
			}
#			endif
			netChange = TRUE;
			if (pBHE->piiu) {
				pBHE->piiu->beaconAnomaly = TRUE;
			}
		}
	
		/*
		 * update a running average period
		 */
		pBHE->averagePeriod = currentPeriod*0.125 + pBHE->averagePeriod*0.875;
	}

	/*
	 * update beacon time stamp 
	 */
	pBHE->timeStamp = ca_static->currentTime;

	return netChange;
}


/*
 * createBeaconHashEntry()
 *
 * LOCK must be applied
 */
bhe *createBeaconHashEntry(const struct sockaddr_in *pina, unsigned sawBeacon)
{
	bhe		*pBHE;
	unsigned	index;

	pBHE = lookupBeaconInetAddr(pina);
	if(pBHE){
		return pBHE;
	}

	index = bhtHashIP(pina);

	pBHE = (bhe *)calloc(1,sizeof(*pBHE));
	if(!pBHE){
		return NULL;
	}

#ifdef DEBUG
	{
		char name[64];

		ipAddrToA (pina, name, sizeof(name));
		ca_printf("created beacon entry for %s\n", name);
	}
#endif

	/*
	 * store the inet address
	 */
	pBHE->inetAddr = *pina;

	/*
	 * set average to -1.0 so that when the next beacon
	 * occurs we can distinguish between:
	 * o new server
	 * o existing server's beacon we are seeing
	 * 	for the first time shortly after program
	 *	start up
	 */
	pBHE->averagePeriod = -1.0;

	/*
	 * if creating this in response to a search reply
	 * and not in response to a beacon then sawBeacon
	 * is false and we set the beacon time stamp to
	 * zero (so we can correctly compute the period
	 * between the 1st and 2nd beacons)
	 */
	if (sawBeacon) {
		pBHE->timeStamp = ca_static->currentTime;
	}
	else {
		pBHE->timeStamp.tv_sec = 0;
		pBHE->timeStamp.tv_usec = 0;
	}

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
bhe *lookupBeaconInetAddr (const struct sockaddr_in *pina)
{
	bhe		*pBHE;
	unsigned	index;

	index = bhtHashIP(pina);

	pBHE = ca_static->ca_beaconHash[index];
	while (pBHE) {
		if (	pBHE->inetAddr.sin_addr.s_addr == pina->sin_addr.s_addr &&
			pBHE->inetAddr.sin_port == pina->sin_port) {
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
void removeBeaconInetAddr (const struct sockaddr_in *pina)
{
	bhe		*pBHE;
	bhe		**ppBHE;
	unsigned	index;

	index = bhtHashIP(pina);

	ppBHE = &ca_static->ca_beaconHash[index];
	pBHE = *ppBHE;
	while (pBHE) {
		if (	pBHE->inetAddr.sin_addr.s_addr == pina->sin_addr.s_addr &&
			pBHE->inetAddr.sin_port == pina->sin_port) {
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
 * bhtHashIP()
 */
LOCAL unsigned bhtHashIP(const struct sockaddr_in *pina)
{
	unsigned index;

#if BHT_INET_ADDR_MASK != 0xff
#	error BHT_INET_ADDR_MASK changed - recode this routine !
#endif

	index = pina->sin_addr.s_addr;
	index ^= pina->sin_port;
	index = (index>>16u) ^ index;
	index = (index>>8u) ^ index;
	index &= BHT_INET_ADDR_MASK;
	assert(index<NELEMENTS(ca_static->ca_beaconHash));
	return index;
}



/*
 * freeBeaconHash()
 *
 * LOCK must be applied
 */
void freeBeaconHash(struct CA_STATIC *ca_temp)
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

/*
 * retryPendingClaims()
 *
 * This assumes that all channels with claims pending are at the 
 * front of the list (and that the channel is moved to the end of
 * the list when a claim message has been sent for it)
 *
 * We send claim messages here until the outgoing message buffer 
 * will not accept any more messages
 */
void retryPendingClaims(IIU *piiu)
{
	chid chan;
	int status;

	LOCK;
	while ( (chan= (ciu) ellFirst (&piiu->chidlist)) ) {
		if (!chan->claimPending) {
			piiu->claimsPending = FALSE;
			break;
		}
		status = issue_claim_channel(chan);
		if (status!=ECA_NORMAL) {
			break;
		}
	}
	UNLOCK;
}

/*
 * Add chan to IIU and guarantee that
 * one chan on the B cast IIU list is pointed to by
 * ca_pEndOfBCastList 
 */
void addToChanList(ciu chan, IIU *piiu)
{
	if (piiu==piiuCast) {
		/*
		 * add to the beginning of the list so that search requests for
		 * this channel will be sent first (since the retry count is zero)
		 */
		if (ellCount(&piiu->chidlist)==0) {
			ca_static->ca_pEndOfBCastList = chan;
		}
		/*
		 * add to the front of the list so that
		 * search requests for new channels will be sent first
		 */
		chan->retry = 0u;
		ellInsert(&piiu->chidlist, NULL, &chan->node);
	}
	else {
		/*
		 * add to the beginning of the list until we
		 * have sent the claim message (after which we
		 * move it to the end of the list)
		 */
		chan->claimPending = TRUE;
		ellInsert(&piiu->chidlist, NULL, &chan->node);
	}
	chan->piiu = piiu;
}

/*
 * Remove chan from B-cast IIU and guarantee that
 * one chan on the list is pointed to by
 * ca_pEndOfBCastList 
 */
void removeFromChanList (ciu chan)
{
	IIU *piiu = (IIU *) chan->piiu;

	if (piiu==piiuCast) {
		if (ca_static->ca_pEndOfBCastList == chan) {
			if (ellPrevious(&chan->node)) {
				ca_static->ca_pEndOfBCastList = (ciu)
					ellPrevious (&chan->node);
			}
			else {
				ca_static->ca_pEndOfBCastList = (ciu)
					ellLast (&piiu->chidlist);
			}
		}
	}
	else if (ellCount (&piiu->chidlist)==1) {
		TAG_CONN_DOWN(piiu);
	}
	ellDelete (&piiu->chidlist, &chan->node);
	chan->piiu = NULL;
}

