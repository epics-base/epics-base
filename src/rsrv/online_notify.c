/*
 *  $Id$
 *
 *  tell CA clients this a server has joined the network
 *
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   103090
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
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/*
 *  EPICS includes
 */
#include "osiSock.h"
#include "osiThread.h"
#include "osiPoolStatus.h"
#include "tsStamp.h"
#include "errlog.h"
#include "envDefs.h"
#include "server.h"

/*
 *  RSRV_ONLINE_NOTIFY_TASK
 */
int rsrv_online_notify_task()
{
    caAddrNode          *pNode;
    double              delay;
    double              maxdelay;
    long                longStatus;
    double              maxPeriod;
    caHdr               msg;
    int                 status;
    SOCKET              sock;
    int                 true = TRUE;
    unsigned short      port;
    
    taskwdInsert(threadGetIdSelf(),NULL,NULL);
    
    longStatus = envGetDoubleConfigParam (
                &EPICS_CA_BEACON_PERIOD, &maxPeriod);
    if (longStatus || maxPeriod<=0.0) {
        maxPeriod = 15.0;
        epicsPrintf ("EPICS \"%s\" float fetch failed\n",
                        EPICS_CA_BEACON_PERIOD.name);
        epicsPrintf ("Setting \"%s\" = %f\n",
            EPICS_CA_BEACON_PERIOD.name, maxPeriod);
    }
    
    delay = 0.02; /* initial beacon period in sec */
    maxdelay = maxPeriod;
    
    /* 
     *  Open the socket.
     *  Use ARPA Internet address format and datagram socket.
     *  Format described in <sys/socket.h>.
     */
    if( (sock = socket (AF_INET, SOCK_DGRAM, 0)) == SOCKET_ERROR){
        errlogPrintf ("CAS: online socket creation error\n");
        threadSuspendSelf ();
    }
    
    status = setsockopt (sock, SOL_SOCKET, SO_BROADCAST, 
                (char *)&true, sizeof(true));
    if (status<0) {
        errlogPrintf ("CAS: online socket set up error\n");
        threadSuspendSelf ();
    }
    
#if 0
    {
        struct sockaddr_in  recv_addr;

        memset((char *)&recv_addr, 0, sizeof recv_addr);
        recv_addr.sin_family = AF_INET;
        recv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* let slib pick lcl addr */
        recv_addr.sin_port = htons(0);   /* let slib pick port */
        status = bind(sock, (struct sockaddr *)&recv_addr, sizeof recv_addr);
        if(status<0)
            abort();
    }
#endif
    
    memset((char *)&msg, 0, sizeof msg);
    msg.m_cmmd = htons (CA_PROTO_RSRV_IS_UP);
    msg.m_count = htons (ca_server_port);
    
    ellInit (&beaconAddrList);
    
    /*
     * load user and auto configured
     * broadcast address list
     */
    port = caFetchPortConfig(&EPICS_CA_REPEATER_PORT, CA_REPEATER_PORT);
    caSetupBCastAddrList (&beaconAddrList, sock, port);
    
#   ifdef DEBUG
    caPrintAddrList (&beaconAddrList);
#   endif
    
    while (TRUE) {
        
        /*
         * check to see if we are running low on memory
         * and disable new channels if so
         */
        casSufficentSpaceInPool = osiSufficentSpaceInPool ();
  
        pNode = (caAddrNode *) ellFirst (&beaconAddrList);
        while (pNode) {
            char buf[64];
 
            status = connect (sock, &pNode->destAddr.sa, sizeof(pNode->destAddr.sa));
            if (status<0) {
                ipAddrToA (&pNode->destAddr.in, buf, sizeof(buf));
                errlogPrintf ( "%s: CA beacon routing (connect to \"%s\") error was \"%s\"\n",
                    __FILE__, buf, SOCKERRSTR(SOCKERRNO));
            }
            else {
                struct sockaddr_in if_addr;

                int size = sizeof (if_addr);
                status = getsockname (sock, (struct sockaddr *) &if_addr, &size);
                if (status<0) {
                    errlogPrintf ( "%s: CA beacon routing (getsockname) error was \"%s\"\n",
                        __FILE__, SOCKERRSTR(SOCKERRNO));
                }
                else if (if_addr.sin_family==AF_INET) {
                    msg.m_available = if_addr.sin_addr.s_addr;

ipAddrToA (&if_addr, buf, sizeof(buf));
printf ("**** Setting local address to \"%s\" - this may not work correctly ****\n", buf);

                    status = send (sock, (char *)&msg, sizeof(msg), 0);
                    if (status < 0) {
                        ipAddrToA (&pNode->destAddr.in, buf, sizeof(buf));
                        errlogPrintf ( "%s: CA beacon (send to \"%s\") error was \"%s\"\n",
                            __FILE__, buf, SOCKERRSTR(SOCKERRNO));
                    }
                    else {
                        assert (status == sizeof(msg));
                    }
                }
            }
            pNode = (caAddrNode *)pNode->node.next;
        }

        threadSleep(delay);
        if (delay<maxdelay) {
            delay *= 2.0;
            if (delay>maxdelay) {
                delay = maxdelay;
            }
        }
    }
}


