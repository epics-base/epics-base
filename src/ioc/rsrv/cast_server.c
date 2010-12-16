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

#include "osiSock.h"
#include "epicsMutex.h"
#include "dbDefs.h"
#include "errlog.h"
#include "taskwd.h"
#include "epicsTime.h"
#include "envDefs.h"
#include "freeList.h"

#define epicsExportSharedSymbols
#include "server.h"
#include "rsrv.h"
    
#define TIMEOUT 60.0 /* sec */

/*
 * clean_addrq
 */
static void clean_addrq(void)
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

    epicsMutexMustLock ( prsrv_cast_client->chanListLock );
    pnextciu = (struct channel_in_use *) 
            prsrv_cast_client->chanList.node.next;

    while( (pciu = pnextciu) ) {
        pnextciu = (struct channel_in_use *)pciu->node.next;

        delay = epicsTimeDiffInSeconds(&current,&pciu->time_at_creation);
        if (delay > timeout) {

            ellDelete(&prsrv_cast_client->chanList, &pciu->node);
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
    epicsMutexUnlock ( prsrv_cast_client->chanListLock );

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
    struct sockaddr_in  sin;    
    int                 status;
    int                 count=0;
    struct sockaddr_in  new_recv_addr;
    osiSocklen_t        recv_addr_size;
    unsigned short      port;
    osiSockIoctl_t      nchars;

    if ( envGetConfigParamPtr ( &EPICS_CAS_SERVER_PORT ) ) {
        port = envGetInetPortConfigParam ( &EPICS_CAS_SERVER_PORT, 
            (unsigned short) CA_SERVER_PORT );
    }
    else {
        port = envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT, 
            (unsigned short) CA_SERVER_PORT );
    }

    recv_addr_size = sizeof(new_recv_addr);

    if( IOC_cast_sock!=0 && IOC_cast_sock!=INVALID_SOCKET ) {
        epicsSocketDestroy ( IOC_cast_sock );
    }

    /* 
     *  Open the socket.
     *  Use ARPA Internet address format and datagram socket.
     */

    if ( ( IOC_cast_sock = epicsSocketCreate (AF_INET, SOCK_DGRAM, 0) ) == INVALID_SOCKET ) {
        epicsPrintf ("CAS: cast socket creation error\n");
        epicsThreadSuspendSelf ();
    }

    /*
     * some concern that vxWorks will run out of mBuf's
     * if this change is made
     *
     * joh 11-10-98
     */
#if 0
    {
        /*
         *
         * this allows for faster connects by queuing
         * additional incomming UDP search frames
         *
         * this allocates a 32k buffer
         * (uses a power of two)
         */
        int size = 1u<<15u;
        status = setsockopt (IOC_cast_sock, SOL_SOCKET,
                        SO_RCVBUF, (char *)&size, sizeof(size));
        if (status<0) {
            epicsPrintf ("CAS: unable to set cast socket size\n");
        }
    }
#endif

    epicsSocketEnableAddressUseForDatagramFanout ( IOC_cast_sock );
    
    /*  Zero the sock_addr structure */
    memset((char *)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    
    /* get server's Internet address */
    if( bind(IOC_cast_sock, (struct sockaddr *)&sin, sizeof (sin)) < 0){
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        epicsPrintf ("CAS: UDP server port bind error was \"%s\"\n", sockErrBuf );
        epicsSocketDestroy ( IOC_cast_sock );
        epicsThreadSuspendSelf ();
    }

    /*
     * setup new client structure but reuse old structure if
     * possible
     *
     */
    while ( TRUE ) {
        prsrv_cast_client = create_client ( IOC_cast_sock, IPPROTO_UDP );
        if ( prsrv_cast_client ) {
            break;
        }
        epicsThreadSleep(300.0);
    }

    casAttachThreadToClient ( prsrv_cast_client );

    /*
     * add placeholder for the first version message should it be needed
     */
    rsrv_version_reply ( prsrv_cast_client );

    epicsEventSignal(casudp_startStopEvent);

    while (TRUE) {
        status = recvfrom (
            IOC_cast_sock,
            prsrv_cast_client->recv.buf,
            prsrv_cast_client->recv.maxstk,
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
        }
        else if (casudp_ctl == ctlRun) {
            prsrv_cast_client->recv.cnt = (unsigned) status;
            prsrv_cast_client->recv.stk = 0ul;
            epicsTimeGetCurrent(&prsrv_cast_client->time_at_last_recv);

            prsrv_cast_client->minor_version_number = 0;
            prsrv_cast_client->seqNoOfReq = 0;

            /*
             * If we are talking to a new client flush to the old one 
             * in case we are holding UDP messages waiting to 
             * see if the next message is for this same client.
             */
            if (prsrv_cast_client->send.stk>sizeof(caHdr)) {
                status = memcmp( (void *)&prsrv_cast_client->addr, (void *)&new_recv_addr, recv_addr_size);
                if(status){     
                    /* 
                     * if the address is different 
                     */
                    cas_send_dg_msg(prsrv_cast_client);
                    prsrv_cast_client->addr = new_recv_addr;
                }
            }
            else {
                prsrv_cast_client->addr = new_recv_addr;
            }

            if (CASDEBUG>1) {
                char    buf[40];
    
                ipAddrToDottedIP (&prsrv_cast_client->addr, buf, sizeof(buf));
                errlogPrintf ("CAS: cast server msg of %d bytes from addr %s\n", 
                    prsrv_cast_client->recv.cnt, buf);
            }

            if (CASDEBUG>2)
                count = ellCount (&prsrv_cast_client->chanList);

            status = camessage ( prsrv_cast_client );
            if(status == RSRV_OK){
                if(prsrv_cast_client->recv.cnt != 
                    prsrv_cast_client->recv.stk){
                    char buf[40];
        
                    ipAddrToDottedIP (&prsrv_cast_client->addr, buf, sizeof(buf));

                    epicsPrintf ("CAS: partial (damaged?) UDP msg of %d bytes from %s ?\n",
                        prsrv_cast_client->recv.cnt-prsrv_cast_client->recv.stk, buf);
                }
            }
            else {
                char buf[40];
    
                ipAddrToDottedIP (&prsrv_cast_client->addr, buf, sizeof(buf));

                epicsPrintf ("CAS: invalid (damaged?) UDP request from %s ?\n", buf);
            }

            if (CASDEBUG>2) {
                if ( ellCount (&prsrv_cast_client->chanList) ) {
                    errlogPrintf ("CAS: Fnd %d name matches (%d tot)\n",
                        ellCount(&prsrv_cast_client->chanList)-count,
                        ellCount(&prsrv_cast_client->chanList));
                }
            }
        }

        /*
         * allow messages to batch up if more are comming
         */
        nchars = 0; /* supress purify warning */
        status = socket_ioctl(IOC_cast_sock, FIONREAD, &nchars);
        if (status<0) {
            errlogPrintf ("CA cast server: Unable to fetch N characters pending\n");
            cas_send_dg_msg (prsrv_cast_client);
            clean_addrq ();
        }
        else if (nchars == 0) {
            cas_send_dg_msg (prsrv_cast_client);
            clean_addrq ();
        }
    }
}
