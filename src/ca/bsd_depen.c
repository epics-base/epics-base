/*
 *      %W% %G%
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
 */
int cac_select_io(struct timeval *ptimeout, int flags)
{
        long            status;
        IIU             *piiu;
        unsigned long   freespace;
	int		maxfd;

        LOCK;

	maxfd = 0;
	for(    piiu=(IIU *)iiuList.node.next;
		piiu;
		piiu=(IIU *)piiu->node.next){

		if(!piiu->conn_up){
			continue;
		}

                /*
                 * Dont bother receiving if we have insufficient
                 * space for the maximum UDP message
                 */
                if (flags&CA_DO_RECVS) {
                        freespace = cacRingBufferWriteSize(&piiu->recv, TRUE);
                        if(freespace>=piiu->minfreespace){
				maxfd = max(maxfd,piiu->sock_chan);
                                FD_SET(piiu->sock_chan,&readch);
                        }
                }

                if (flags&CA_DO_SENDS) {
                        if(cacRingBufferReadSize(&piiu->send, FALSE)>0){
				maxfd = max(maxfd,piiu->sock_chan);
                                FD_SET(piiu->sock_chan,&writech);
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
        status = select(
                        maxfd+1,
                        &readch,
                        &writech,
                        NULL,
                        ptimeout);
#if 0
printf("leaving select stat=%d errno=%d \n", status, MYERRNO);
#endif
        if(status<0){
                if(MYERRNO == EINTR){
                }
                else if(MYERRNO == EWOULDBLOCK){
                        ca_printf("CAC: blocked at select ?\n");
                }                  
                else{
                        ca_printf(
                                "CAC: unexpected select fail: %s\n",
                                strerror(MYERRNO));
			return status;
                }
        }

        LOCK;
        if(status>0){
                for(    piiu=(IIU *)iiuList.node.next;
                        piiu;
                        piiu=(IIU *)piiu->node.next){

                        if(!piiu->conn_up){
                                continue;
                        }

                        if(flags&CA_DO_SENDS &&
                                FD_ISSET(piiu->sock_chan,&writech)){
                                (*piiu->sendBytes)(piiu);
                        }

                        if(flags&CA_DO_RECVS &&
                                FD_ISSET(piiu->sock_chan,&readch)){
                                (*piiu->recvBytes)(piiu);
                        }

                        FD_CLR(piiu->sock_chan,&readch);
                        FD_CLR(piiu->sock_chan,&writech);
                }
        }
        UNLOCK;

        return status;
}

