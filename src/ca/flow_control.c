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
/*	06xx89	joh	First Release					*/
/*	060591	joh	delinting					*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC network flow control module				*/
/*	File:	atcs:[ca]flow_control.c					*/
/*	Environment: VMS. UNIX, VRTX					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	ioc flow control module						*/
/*									*/
/*									*/
/*	Special comments						*/	
/*	------- --------						*/
/*									*/	
/************************************************************************/
/*_end									*/

static char	*sccsId = "@(#) $Id$";

#include		"iocinf.h"


/*
 * FLOW CONTROL
 * 
 * Keep track of how many times messages have 
 * come with out a break in between and 
 * suppress monitors if we are behind
 * (an update is sent when we catch up)
 */

void flow_control_on(struct ioc_in_use *piiu)
{
	int status;
	CA_STATIC *ca_static;

	ca_static = piiu->pcas;
	LOCK;

	/*	
	 * I prefer to avoid going into flow control 
	 * as this impacts the performance of batched fetches
	 */
	if (piiu->contiguous_msg_count >= MAX_CONTIGUOUS_MSG_COUNT) {
		if (!piiu->client_busy) {
			status = ca_busy_message(piiu);
			if (status==ECA_NORMAL) {
				assert(ca_static->ca_number_iiu_in_fc<UINT_MAX);
				ca_static->ca_number_iiu_in_fc++;
				piiu->client_busy = TRUE;
#				if defined(DEBUG) 
					printf("fc on\n");
#				endif
			}
		}
	}
	else {
		piiu->contiguous_msg_count++;
	}

	UNLOCK;
	return;
}

void flow_control_off(struct ioc_in_use *piiu)
{
	int    		status;
	CA_STATIC *ca_static;

	ca_static = piiu->pcas;
	LOCK;

	piiu->contiguous_msg_count = 0;
	if (piiu->client_busy) {
		status = ca_ready_message(piiu);
		if (status==ECA_NORMAL) {
			assert(ca_static->ca_number_iiu_in_fc>0u);
			ca_static->ca_number_iiu_in_fc--;
			piiu->client_busy = FALSE;
#			if defined(DEBUG) 
				printf("fc off\n");
#			endif
		}
	}

	UNLOCK;
	return;
}
