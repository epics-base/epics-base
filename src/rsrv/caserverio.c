/*	
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	060791
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * 	Modification Log:
 * 	-----------------
 *	.01 joh 071591	log time of last io in the client structure
 *	.02 joh 091691	use greater than on the DEBUG level test
 *	.03 joh	110491	improved diagnostics
 *	.04 joh	021292	improved diagnostics
 *	.05 joh	022092	improved diagnostics
 *	.06 joh	031992	improved diagnostics
 */

static char *sccsId = "$Id$\t$Date$";

#include <vxWorks.h>
#include <lstLib.h>
#include <types.h>
#include <socket.h>
#include <ioLib.h>
#include <in.h>
#include <tcp.h>
#include <errno.h>
#include <server.h>


/*
 *
 *	cas_send_msg()
 *
 *	(channel access server send message)
 */
void cas_send_msg(pclient, lock_needed)
struct client	*pclient;
int		lock_needed;
{
	int	status;

  	if(CASDEBUG>2){
		logMsg(	"CAS: Sending a message of %d bytes\n",
			pclient->send.cnt);
	}

	if(pclient->disconnect){
  		if(CASDEBUG>2){
			logMsg(	"CAS: msg Discard for sock %d addr %x\n",
				pclient->sock,
				pclient->addr.sin_addr.s_addr);
		}
		return;
	}

	if(lock_needed){
		LOCK_CLIENT(pclient);
	}

	if(pclient->send.stk){
  		pclient->send.cnt = pclient->send.stk;

  		status = sendto(	
			pclient->sock,
			pclient->send.buf,
			pclient->send.cnt,
			NULL,
			&pclient->addr,
			sizeof(pclient->addr));
		if(status != pclient->send.cnt){
			if(status < 0){
				int	anerrno;

				anerrno = errnoGet(taskIdSelf());

				if(     (anerrno!=ECONNABORTED&&
					anerrno!=ECONNRESET&&
					anerrno!=EPIPE&&
					anerrno!=ETIMEDOUT)||
					CASDEBUG>2){

					logMsg(
		"CAS: client unreachable (errno=%d)\n",
						anerrno);	
				}
				pclient->disconnect = TRUE;
				if(pclient==prsrv_cast_client){
					taskSuspend(taskIdSelf());
				}
			}
			else{
				logMsg(
		"CAS: blk sock partial send: req %d sent %d \n",
					pclient->send.cnt,
					status);
			}
		}

  		pclient->send.stk = 0;

		pclient->ticks_at_last_io = tickGet();
	}


	if(lock_needed){
		UNLOCK_CLIENT(pclient);
	}

	return;
}



/*
 *
 *	cas_alloc_msg() 
 *
 *	see also ALLOC_MSG()/END_MSG() in server.h
 *
 *	(allocate space in the outgoing message buffer)
 *
 * 	send lock must be on while in this routine
 *
 *	returns		1)	a valid ptr to msg buffer space
 *			2)	NULL (msg will not fit)
 */			
struct extmsg *cas_alloc_msg(pclient, extsize)
struct client	*pclient;	/* ptr to per client struct */
unsigned	extsize;	/* extension size */
{
	unsigned	msgsize;
	unsigned	newstack;
	
	msgsize = extsize + sizeof(struct extmsg);
	newstack = pclient->send.stk + msgsize;
	if(newstack > pclient->send.maxstk){
		if(pclient->disconnect){
			pclient->send.stk = 0;
		}
		else{
			cas_send_msg(pclient, FALSE);
		}

		newstack = pclient->send.stk + msgsize;

		/*
		 * If dosnt fit now it never will
		 */
		if(newstack > pclient->send.maxstk){
			return NULL;
		}
	}

	/*
	 * it fits END_MSG will push it on the stack
	 */
	return (struct extmsg *) &pclient->send.buf[pclient->send.stk];
}
