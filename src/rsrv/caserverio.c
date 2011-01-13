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
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   060791
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
#include "epicsSignal.h"
#include "errlog.h"
#include "caerr.h"

#include "net_convert.h"

#define epicsExportSharedSymbols
#include "server.h"

/*
 *  cas_send_bs_msg()
 *
 *  (channel access server send message)
 */
void cas_send_bs_msg ( struct client *pclient, int lock_needed )
{
    int status;

    if ( CASDEBUG > 2 && pclient->send.stk ) {
        errlogPrintf ( "CAS: Sending a message of %d bytes\n", pclient->send.stk );
    }

    if ( pclient->disconnect ) {
        if ( CASDEBUG > 2 ) {
            errlogPrintf ( "CAS: msg Discard for sock %d addr %x\n",
                pclient->sock, (unsigned) pclient->addr.sin_addr.s_addr );
        }
        pclient->send.stk = 0u;
        return;
    }

    if ( lock_needed ) {
        SEND_LOCK ( pclient );
    }

    while ( pclient->send.stk && ! pclient->disconnect ) {
        status = send ( pclient->sock, pclient->send.buf, pclient->send.stk, 0 );
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
            int causeWasSocketHangup = 0;
            int anerrno = SOCKERRNO;
            char buf[64];

            if ( pclient->disconnect ) {
                pclient->send.stk = 0u;
                break;
            }

            if ( anerrno == SOCK_EINTR ) {
                continue;
            }

            if ( anerrno == SOCK_ENOBUFS ) {
                errlogPrintf ( "rsrv: system low on network buffers - send retry in 15 seconds\n" );
                epicsThreadSleep ( 15.0 );
                continue;
            }

            ipAddrToDottedIP ( &pclient->addr, buf, sizeof(buf) );

            if (    
                anerrno == SOCK_ECONNABORTED ||
                anerrno == SOCK_ECONNRESET ||
                anerrno == SOCK_EPIPE ||
                anerrno == SOCK_ETIMEDOUT ) {
                causeWasSocketHangup = 1;
            }
            else {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf (
    "CAS: TCP send to \"%s\" failed because \"%s\"\n",
                    buf, sockErrBuf);
            }
            pclient->disconnect = TRUE;
            pclient->send.stk = 0u;

            /*
             * wakeup the receive thread
             */
            if ( ! causeWasSocketHangup ) {
                enum epicsSocketSystemCallInterruptMechanismQueryInfo info  =
                    epicsSocketSystemCallInterruptMechanismQuery ();
                switch ( info ) {
                case esscimqi_socketCloseRequired:
                    if ( pclient->sock != INVALID_SOCKET ) {
                        epicsSocketDestroy ( pclient->sock );
                        pclient->sock = INVALID_SOCKET;
                    }
                    break;
                case esscimqi_socketBothShutdownRequired:
                    {
                        int status = shutdown ( pclient->sock, SHUT_RDWR );
                        if ( status ) {
                            char sockErrBuf[64];
                            epicsSocketConvertErrnoToString ( 
                                sockErrBuf, sizeof ( sockErrBuf ) );
                            errlogPrintf ("rsrv: socket shutdown error was %s\n", 
                                sockErrBuf );
                        }
                    }
                    break;
                case esscimqi_socketSigAlarmRequired:
                    epicsSignalRaiseSigAlarm ( pclient->tid );
                    break;
                default:
                    break;
                };
                break;
            }
        }
    }

    if ( lock_needed ) {
        SEND_UNLOCK(pclient);
    }

    DLOG ( 3, ( "------------------------------\n\n" ) );

    return;
}

/*
 *  cas_send_dg_msg()
 *
 *  (channel access server send udp message)
 */
void cas_send_dg_msg ( struct client * pclient )
{
    int status;
    int sizeDG;
    char * pDG; 
    caHdr * pMsg;

    if ( CASDEBUG > 2 && pclient->send.stk ) {
        errlogPrintf ( "CAS: Sending a udp message of %d bytes\n", pclient->send.stk );
    }

    SEND_LOCK ( pclient );

    if ( pclient->send.stk <= sizeof (caHdr) ) {
        SEND_UNLOCK(pclient);
        return;
    }

    pDG = pclient->send.buf;
    pMsg = ( caHdr * ) pDG;
    sizeDG = pclient->send.stk;
    assert ( ntohs ( pMsg->m_cmmd ) == CA_PROTO_VERSION );
    if ( CA_V411 ( pclient->minor_version_number ) ) {
        pMsg->m_cid = htonl ( pclient->seqNoOfReq );
        pMsg->m_dataType = htons ( sequenceNoIsValid );
    }
    else {
        pDG += sizeof (caHdr);
        sizeDG -= sizeof (caHdr);
    }

    status = sendto ( pclient->sock, pDG, sizeDG, 0,
       (struct sockaddr *)&pclient->addr, sizeof(pclient->addr) );
    if ( status >= 0 ) {
        if ( status >= sizeDG ) {
            epicsTimeGetCurrent ( &pclient->time_at_last_send );
        }
        else {
            errlogPrintf ( 
                "cas: system failed to send entire udp frame?\n" );
        }
    }
    else {
        char sockErrBuf[64];
        char buf[128];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        ipAddrToDottedIP ( &pclient->addr, buf, sizeof(buf) );
        errlogPrintf(
                    "CAS: UDP send to \"%s\" "
                    "failed because \"%s\"\n",
                    buf,
                    sockErrBuf);
    }

    pclient->send.stk = 0u;

    /*
     * add placeholder for the first version message should it be needed
     */
    rsrv_version_reply ( prsrv_cast_client );

    SEND_UNLOCK(pclient);

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
    ca_uint32_t alignedPayloadSize;
    caHdr *pMsg;

    if ( payloadSize > UINT_MAX - sizeof ( caHdr ) - 8u ) {
        return ECA_TOLARGE;
    }

    alignedPayloadSize = CA_MESSAGE_ALIGN ( payloadSize );

    msgSize = alignedPayloadSize + sizeof ( caHdr );
    if ( alignedPayloadSize >= 0xffff || nElem >= 0xffff ) {
        if ( ! CA_V49 ( pclient->minor_version_number ) ) {
            return ECA_16KARRAYCLIENT;
        }
        msgSize += 2 * sizeof ( ca_uint32_t );
    }

    if ( msgSize > pclient->send.maxstk ) {
        casExpandSendBuffer ( pclient, msgSize );
        if ( msgSize > pclient->send.maxstk ) {
            return ECA_TOLARGE;
        }
    }

    if ( pclient->send.stk > pclient->send.maxstk - msgSize ) {
        if ( pclient->disconnect ) {
            pclient->send.stk = 0;
        }
        else{
            if ( pclient->proto == IPPROTO_TCP) {
                cas_send_bs_msg ( pclient, FALSE );
            }
            else if ( pclient->proto == IPPROTO_UDP ) {
                cas_send_dg_msg ( pclient );
            }
            else {
                return ECA_INTERNAL;
            }
        }
    }

    pMsg = (caHdr *) &pclient->send.buf[pclient->send.stk];
    pMsg->m_cmmd = htons(response);
    pMsg->m_dataType = htons(dataType);
    pMsg->m_cid = htonl(cid);
    pMsg->m_available = htonl(responseSpecific);
    if (alignedPayloadSize < 0xffff && nElem < 0xffff) {
        pMsg->m_postsize = htons(((ca_uint16_t) alignedPayloadSize));
        pMsg->m_count = htons(((ca_uint16_t) nElem));
        if (ppPayload)
            *ppPayload = (void *) (pMsg + 1);
    }
    else {
        ca_uint32_t *pW32 = (ca_uint32_t *) (pMsg + 1);
        pMsg->m_postsize = htons(0xffff);
        pMsg->m_count = htons(0u);
        pW32[0] = htonl(alignedPayloadSize);
        pW32[1] = htonl(nElem);
        if (ppPayload)
            *ppPayload = (void *) (pW32 + 2);
    }

    /* zero out pad bytes */
    if ( alignedPayloadSize > payloadSize ) {
        char *p = ( char * ) *ppPayload;
        memset ( p + payloadSize, '\0', 
            alignedPayloadSize - payloadSize );
    }

    return ECA_NORMAL;
}

void cas_set_header_cid ( struct client *pClient, ca_uint32_t cid )
{
    caHdr *pMsg = ( caHdr * ) &pClient->send.buf[pClient->send.stk];
    pMsg->m_cid = htonl ( cid );
}

void cas_set_header_count (struct client *pClient, ca_uint32_t count)
{
    caHdr *pMsg = (caHdr *) &pClient->send.buf[pClient->send.stk];
    if (pMsg->m_postsize == htons(0xffff)) {
        ca_uint32_t *pLW;

        assert(pMsg->m_count == 0);
        pLW = (ca_uint32_t *) (pMsg + 1);
        pLW[1] = htonl(count);
    }
    else {
        assert(count < 65536);
        pMsg->m_count = htons((ca_uint16_t) count);
    }
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
    ca_uint8_t *pBuf = ( ca_uint8_t * ) recv->buf;
    ca_uint16_t result;
    /*
     * this assumes that we have already checked to see 
     * if sufficent bytes are available
     */
    assert ( recv->cnt - recv->stk >= 2u );
    result  = pBuf[recv->stk++] << 8u;
    result |= pBuf[recv->stk++] << 0u;
    return result;
}

/*
 * this assumes that we have already checked to see 
 * if sufficent bytes are available
 */
ca_uint32_t rsrvGetUInt32 ( struct message_buffer *recv )
{
    ca_uint8_t *pBuf = ( ca_uint8_t * ) recv->buf;
    ca_uint32_t result;
    /*
     * this assumes that we have already checked to see 
     * if sufficent bytes are available
     */
    assert ( recv->cnt - recv->stk >= 4u );
    result  = pBuf[recv->stk++] << 24u;
    result |= pBuf[recv->stk++] << 16u;
    result |= pBuf[recv->stk++] << 8u;
    result |= pBuf[recv->stk++] << 0u;
    return result;
}
