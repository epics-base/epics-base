/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
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
#include <cantProceed.h>

#include "addrList.h"
#include "dbDefs.h"
#include "envDefs.h"
#include "errlog.h"
#include "osiSock.h"
#include "taskwd.h"

#include "server.h"

/*
 *  RSRV_ONLINE_NOTIFY_TASK
 */
void rsrv_online_notify_task(void *pParm)
{
    double                      delay;
    double                      maxdelay;
    long                        longStatus;
    double                      maxPeriod;
    caHdr                       msg;
    int                         status;
    ca_uint32_t                 beaconCounter = 0;
    int *lastError;

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

    memset((char *)&msg, 0, sizeof msg);
    msg.m_cmmd = htons (CA_PROTO_RSRV_IS_UP);
    msg.m_count = htons (ca_server_port);
    msg.m_dataType = htons (CA_MINOR_PROTOCOL_REVISION);

    /* beaconAddrList should not change after rsrv_init(), which then starts this thread */
    lastError = callocMustSucceed(ellCount(&beaconAddrList), sizeof(*lastError), "rsrv_online_notify_task lastError");

    epicsEventSignal(beacon_startStopEvent);

    while (TRUE) {
        ELLNODE *cur;
        unsigned i;

        /* send beacon to each interface */
        for(i=0, cur=ellFirst(&beaconAddrList); cur; i++, cur=ellNext(cur))
        {
            osiSockAddrNode *pAddr = CONTAINER(cur, osiSockAddrNode, node);
            status = sendto (beaconSocket, (char *)&msg, sizeof(msg), 0,
                             &pAddr->addr.sa, sizeof(pAddr->addr));
            if (status < 0) {
                int err = SOCKERRNO;
                if(err != lastError[i]) {
                    char sockErrBuf[64];
                    char sockDipBuf[22];

                    epicsSocketConvertErrorToString(sockErrBuf, sizeof(sockErrBuf), err);
                    ipAddrToDottedIP(&pAddr->addr.ia, sockDipBuf, sizeof(sockDipBuf));
                    errlogPrintf ( "CAS: CA beacon send to %s " ERL_ERROR ": %s\n",
                        sockDipBuf, sockErrBuf);

                    lastError[i] = err;
                }
            }
            else {
                assert (status == sizeof(msg));
                if(lastError[i]) {
                    char sockDipBuf[22];

                    ipAddrToDottedIP(&pAddr->addr.ia, sockDipBuf, sizeof(sockDipBuf));
                    errlogPrintf ( "CAS: CA beacon send to %s ok\n",
                        sockDipBuf);
                }
                lastError[i] = 0;
            }
        }

        epicsThreadSleep(delay);
        if (delay<maxdelay) {
            delay *= 2.0;
            if (delay>maxdelay) {
                delay = maxdelay;
            }
        }

        msg.m_cid = htonl ( beaconCounter++ ); /* expected to overflow */

        while (beacon_ctl == ctlPause) {
            epicsThreadSleep(0.1);
            delay = 0.02; /* Restart beacon timing if paused */
        }
    }

    free(lastError);
}


