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


/*
 * cac_select_io()
 *
 * NOTE: on multithreaded systems this assumes that the
 * local implementation of select is reentrant
 */
int cac_select_io(struct timeval *ptimeout, int flags)
{
        long            status;
        IIU             *piiu;
        unsigned long   freespace;
	SOCKET		maxfd;
	caFDInfo	*pfdi;

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
	for(    piiu = (IIU *) iiuList.node.next;
		piiu;
		piiu = (IIU *) piiu->node.next) {

		if (!piiu->conn_up) {
			continue;
		}

                /*
                 * Dont bother receiving if we have insufficient
                 * space for the maximum UDP message
                 */
                if (flags&CA_DO_RECVS) {
                        freespace = cacRingBufferWriteSize (&piiu->recv, TRUE);
                        if (freespace>=piiu->minfreespace) {
				maxfd = max (maxfd,piiu->sock_chan);
                                FD_SET (piiu->sock_chan, &pfdi->readMask);
                        }
                }

                if (flags&CA_DO_SENDS) {
                        if (cacRingBufferReadSize(&piiu->send, FALSE)>0) {
				maxfd = max (maxfd,piiu->sock_chan);
                                FD_SET (piiu->sock_chan, &pfdi->writeMask);
                        }
                }
        }
	UNLOCK;

#if 0
printf(	"max fd=%d tv_usec=%d tv_sec=%d\n", 
	maxfd, 	
	ptimeout->tv_usec, 
	ptimeout->tv_sec);
#endif

#if defined(vxWorks) && 0
if(client_lock->recurse>0){
	ca_printf("lock is on and we are going to sleep %d!",
			client_lock->recurse);
	taskSuspend(0);
}
#endif

        status = select(
                        maxfd+1,
                        &pfdi->readMask,
                        &pfdi->writeMask,
                        NULL,
                        ptimeout);

#if 0
printf("leaving select stat=%d errno=%d \n", status, MYERRNO);
#endif
        if (status<0) {
                if (MYERRNO == EINTR) {
                }
                else if (MYERRNO == EWOULDBLOCK) {
                        ca_printf("CAC: blocked at select ?\n");
                }                  
		else if (MYERRNO == ESRCH) {
		}
                else {
                        ca_printf (
                                "CAC: unexpected select fail: %s\n",
                                strerror(MYERRNO));
                }
        }

	LOCK;
        if (status>0) {
                for (	piiu = (IIU *) iiuList.node.next;
                        piiu;
                        piiu = (IIU *) piiu->node.next) {

                        if (!piiu->conn_up) {
                                continue;
                        }

                        if (FD_ISSET(piiu->sock_chan,&pfdi->writeMask)) {
                                (*piiu->sendBytes)(piiu);
                        }

                        if (FD_ISSET(piiu->sock_chan,&pfdi->readMask)) {
                                (*piiu->recvBytes)(piiu);
                        }
                }
        }

	ellDelete (&ca_static->fdInfoList, &pfdi->node);
	ellAdd (&ca_static->fdInfoFreeList, &pfdi->node);
	UNLOCK;

        return status;
}

