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

static char *sccsId = "@(#) $Id$";

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>

#include "osiSock.h"
#include "osiClock.h"
#include "ellLib.h"
#include "errlog.h"
#include "server.h"
#include "bsdSocketResource.h"


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

  	if(CASDEBUG>2 && pclient->send.stk){
		errlogPrintf(	"CAS: Sending a message of %d bytes\n",
			pclient->send.stk);
	}

	if(pclient->disconnect){
  		if(CASDEBUG>2){
			errlogPrintf("CAS: msg Discard for sock %d addr %x\n",
				pclient->sock,
				pclient->addr.sin_addr.s_addr);
		}
		return;
	}

	if(lock_needed){
		SEND_LOCK(pclient);
	}

	if(pclient->send.stk){
#ifdef CONVERSION_REQUIRED
		/*	Convert all caHdr into net format.
		 *	The remaining bytes must already be in
		 *	net format, because here we have no clue
		 *	how to convert them.
		 */
		char		*buf;
		unsigned long	msg_size, num_bytes;
		caHdr		*mp;

		
		buf       = (char *) pclient->send.buf;
		num_bytes = pclient->send.stk;

		/* convert only if we have at least a complete caHdr */
		while (num_bytes >= sizeof(caHdr))
		{
			mp = (caHdr *) buf;

			msg_size  = sizeof (caHdr) + mp->m_postsize;

			DLOG(3,"CAS: sending cmmd %d, postsize %d\n",
				mp->m_cmmd, (int)mp->m_postsize,
				0, 0, 0, 0);

			/* convert the complete header into host format */
			mp->m_cmmd      = htons (mp->m_cmmd);
			mp->m_postsize  = htons (mp->m_postsize);
			mp->m_dataType  = htons (mp->m_dataType);
			mp->m_count     = htons (mp->m_count);
			mp->m_cid       = htonl (mp->m_cid);
			mp->m_available = htonl (mp->m_available);

			/* get next message: */
			buf       += msg_size;
			num_bytes -= msg_size;
		}
#endif

  		status = sendto(	
			pclient->sock,
			pclient->send.buf,
			pclient->send.stk,
			NULL,
			(struct sockaddr *)&pclient->addr,
			sizeof(pclient->addr));
		if( pclient->send.stk != (unsigned)status){
			if(status < 0){
				int	anerrno;
				char	buf[64];

				anerrno = SOCKERRNO;

				ipAddrToA (&pclient->addr, buf, sizeof(buf));

				if(pclient->proto == IPPROTO_TCP) {
					if(     (anerrno!=ECONNABORTED&&
						anerrno!=ECONNRESET&&
						anerrno!=EPIPE&&
						anerrno!=ETIMEDOUT)||
						CASDEBUG>2){

						errlogPrintf(
			"CAS: TCP send to \"%s\" failed because \"%s\"\n",
							(int)buf,
							(int)SOCKERRSTR(anerrno));
					}
					pclient->disconnect = TRUE;
				}
				else if (pclient->proto == IPPROTO_UDP) {
					errlogPrintf(
			"CAS: UDP send to \"%s\" failed because \"%s\"\n",
							(int)buf,
							(int)SOCKERRSTR(anerrno));
				}
				else {
					assert (0);
				}
			}
			else{
				errlogPrintf(
				"CAS: blk sock partial send: req %d sent %d \n",
					pclient->send.stk,
					status);
			}
		}

  		pclient->send.stk = 0;

		pclient->ticks_at_last_send = clockGetRate();
	}


	if(lock_needed){
		SEND_UNLOCK(pclient);
	}

  	DLOG(3, "------------------------------\n\n", 0,0,0,0,0,0);

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
caHdr *cas_alloc_msg(pclient, extsize)
struct client	*pclient;	/* ptr to per client struct */
unsigned	extsize;	/* extension size */
{
	unsigned	msgsize;
	unsigned	newstack;
	
	extsize = CA_MESSAGE_ALIGN(extsize);

	msgsize = extsize + sizeof(caHdr);

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
	return (caHdr *) &pclient->send.buf[pclient->send.stk];
}
