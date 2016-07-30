/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
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
 *  Dont send channel found message unless there is memory, a task slot,
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
#include "taskwd.h"

#define epicsExportSharedSymbols
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
    struct sockaddr_in  new_recv_addr;
    osiSocklen_t        recv_addr_size;
    osiSockIoctl_t      nchars;
    SOCKET              recv_sock, reply_sock;
    struct client      *client;

    recv_addr_size = sizeof(new_recv_addr);

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
        status = recvfrom (
            recv_sock,
            client->recv.buf,
            client->recv.maxstk,
            0,
            (struct sockaddr *)&new_recv_addr, 
            &recv_addr_size);
        if (status < 0) {
            if (SOCKERRNO != SOCK_EINTR) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                epicsPrintf ("CAS: UDP recv error (errno=%s)\n",
                        sockErrBuf);
                epicsThreadSleep(1.0);
            }

        } else {
            size_t idx;
            for(idx=0; casIgnoreAddrs[idx]; idx++)
            {
                if(new_recv_addr.sin_addr.s_addr==casIgnoreAddrs[idx]) {
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
                status = memcmp(&client->addr,
                    &new_recv_addr, recv_addr_size);
                if(status){     
                    /* 
                     * if the address is different 
                     */
                    cas_send_dg_msg(client);
                    client->addr = new_recv_addr;
                }
            }
            else {
                client->addr = new_recv_addr;
            }

            if (CASDEBUG>1) {
                char    buf[40];
    
                ipAddrToDottedIP (&client->addr, buf, sizeof(buf));
                errlogPrintf ("CAS: cast server msg of %d bytes from addr %s\n", 
                    client->recv.cnt, buf);
            }

            if (CASDEBUG>2)
                count = ellCount (&client->chanList);

            status = camessage ( client );
            if(status == RSRV_OK){
                if(client->recv.cnt !=
                    client->recv.stk){
                    char buf[40];

                    ipAddrToDottedIP (&client->addr, buf, sizeof(buf));

                    epicsPrintf ("CAS: partial (damaged?) UDP msg of %d bytes from %s ?\n",
                        client->recv.cnt - client->recv.stk, buf);

                    epicsTimeToStrftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
                        &client->time_at_last_recv);
                    epicsPrintf ("CAS: message received at %s\n", buf);
                }
            }
            else if (CASDEBUG>0){
                char buf[40];

                ipAddrToDottedIP (&client->addr, buf, sizeof(buf));

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
         * allow messages to batch up if more are comming
         */
        nchars = 0; /* supress purify warning */
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
