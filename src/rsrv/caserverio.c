/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*	
 *	Author:	Jeffrey O. Hill
 *		hill@luke.lanl.gov
 *		(505) 665 1831
 *	Date:	060791
 *
 */

static char *sccsId = "@(#) $Id$";

#include <string.h>
#include <errno.h>

#include <vxWorks.h>
#include "ellLib.h"
#include <types.h>
#include <socket.h>
#include <ioLib.h>
#include <in.h>
#include <netinet/tcp.h>
#include <logLib.h>
#include <sockLib.h>
#include <errnoLib.h>
#include <taskLib.h>
#include <tickLib.h>
#include <inetLib.h>

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
		logMsg(	"CAS: Sending a message of %d bytes\n",
			pclient->send.stk,
			NULL,
			NULL,
			NULL,
			NULL,
			NULL);
	}

	if(pclient->disconnect){
        pclient->send.stk = 0;
  		if(CASDEBUG>2){
			logMsg(	"CAS: msg Discard for sock %d addr %x\n",
				pclient->sock,
				pclient->addr.sin_addr.s_addr,
				NULL,
				NULL,
				NULL,
				NULL);
		}
		return;
	}

	if(lock_needed){
		SEND_LOCK(pclient);
	}

#ifdef CONVERSION_REQUIRED
    {
	    /*	Convert all caHdr into net format.
	    *	The remaining bytes must already be in
	    *	net format, because here we have no clue
	    *	how to convert them.
	    */
	    char * buf;
	    unsigned long msg_size, num_bytes;
	    caHdr * mp;

    	
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
		    mp->m_dataType      = htons (mp->m_dataType);
		    mp->m_count     = htons (mp->m_count);
		    mp->m_cid       = htonl (mp->m_cid);
		    mp->m_available = htonl (mp->m_available);

		    /* get next message: */
		    buf       += msg_size;
		    num_bytes -= msg_size;
	    }
    }
#endif

	while(pclient->send.stk&&!pclient->disconnect){

  		status = sendto(	
			pclient->sock,
			pclient->send.buf,
			pclient->send.stk,
			NULL,
			(struct sockaddr *)&pclient->addr,
			sizeof(pclient->addr));
		if( pclient->send.stk == (unsigned)status){
      		pclient->send.stk = 0;
    		pclient->ticks_at_last_send = tickGet();
		}
		else {
			if(status < 0){
				int	anerrno;
				char	buf[64];

				anerrno = errnoGet();

                if ( pclient->disconnect ) {
                    pclient->send.stk = 0;
                    break;
                }
				
				ipAddrToA (&pclient->addr, buf, sizeof(buf));

				if(pclient->proto == IPPROTO_TCP) {
				    if ( anerrno == ENOBUFS ) {
				        logMsg ( 
				        "rsrv: system low on network buffers - "
				            "tcp send retry in 15 sec\n",
				            0,0,0,0,0,0);
				        taskDelay ( 15 * sysClkRateGet() );
				        continue;
				    }

					if(     (anerrno!=ECONNABORTED&&
						anerrno!=ECONNRESET&&
						anerrno!=EPIPE&&
						anerrno!=ETIMEDOUT)||
						CASDEBUG>2){

						logMsg(
			"CAS: TCP send to \"%s\" failed because \"%s\"\n",
							(int)buf,
							(int)strerror(anerrno),
							NULL,
							NULL,
							NULL,
							NULL);	
					}
                    pclient->send.stk = 0;
					pclient->disconnect = TRUE;
                    {
                        static const int shutdownReadAndWrite = 2;
                        int status = shutdown ( pclient->sock, shutdownReadAndWrite );
                        if ( status ) {
                            logMsg (
                                "rsrv: socket shutdown error was %s\n", 
                                (int) strerror ( anerrno ), NULL, NULL,
                                NULL, NULL, NULL );
                        }
                    }
					break;
				}
				else if (pclient->proto == IPPROTO_UDP) {
					if (anerrno==ENOBUFS) {
						pclient->udpNoBuffCount++;
					}
					else {
						logMsg(
			"CAS: UDP send to \"%s\" failed because \"%s\"\n",
							(int)buf,
							(int)strerror(anerrno),
							NULL,
							NULL,
							NULL,
							NULL);	
					}
					break;
				}
				else {
					assert (0);
				}
			}
			else{
        	    pclient->ticks_at_last_send = tickGet();
				if(pclient->proto == IPPROTO_TCP) {
                    unsigned transferSize = (unsigned) status;
                    if ( pclient->send.stk > transferSize ) {
                        unsigned bytesLeft = pclient->send.stk - transferSize;
                        memmove ( pclient->send.buf, &pclient->send.buf[transferSize], 
                            bytesLeft );
                        pclient->send.stk = bytesLeft;
                    }
                    else {
                        pclient->send.stk = 0;
                    }
                }
                else if (pclient->proto == IPPROTO_UDP) {
                    pclient->send.stk = 0;
                    logMsg ( "partial UDP message sent?\n",
                        0,0,0,0,0,0);
                    break;
                }
                else {
                    assert ( 0 );
                }
			}
		}
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
