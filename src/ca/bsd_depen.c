/*
 *     	$Id$ 
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *      Date:  9-93
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      Modification Log:
 *      -----------------
 *
 */

#include "iocinf.h"
#include "osiSleep.h"
#include "bsdSocketResource.h"


/*
 * cac_select_io()
 *
 * NOTE: on multithreaded systems this assumes that the
 * local implementation of select is reentrant
 */
int cac_select_io (struct timeval *ptimeout, int flags)
{
	/*
	 * Use auto timeout so there is no chance of
	 * recursive reuse of ptimeout 
	 */
	struct timeval	autoTimeOut = *ptimeout;
	long            status;
	IIU             *piiu;
	unsigned long   freespace;
	SOCKET		maxfd;
	caFDInfo	*pfdi;
	int		ioPending;

	LOCK;
	pfdi = (caFDInfo *) ellGet(&ca_static->fdInfoFreeList);

	if (!pfdi) {
		pfdi = (caFDInfo *) calloc (1, sizeof(*pfdi));
		if (!pfdi) {
                        ca_printf("CAC: no mem for select ctx?\n");
			UNLOCK;
			return -1;
		}
	}
	ellAdd (&ca_static->fdInfoList, &pfdi->node);

	FD_ZERO (&pfdi->readMask);
	FD_ZERO (&pfdi->writeMask);

	maxfd = 0;
	ioPending = FALSE;
	for( piiu = (IIU *) iiuList.node.next;
		piiu; piiu = (IIU *) piiu->node.next) {

		if (piiu->state==iiu_disconnected) {
			continue;
		}

		if (!FD_IN_FDSET(piiu->sock_chan))
		{
			ca_printf(
			"%s.%d: file number %d > FD_SETSIZE=%d ignored\n",
			    __FILE__, __LINE__, piiu->sock_chan, FD_SETSIZE);
			continue;
		}

		/*
		 * Dont bother receiving if we have insufficient
		 * space for the maximum UDP message, or space
		 * for one TCP byte.
		 */
		if (flags&CA_DO_RECVS) {
			freespace = cacRingBufferWriteSize (&piiu->recv, TRUE);
			if (freespace>=piiu->minfreespace) {
				maxfd = max (maxfd,piiu->sock_chan);
				FD_SET (piiu->sock_chan, &pfdi->readMask);
				piiu->recvPending = TRUE;
				ioPending = TRUE;
			}
			else {
				piiu->recvPending = FALSE;
			}
		}
		else {
			piiu->recvPending = FALSE;
		}

		if (piiu->state==iiu_connecting) {
			FD_SET (piiu->sock_chan, &pfdi->writeMask);
			ioPending = TRUE;
		}               
		else if (flags&CA_DO_SENDS || piiu->pushPending) {
			if (cacRingBufferReadSize(&piiu->send, FALSE)>0) {
				maxfd = max (maxfd,piiu->sock_chan);
				FD_SET (piiu->sock_chan, &pfdi->writeMask);
				ioPending = TRUE;
			}
		}
	}
	UNLOCK;

	/*
 	 * win32 requires this (others will
	 * run faster with this installed)
	 */
	if (!ioPending) {
		/*
		 * recover from subtle differences between
		 * windows sockets and UNIX sockets implementation
		 * of select()
		 */
		if (ptimeout->tv_sec!=0 ||
			ptimeout->tv_usec!=0 ) {
			osiSleep (ptimeout->tv_sec, ptimeout->tv_usec);
		}
		status = 0;
	}
	else {
		status = select(
				maxfd+1,
				&pfdi->readMask,
				&pfdi->writeMask,
				NULL,
				&autoTimeOut);
		if (status<0) {
			int errnoCpy = SOCKERRNO;

			if (errnoCpy!=SOCK_EINTR) {
				ca_printf (
					"CAC: unexpected select fail: %d=%s\n",
					errnoCpy, SOCKERRSTR);
			}
		}
	}

	/*
	 * get a new time stamp if we have been waiting
	 * for any significant length of time
	 */
	if (ptimeout->tv_sec || ptimeout->tv_usec) {
		cac_gettimeval (&ca_static->currentTime);
	}

	LOCK;
	/*
	 * must run through the IIU list even if no IO is pending
	 * if any of the IOCs are in flow control (so that an exit 
	 * flow control msg can be sent to each of them that are)
	 */
	if (status>0 || (ca_static->ca_number_iiu_in_fc>0u&&status>=0) ) {
		for (	piiu = (IIU *) iiuList.node.next;
			piiu; piiu = (IIU *) piiu->node.next) {

			if (piiu->state==iiu_disconnected) {
				continue;
			}

			if (FD_ISSET(piiu->sock_chan,&pfdi->readMask)) {
				(*piiu->recvBytes)(piiu);
				/*
				 * if we were not blocking and there is a 
				 * message present then start to suspect that
				 * we are getting behind
				 */
				if (piiu->sock_proto==IPPROTO_TCP) {
					if (ptimeout->tv_sec==0 || ptimeout->tv_usec==0) {
						flow_control_on(piiu);
					}
				}
			}
			else if (piiu->recvPending) {
				/*
				 * if we are looking for incoming messages
				 * and there are none then we are certain that
				 * we are not getting behind 
				 */
				if (piiu->sock_proto==IPPROTO_TCP) {
					flow_control_off(piiu);
				}
			}

			if (FD_ISSET(piiu->sock_chan,&pfdi->writeMask)) {
				(*piiu->sendBytes)(piiu);
			}
		}
	}

	ellDelete (&ca_static->fdInfoList, &pfdi->node);
	ellAdd (&ca_static->fdInfoFreeList, &pfdi->node);
	UNLOCK;

	return status;
}

