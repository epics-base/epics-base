/*  
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   060791
 *
 *  Experimental Physics and Industrial Control System (EPICS)
 *
 *  Copyright 1991, the Regents of the University of California,
 *  and the University of Chicago Board of Governors.
 *
 *  This software was produced under  U.S. Government contracts:
 *  (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *  and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *  Initial development by:
 *      The Controls and Automation Group (AT-8)
 *      Ground Test Accelerator
 *      Accelerator Technology Division
 *      Los Alamos National Laboratory
 *
 *  Co-developed with
 *      The Controls and Computing Group
 *      Accelerator Systems Division
 *      Advanced Photon Source
 *      Argonne National Laboratory
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "dbDefs.h"
#include "osiSock.h"
#include "epicsTime.h"
#include "errlog.h"

typedef unsigned long arrayElementCount;

#include "net_convert.h"
#include "server.h"

/*
 *  cas_send_msg()
 *
 *  (channel access server send message)
 */
void cas_send_msg ( struct client *pclient, int lock_needed )
{
    int status;

    if ( CASDEBUG > 2 && pclient->send.stk ) {
        errlogPrintf ( "CAS: Sending a message of %d bytes\n", pclient->send.stk );
    }

    if ( pclient->disconnect ) {
        if ( CASDEBUG > 2 ) {
            errlogPrintf ( "CAS: msg Discard for sock %d addr %x\n",
                pclient->sock, pclient->addr.sin_addr.s_addr );
        }
        return;
    }

    if ( lock_needed ) {
        SEND_LOCK ( pclient );
    }

    while ( pclient->send.stk ) {
        status = sendto ( pclient->sock, pclient->send.buf, pclient->send.stk, 0,
                        (struct sockaddr *)&pclient->addr, sizeof(pclient->addr) );
        if ( status >= 0 ) {
            unsigned transferSize = (unsigned) status;
            if ( transferSize >= pclient->send.stk ) {
                pclient->send.stk = 0;
                epicsTimeGetCurrent ( &pclient->time_at_last_send );
                break;
            }
            else {
                unsigned bytesLeft = pclient->send.stk - transferSize;
                memmove ( pclient->send.buf, &pclient->send.buf[transferSize], 
                    bytesLeft );
                pclient->send.stk = bytesLeft;
            }
        }
        else {
            int anerrno = SOCKERRNO;
            char buf[64];

            if ( anerrno == SOCK_EINTR ) {
                continue;
            }

            ipAddrToDottedIP ( &pclient->addr, buf, sizeof(buf) );

            if(pclient->proto == IPPROTO_TCP) {
                if (    (anerrno!=SOCK_ECONNABORTED&&
                        anerrno!=SOCK_ECONNRESET&&
                        anerrno!=SOCK_EPIPE&&
                        anerrno!=SOCK_ETIMEDOUT)||
                        CASDEBUG>2){

                    errlogPrintf (
        "CAS: TCP send to \"%s\" failed because \"%s\"\n",
                        buf, SOCKERRSTR(anerrno));
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
            pclient->send.stk = 0u;
            break;
        }
    }

    if ( lock_needed ) {
        SEND_UNLOCK(pclient);
    }

    DLOG ( 3, ( "------------------------------\n\n" ) );

    return;
}

/*
 *
 *  cas_copy_in_header() 
 *
 *  Allocate space in the outgoing message buffer and
 *  copy in message header. Return pointer to message body.
 *
 *  send lock must be on while in this routine
 *
 *  Returns a valid ptr to message body or NULL if the msg 
 *  will not fit.
 */         
int cas_copy_in_header ( 
    struct client *pclient, ca_uint16_t response, ca_uint32_t payloadSize,
    ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
    ca_uint32_t responseSpecific, void **ppPayload )
{
    unsigned    msgSize;
    
    if ( payloadSize > UINT_MAX - sizeof ( caHdr ) - 8u ) {
        return FALSE;
    }

    payloadSize = CA_MESSAGE_ALIGN ( payloadSize );

    msgSize = payloadSize + sizeof ( caHdr );
    if ( payloadSize >= 0xffff || nElem >= 0xffff ) {
        if ( ! CA_V49 ( pclient->minor_version_number ) ) {
            return FALSE;
        }
        msgSize += 2 * sizeof ( ca_uint32_t );
    }

    if ( msgSize > pclient->send.maxstk ) {
        casExpandSendBuffer ( pclient, msgSize );
        if ( msgSize > pclient->send.maxstk ) {
            return FALSE;
        }
    }

    if ( pclient->send.stk > pclient->send.maxstk - msgSize ) {
        if ( pclient->disconnect ) {
            pclient->send.stk = 0;
        }
        else{
            cas_send_msg ( pclient, FALSE );
        }
    }

    if ( payloadSize < 0xffff && nElem < 0xffff ) {
        caHdr *pMsg = ( caHdr * ) &pclient->send.buf[pclient->send.stk];        
        pMsg->m_cmmd = htons ( response );   
        pMsg->m_postsize = htons ( ( ( ca_uint16_t ) payloadSize ) ); 
        pMsg->m_dataType = htons ( dataType );  
        pMsg->m_count = htons ( ( ( ca_uint16_t ) nElem ) );      
        pMsg->m_cid = htonl ( cid );          
        pMsg->m_available = htonl ( responseSpecific );  
        if ( ppPayload ) {
            *ppPayload = ( void * ) ( pMsg + 1 );
        }
    }
    else {
        caHdr *pMsg = ( caHdr * ) &pclient->send.buf[pclient->send.stk];
        ca_uint32_t *pW32 = ( ca_uint32_t * ) ( pMsg + 1 );
        pMsg->m_cmmd = htons ( response );   
        pMsg->m_postsize = htons ( 0xffff ); 
        pMsg->m_dataType = htons ( dataType );  
        pMsg->m_count = htons ( 0u );      
        pMsg->m_cid = htonl ( cid );          
        pMsg->m_available = htonl ( responseSpecific ); 
        pW32[0] = htonl ( payloadSize );
        pW32[1] = htonl ( nElem );
        if ( ppPayload ) {
            *ppPayload = ( void * ) ( pW32 + 2 );
        }
    }

    return TRUE;
}

void cas_set_header_cid ( struct client *pClient, ca_uint32_t cid )
{
    caHdr *pMsg = ( caHdr * ) &pClient->send.buf[pClient->send.stk];
    pMsg->m_cid = htonl ( cid );
}

void cas_commit_msg ( struct client *pClient, ca_uint32_t size )
{
    caHdr * pMsg = ( caHdr * ) &pClient->send.buf[pClient->send.stk];
    size = CA_MESSAGE_ALIGN ( size );
    if ( pMsg->m_postsize == htons ( 0xffff ) ) {
        ca_uint32_t * pLW = ( ca_uint32_t * ) ( pMsg + 1 );
        assert ( size <= ntohl ( *pLW ) );
        pLW[0] = htonl ( size );
        size += sizeof ( caHdr ) + 2 * sizeof ( *pLW );
    }
    else {
        assert ( size <= ntohs ( pMsg->m_postsize ) );
        pMsg->m_postsize = htons ( (ca_uint16_t) size );
        size += sizeof ( caHdr );
    }
    pClient->send.stk += size;
}

/*
 * this assumes that we have already checked to see 
 * if sufficent bytes are available
 */
ca_uint16_t rsrvGetUInt16 ( struct message_buffer *recv )
{
    ca_uint16_t tmp;
    /*
     * this assumes that we have already checked to see 
     * if sufficent bytes are available
     */
    assert ( recv->cnt - recv->stk >= 2u );
    tmp = recv->buf[recv->stk++];
    tmp <<= 8u;
    tmp |= recv->buf[recv->stk++];
    return tmp;
}

/*
 * this assumes that we have already checked to see 
 * if sufficent bytes are available
 */
ca_uint32_t rsrvGetUInt32 ( struct message_buffer *recv )
{
    ca_uint32_t tmp;
    /*
     * this assumes that we have already checked to see 
     * if sufficent bytes are available
     */
    assert ( recv->cnt - recv->stk >= 4u );
    tmp = recv->buf[recv->stk++] <<= 24u;
    tmp |= recv->buf[recv->stk++] << 16u;
    tmp |= recv->buf[recv->stk++] << 8u;
    tmp |= recv->buf[recv->stk++];
    return tmp;
}
