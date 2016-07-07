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
 *  tell CA clients this a server has joined the network
 *
 *  Author: Jeffrey O. Hill
 *      hill@luke.lanl.gov
 *      (505) 665 1831
 *  Date:   103090
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
#include "dbDefs.h"
#include "osiSock.h"
#include "errlog.h"
#include "envDefs.h"
#include "addrList.h"
#include "taskwd.h"

#define epicsExportSharedSymbols
#include "server.h"

/*
 * forcePort ()
 */
static void  forcePort (ELLLIST *pList, unsigned short port)
{
    osiSockAddrNode *pNode;

    pNode  = (osiSockAddrNode *) ellFirst ( pList );
    while ( pNode ) {
        if ( pNode->addr.sa.sa_family == AF_INET ) {
            pNode->addr.ia.sin_port = htons ( port );
        }
        pNode = (osiSockAddrNode *) ellNext ( &pNode->node );
    }
}

/*
 *  RSRV_ONLINE_NOTIFY_TASK
 */
void rsrv_online_notify_task(void *pParm)
{
    unsigned                    priorityOfSelf = epicsThreadGetPrioritySelf ();
    osiSockAddrNode             *pNode;
    double                      delay;
    double                      maxdelay;
    long                        longStatus;
    double                      maxPeriod;
    caHdr                       msg;
    int                         status;
    SOCKET                      sock;
    int                         intTrue = TRUE;
    unsigned short              port;
    ca_uint32_t                 beaconCounter = 0;
    char                        * pStr;
    int                         autoBeaconAddr;
    ELLLIST                     autoAddrList;
    char                        buf[16];
    unsigned                    priorityOfUDP;
    epicsThreadBooleanStatus    tbs;
    epicsThreadId               tid;
    
    taskwdInsert (epicsThreadGetIdSelf(),NULL,NULL);
    
    if ( envGetConfigParamPtr ( & EPICS_CAS_BEACON_PERIOD ) ) {
        longStatus = envGetDoubleConfigParam ( & EPICS_CAS_BEACON_PERIOD, & maxPeriod );
    }
    else {
        longStatus = envGetDoubleConfigParam ( & EPICS_CA_BEACON_PERIOD, & maxPeriod );
    }
    if (longStatus || maxPeriod<=0.0) {
        maxPeriod = 15.0;
        epicsPrintf ("EPICS \"%s\" float fetch failed\n",
                        EPICS_CAS_BEACON_PERIOD.name);
        epicsPrintf ("Setting \"%s\" = %f\n",
            EPICS_CAS_BEACON_PERIOD.name, maxPeriod);
    }
    
    delay = 0.02; /* initial beacon period in sec */
    maxdelay = maxPeriod;
    
    /* 
     *  Open the socket.
     *  Use ARPA Internet address format and datagram socket.
     *  Format described in <sys/socket.h>.
     */
    if ( (sock = epicsSocketCreate (AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        errlogPrintf ("CAS: online socket creation error\n");
        epicsThreadSuspendSelf ();
    }
    
    status = setsockopt (sock, SOL_SOCKET, SO_BROADCAST, 
                (char *)&intTrue, sizeof(intTrue));
    if (status<0) {
        errlogPrintf ("CAS: online socket set up error\n");
        epicsThreadSuspendSelf ();
    }

    {
        /*
         * this connect is to supress a warning message on Linux
         * when we shutdown the read side of the socket. If it
         * fails (and it will on old ip kernels) we just ignore 
         * the failure.
         */
        osiSockAddr sockAddr;
        sockAddr.ia.sin_family = AF_UNSPEC;
        sockAddr.ia.sin_port = htons ( 0 );
        sockAddr.ia.sin_addr.s_addr = htonl (0);
        connect ( sock, & sockAddr.sa, sizeof ( sockAddr.sa ) );
        shutdown ( sock, SHUT_RD );
    }
    
    memset((char *)&msg, 0, sizeof msg);
    msg.m_cmmd = htons (CA_PROTO_RSRV_IS_UP);
    msg.m_count = htons (ca_server_port);
    msg.m_dataType = htons (CA_MINOR_PROTOCOL_REVISION);
    
    ellInit ( & beaconAddrList );
    ellInit ( & autoAddrList );

    pStr = envGetConfigParam(&EPICS_CAS_AUTO_BEACON_ADDR_LIST, sizeof(buf), buf);
    if ( ! pStr ) {
	    pStr = envGetConfigParam(&EPICS_CA_AUTO_ADDR_LIST, sizeof(buf), buf);
    }
	if (pStr) {
		if (strstr(pStr,"no")||strstr(pStr,"NO")) {
			autoBeaconAddr = FALSE;
		}
		else if (strstr(pStr,"yes")||strstr(pStr,"YES")) {
			autoBeaconAddr = TRUE;
		}
		else {
			fprintf(stderr, 
		"CAS: EPICS_CA(S)_AUTO_ADDR_LIST = \"%s\"? Assuming \"YES\"\n", pStr);
			autoBeaconAddr = TRUE;
		}
	}
	else {
		autoBeaconAddr = TRUE;
	}

    /*
     * load user and auto configured
     * broadcast address list
     */
    if (envGetConfigParamPtr(&EPICS_CAS_BEACON_PORT)) {
        port = envGetInetPortConfigParam (&EPICS_CAS_BEACON_PORT, 
            (unsigned short) CA_REPEATER_PORT );
    }
    else {
        port = envGetInetPortConfigParam (&EPICS_CA_REPEATER_PORT, 
            (unsigned short) CA_REPEATER_PORT );
    }

    /*
     * discover beacon addresses associated with this interface
     */
    if ( autoBeaconAddr ) {
        osiSockAddr addr;
		ELLLIST tmpList;

		ellInit ( &tmpList );
        addr.ia.sin_family = AF_UNSPEC;
        osiSockDiscoverBroadcastAddresses (&tmpList, sock, &addr); 
        forcePort ( &tmpList, port );
		removeDuplicateAddresses ( &autoAddrList, &tmpList, 1 );
    }
            
    /*
     * by default use EPICS_CA_ADDR_LIST for the
     * beacon address list
     */
    {
        const ENV_PARAM *pParam;
        
        if (envGetConfigParamPtr(&EPICS_CAS_INTF_ADDR_LIST) || 
            envGetConfigParamPtr(&EPICS_CAS_BEACON_ADDR_LIST)) {
            pParam = &EPICS_CAS_BEACON_ADDR_LIST;
        }
        else {
            pParam = &EPICS_CA_ADDR_LIST;
        }
    
        /* 
         * add in the configured addresses
         */
        addAddrToChannelAccessAddressList (
            &autoAddrList, pParam, port,  pParam == &EPICS_CA_ADDR_LIST );
    }
 
    removeDuplicateAddresses ( &beaconAddrList, &autoAddrList, 0 );

    if ( ellCount ( &beaconAddrList ) == 0 ) {
        errlogPrintf ("The CA server's beacon address list was empty after initialization?\n");
    }
  
#   ifdef DEBUG
        printChannelAccessAddressList (&beaconAddrList);
#   endif

    tbs  = epicsThreadHighestPriorityLevelBelow ( priorityOfSelf, &priorityOfUDP );
    if ( tbs != epicsThreadBooleanStatusSuccess ) {
        priorityOfUDP = priorityOfSelf;
    }

    casudp_startStopEvent = epicsEventMustCreate(epicsEventEmpty);
    casudp_ctl = ctlPause;

    tid = epicsThreadCreate ( "CAS-UDP", priorityOfUDP,
        epicsThreadGetStackSize (epicsThreadStackMedium),
        cast_server, 0 );
    if ( tid == 0 ) {
        epicsPrintf ( "CAS: unable to start UDP daemon thread\n" );
    }

    epicsEventMustWait(casudp_startStopEvent);
    epicsEventSignal(beacon_startStopEvent);

    while (TRUE) {
        pNode = (osiSockAddrNode *) ellFirst (&beaconAddrList);
        while (pNode) {
            char buf[64];

            status = connect (sock, &pNode->addr.sa, 
                sizeof(pNode->addr.sa));
            if (status<0) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                ipAddrToDottedIP (&pNode->addr.ia, buf, sizeof(buf));
                errlogPrintf ( "%s: CA beacon routing (connect to \"%s\") error was \"%s\"\n",
                    __FILE__, buf, sockErrBuf);
            }
            else {
                struct sockaddr_in if_addr;

                osiSocklen_t size = sizeof (if_addr);
                status = getsockname (sock, (struct sockaddr *) &if_addr, &size);
                if (status<0) {
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                    errlogPrintf ( "%s: CA beacon routing (getsockname) error was \"%s\"\n",
                        __FILE__, sockErrBuf);
                }
                else if (if_addr.sin_family==AF_INET) {
                    msg.m_available = if_addr.sin_addr.s_addr;
                    msg.m_cid = htonl ( beaconCounter );

                    status = send (sock, (char *)&msg, sizeof(msg), 0);
                    if (status < 0) {
                        char sockErrBuf[64];
                        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
                        ipAddrToDottedIP (&pNode->addr.ia, buf, sizeof(buf));
                        errlogPrintf ( "%s: CA beacon (send to \"%s\") error was \"%s\"\n",
                            __FILE__, buf, sockErrBuf);
                    }
                    else {
                        assert (status == sizeof(msg));
                    }
                }
            }
            pNode = (osiSockAddrNode *) pNode->node.next;
        }

        epicsThreadSleep(delay);
        if (delay<maxdelay) {
            delay *= 2.0;
            if (delay>maxdelay) {
                delay = maxdelay;
            }
        }

        beaconCounter++; /* expected to overflow */

        while (beacon_ctl == ctlPause) {
            epicsThreadSleep(0.1);
            delay = 0.02; /* Restart beacon timing if paused */
        }
    }
}


