/*
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   5-88
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
#include "tsStamp.h"
#include "osiThread.h"
#include "errlog.h"
#include "ellLib.h"
#include "taskwd.h"
#include "db_access.h"
#include "envDefs.h"
#include "freeList.h"
#include "serverInclude.h"
#define epicsExportSharedSymbols
#include "server.h"
    
#define     TIMEOUT 60.0 /* sec */

/*
 * clean_addrq
 */
LOCAL void clean_addrq()
{
    struct channel_in_use   *pciu;
    struct channel_in_use   *pnextciu;
    TS_STAMP        current;
    double          delay;
    double          maxdelay = 0;
    unsigned        ndelete=0;
    double          timeout = TIMEOUT;
    int         s;

    tsStampGetCurrent(&current);

    semMutexMustTake(prsrv_cast_client->addrqLock);
    pnextciu = (struct channel_in_use *) 
            prsrv_cast_client->addrq.node.next;

    while( (pciu = pnextciu) ) {
        pnextciu = (struct channel_in_use *)pciu->node.next;

        delay = tsStampDiffInSeconds(&current,&pciu->time_at_creation);
        if (delay > timeout) {

            ellDelete(&prsrv_cast_client->addrq, &pciu->node);
                LOCK_CLIENTQ;
            s = bucketRemoveItemUnsignedId (
                pCaBucket,
                &pciu->sid);
            if(s){
                errMessage (s, "Bad id at close");
            }
                UNLOCK_CLIENTQ;
            freeListFree(rsrvChanFreeList, pciu);
            ndelete++;
            if(delay>maxdelay) maxdelay = delay;
        }
    }
    semMutexGive(prsrv_cast_client->addrqLock);

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
int cast_server(void)
{
    struct sockaddr_in  sin;    
    int         status;
    int         count=0;
    struct sockaddr_in  new_recv_addr;
    int             recv_addr_size;
    unsigned short      port;
    int         nchars;
    threadId        tid;

    taskwdInsert(threadGetIdSelf(),NULL,NULL);

    port = caFetchPortConfig(&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

    recv_addr_size = sizeof(new_recv_addr);

    if( IOC_cast_sock!=0 && IOC_cast_sock!=INVALID_SOCKET ) {
        if( (status = socket_close(IOC_cast_sock)) < 0 ) {
            epicsPrintf ("CAS: Unable to close master cast socket\n");
        }
    }

    /* 
     *  Open the socket.
     *  Use ARPA Internet address format and datagram socket.
     */

    if ((IOC_cast_sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
        epicsPrintf ("CAS: cast socket creation error\n");
        threadSuspendSelf ();
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
    
    /*  Zero the sock_addr structure */
    memset((char *)&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    
    /* get server's Internet address */
    if( bind(IOC_cast_sock, (struct sockaddr *)&sin, sizeof (sin)) < 0){
        epicsPrintf ("CAS: cast bind error\n");
        socket_close (IOC_cast_sock);
        threadSuspendSelf ();
    }
    
    /* tell clients we are on line again */
    tid = threadCreate("CAonline",threadPriorityChannelAccessClient-3,
    threadGetStackSize(threadStackSmall),
    (THREADFUNC)rsrv_online_notify_task,0);
    if(tid == 0) {
        epicsPrintf ("CAS: couldnt start up online notify task because \"%s\"\n",
            strerror(errno));
    }
    
 
    /*
     * setup new client structure but reuse old structure if
     * possible
     *
     */
    while (TRUE) {
        prsrv_cast_client = create_base_client ();
        if (prsrv_cast_client) {
            break;
        }
        threadSleep(300.0);
    }

    prsrv_cast_client->sock = IOC_cast_sock;
    prsrv_cast_client->tid = threadGetIdSelf ();
    
    while (TRUE) {
        status = recvfrom (
            IOC_cast_sock,
            prsrv_cast_client->recv.buf,
            sizeof(prsrv_cast_client->recv.buf),
            NULL,
            (struct sockaddr *)&new_recv_addr, 
            &recv_addr_size);
        if (status<0) {
            epicsPrintf ("CAS: UDP recv error (errno=%s)\n",
                    SOCKERRSTR(SOCKERRNO));
        threadSleep(1.0);
        }
        else {
            prsrv_cast_client->recv.cnt = (unsigned long) status;
            prsrv_cast_client->recv.stk = 0ul;
        tsStampGetCurrent(&prsrv_cast_client->time_at_last_recv);

            /*
             * If we are talking to a new client flush to the old one 
             * in case we are holding UDP messages waiting to 
             * see if the next message is for this same client.
             */
            if (prsrv_cast_client->send.stk) {
                status = memcmp( (void *)&prsrv_cast_client->addr, (void *)&new_recv_addr, recv_addr_size);
                if(status){     
                    /* 
                     * if the address is different 
                     */
                    cas_send_msg(prsrv_cast_client, TRUE);
                    prsrv_cast_client->addr = new_recv_addr;
                }
            }
            else {
                prsrv_cast_client->addr = new_recv_addr;
            }

            if (CASDEBUG>1) {
                char    buf[40];
    
                ipAddrToA (&prsrv_cast_client->addr, buf, sizeof(buf));
                errlogPrintf ("CAS: cast server msg of %d bytes from addr %s\n", 
                    prsrv_cast_client->recv.cnt, buf);
            }

            if (CASDEBUG>2)
                count = ellCount (&prsrv_cast_client->addrq);

            status = camessage(
                prsrv_cast_client,&prsrv_cast_client->recv);
            if(status == RSRV_OK){
                if(prsrv_cast_client->recv.cnt != 
                    prsrv_cast_client->recv.stk){
                    char buf[40];
        
                    ipAddrToA (&prsrv_cast_client->addr, buf, sizeof(buf));

                    epicsPrintf ("CAS: partial (damaged?) UDP msg of %d bytes from %s ?\n",
                        prsrv_cast_client->recv.cnt-prsrv_cast_client->recv.stk, buf);
                }
            }
            else {
                char buf[40];
    
                ipAddrToA (&prsrv_cast_client->addr, buf, sizeof(buf));

                epicsPrintf ("CAS: invalid (damaged?) UDP request from %s ?\n", buf);
            }

            if (CASDEBUG>2) {
                if ( ellCount (&prsrv_cast_client->addrq) ) {
                    errlogPrintf ("CAS: Fnd %d name matches (%d tot)\n",
                        ellCount(&prsrv_cast_client->addrq)-count,
                        ellCount(&prsrv_cast_client->addrq));
                }
            }
        }

        /*
         * allow messages to batch up if more are comming
         */
        status = socket_ioctl(IOC_cast_sock, FIONREAD, &nchars);
        if (status<0) {
            errlogPrintf ("CA cast server: Unable to fetch N characters pending\n");
            cas_send_msg (prsrv_cast_client, TRUE);
            clean_addrq ();
        }
        else if (nchars == 0) {
            cas_send_msg (prsrv_cast_client, TRUE);
            clean_addrq ();
        }
    }
}
