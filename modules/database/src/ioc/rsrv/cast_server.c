/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   5-88
 *
 *  Improvements
 *  ------------
 *  .01
 *  Don't send channel found message unless there is memory, a task slot,
 *  and a TCP socket available. Send a diagnostic instead.
 *  Or ... make the timeout shorter? This is only a problem if
 *  they persist in trying to make a connection after getting no
 *  response.
 *
 *  Notes:
 *  ------
 *  .01
 *  Replies to broadcasts are not returned over
 *  an existing TCP connection to avoid a TCP
 *  pend which could lock up the cast server.
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "dbDefs.h"
#include "envDefs.h"
#include "epicsMutex.h"
#include "epicsTime.h"
#include "errlog.h"
#include "freeList.h"
#include "osiSock.h"
#include "epicsSock.h"
#include "epicsBaseDebugLog.h"
#include "taskwd.h"

#include "rsrv.h"
#include "server.h"

#define TIMEOUT 60.0 /* sec */

/*
 * clean_addrq
 */
static void clean_addrq(struct client *client)
{
    struct channel_in_use * pciu;
    struct channel_in_use * pnextciu;
    epicsTimeStamp current;
    double delay;
    double maxdelay = 0;
    unsigned ndelete=0;
    double timeout = TIMEOUT;
    int s;

    epicsTimeGetCurrent ( &current );

    epicsMutexMustLock ( client->chanListLock );
    pnextciu = (struct channel_in_use *)
            client->chanList.node.next;

    while( (pciu = pnextciu) ) {
        pnextciu = (struct channel_in_use *)pciu->node.next;

        delay = epicsTimeDiffInSeconds(&current,&pciu->time_at_creation);
        if (delay > timeout) {

            ellDelete(&client->chanList, &pciu->node);
            LOCK_CLIENTQ;
            s = bucketRemoveItemUnsignedId (
                pCaBucket,
                &pciu->sid);
            if(s){
                errMessage (s, "Bad id at close");
            }
            else {
                rsrvChannelCount--;
            }
            UNLOCK_CLIENTQ;
            if ( ! s ) {
                freeListFree(rsrvChanFreeList, pciu);
                ndelete++;
            }
            if(delay>maxdelay) maxdelay = delay;
        }
    }
    epicsMutexUnlock ( client->chanListLock );

#   ifdef DEBUG
    if(ndelete){
        epicsPrintf ("CAS: %d CA channels have expired after %f sec\n",
            ndelete, maxdelay);
    }
#   endif

}

/*
 * CAST_SERVER
 *
 * service UDP messages
 *
 */
void cast_server(void *pParm)
{
    rsrv_iface_config *conf = pParm;
    int                 status;
    int                 count=0;
    int                 mysocket=0;
    osiSockAddr46       new_recv_addr46;
    osiSockIoctl_t      nchars;
    SOCKET              recv_sock, reply_sock;
    struct client      *client;
#ifdef AF_INET6_IPV6
    unsigned int useIPv4 = 0;
    unsigned int useIPv6 = 0;
    /*
     *                            EPICS_CAS_AUTO_BEACON_ADDR_LIST
     *  EPICS_CA_AUTO_ADDR_LIST | NO        '',4,YES  6         46
     *   -----------------------+---------------------------------
     *           NO             | 4         4        46         46
     *           YES            | 4         4        46         46
     *           4              | 4         4        46         46
     *           6              | 6        46         6         46
     *           46             | 46       46        46         46
     */
    {
        char            addrautolistascii[32u];
        char            addrautobeaconlistascii[32u];
        char            *pAutoAddrList;
        char            *pAutoBeacon;
        memset(addrautolistascii, 0, sizeof (addrautolistascii));
        memset(addrautobeaconlistascii, 0, sizeof (addrautobeaconlistascii));
        /* EPICS_CA_AUTO_ADDR_LIST can enable/disable IPv4/IPv6 */
        pAutoAddrList = envGetConfigParam ( &EPICS_CA_AUTO_ADDR_LIST,
                                            sizeof (addrautolistascii), addrautolistascii );
        if (!pAutoAddrList) {
            pAutoAddrList = "";
        }
        pAutoBeacon = envGetConfigParam ( &EPICS_CAS_AUTO_BEACON_ADDR_LIST,
                                          sizeof (addrautobeaconlistascii), addrautobeaconlistascii );
        if (!pAutoBeacon) {
            pAutoBeacon = "";
        }
        /* Look at EPICS_CA_AUTO_ADDR_LIST. IPv4 is the default */
        if ( !strcmp( pAutoAddrList, "6" ) ) {
            useIPv4 = 0;
            useIPv6 = 1;
        } else if ( !strcmp( pAutoAddrList, "46" ) ) {
            useIPv4 = 1;
            useIPv6 = 1;
        } else if ( ( !strcmp( pAutoAddrList, "NO" ) ) ||
                    ( !strcmp( pAutoAddrList, "no" ) ) ) {
            useIPv4 = 0;
            useIPv6 = 0;
        } else {
            useIPv4 = 1;
            useIPv6 = 0;
        }
        /* Look at EPICS_CAS_AUTO_BEACON_ADDR_LIST. For each protocol that has beacons,
           we accept connections */
        if ( !strcmp( pAutoBeacon, "4" ) ) {
            useIPv4 = 1;
        }  else if ( !strcmp( pAutoBeacon, "6" ) ) {
            useIPv6 = 1;
        } else if ( !strcmp( pAutoBeacon, "46" ) ) {
            useIPv4 = 1;
            useIPv6 = 1;
        }
        epicsPrintf ("cast_server: EPICS_CA_AUTO_ADDR_LIST='%s' EPICS_CAS_AUTO_BEACON_ADDR_LIST='%s' useIPv4=%d useIPv6=%d\n",
                     addrautolistascii, addrautobeaconlistascii,
                     useIPv4, useIPv6);
    }
#endif
    reply_sock = conf->udp;

    /*
     * setup new client structure but reuse old structure if
     * possible
     *
     */
    while ( TRUE ) {
        client = create_client ( reply_sock, IPPROTO_UDP );
        if ( client ) {
            break;
        }
        epicsThreadSleep(300.0);
    }
    if (conf->startbcast) {
        recv_sock = conf->udpbcast;
        conf->bclient = client;
    }
    else {
        recv_sock = conf->udp;
        conf->client = client;
    }
    client->udpRecv = recv_sock;

    casAttachThreadToClient ( client );

    /*
     * add placeholder for the first version message should it be needed
     */
    rsrv_version_reply ( client );

    /* these pointers become invalid after signaling casudp_startStopEvent */
    conf = NULL;

    epicsEventSignal(casudp_startStopEvent);

    while (TRUE) {
        osiSocklen_t addrSize = ( osiSocklen_t ) sizeof ( new_recv_addr46 );
        status = epicsSocket46Recvfrom (
            recv_sock,
            client->recv.buf,
            client->recv.maxstk,
            0,
            &new_recv_addr46.sa, &addrSize);
        if (status < 0) {
            if (SOCKERRNO != SOCK_EINTR) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString (
                    sockErrBuf, sizeof ( sockErrBuf ) );
                epicsPrintf ("CAS: UDP recv error: %s\n",
                        sockErrBuf);
                epicsThreadSleep(1.0);
            }

        } else {
            size_t idx;
#ifdef AF_INET6_IPV6
            if ((new_recv_addr46.sa.sa_family == AF_INET) && !useIPv4) {
#ifdef NETDEBUG
                char buf[64];
                sockAddrToDottedIP(&new_recv_addr46.sa, buf, sizeof(buf));
                epicsBaseDebugLog("NET cast_server ignore request from '%s'\n",
                                  buf);
#endif
                continue;
            }
            if (new_recv_addr46.sa.sa_family == AF_INET6) {
                int useThisIp;
                useThisIp = (IN6_IS_ADDR_V4MAPPED(&new_recv_addr46.in6.sin6_addr)) ? useIPv4 : useIPv6;
                if (!useThisIp)
                {
#ifdef NETDEBUG
                    char buf[64];
                    sockAddrToDottedIP(&new_recv_addr46.sa, buf, sizeof(buf));
                    epicsBaseDebugLog("NET cast_server ignore request from '%s'\n",
                                      buf);
#endif
                    continue;
                }
            }
#endif
            for(idx=0; casIgnoreAddrs46[idx].ia.sin_port; idx++)
            {
                if (sockIPsAreIdentical46(&new_recv_addr46, &casIgnoreAddrs46[idx])) {
                    status = -1; /* ignore */
                    break;
                }
            }
        }

        if (status >= 0 && casudp_ctl == ctlRun) {
            client->recv.cnt = (unsigned) status;
            client->recv.stk = 0ul;
            epicsTimeGetCurrent(&client->time_at_last_recv);

            client->minor_version_number = CA_UKN_MINOR_VERSION;
            client->seqNoOfReq = 0;

            /*
             * If we are talking to a new client flush to the old one
             * in case we are holding UDP messages waiting to
             * see if the next message is for this same client.
             */
            if (client->send.stk>sizeof(caHdr)) {
                if (!(sockAddrAreIdentical46( &client->addr46, &new_recv_addr46))) {
                    /*
                     * if the address is different
                     */
                    cas_send_dg_msg(client);
                    client->addr46 = new_recv_addr46;
                }
            }
            else {
                client->addr46 = new_recv_addr46;
            }

            if (CASDEBUG>1) {
                char    buf[64];

                sockAddrToDottedIP (&client->addr46.sa, buf, sizeof(buf));
                errlogPrintf ("CAS: cast server msg of %d bytes from addr %s\n",
                    client->recv.cnt, buf);
            }

            if (CASDEBUG>2)
                count = ellCount (&client->chanList);

            status = camessage ( client );
            if(status == RSRV_OK){
                if(client->recv.cnt !=
                    client->recv.stk){
                    char buf[64];

                    sockAddrToDottedIP (&client->addr46.sa, buf, sizeof(buf));

                    epicsPrintf ("CAS: partial (damaged?) UDP msg of %d bytes from %s ?\n",
                        client->recv.cnt - client->recv.stk, buf);

                    epicsTimeToStrftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
                        &client->time_at_last_recv);
                    epicsPrintf ("CAS: message received at %s\n", buf);
                }
            }
            else if (CASDEBUG>0){
                char buf[64];

                sockAddrToDottedIP (&client->addr46.sa, buf, sizeof(buf));

                epicsPrintf ("CAS: invalid (damaged?) UDP request from %s ?\n", buf);

                epicsTimeToStrftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
                    &client->time_at_last_recv);
                epicsPrintf ("CAS: message received at %s\n", buf);
            }

            if (CASDEBUG>2) {
                if ( ellCount (&client->chanList) ) {
                    errlogPrintf ("CAS: Fnd %d name matches (%d tot)\n",
                        ellCount(&client->chanList)-count,
                        ellCount(&client->chanList));
                }
            }
        }

        /*
         * allow messages to batch up if more are coming
         */
        nchars = 0; /* suppress purify warning */
        status = socket_ioctl(recv_sock, FIONREAD, &nchars);
        if (status<0) {
            errlogPrintf ("CA cast server: Unable to fetch N characters pending\n");
            cas_send_dg_msg (client);
            clean_addrq (client);
        }
        else if (nchars == 0) {
            cas_send_dg_msg (client);
            clean_addrq (client);
        }
    }

    /* ATM never reached, just a placeholder */

    if(!mysocket)
        client->sock = INVALID_SOCKET; /* only one cast_server should destroy the reply socket */
    destroy_client(client);
    epicsSocketDestroy(recv_sock);
}
