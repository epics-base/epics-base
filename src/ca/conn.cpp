/* $Id$ */
/*
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "iocinf.h"

#ifdef DEBUG
#define LOGRETRYINTERVAL logRetryInterval(__FILE__, __LINE__);
LOCAL void logRetryInterval (pcac, char *pFN, unsigned lineno);
#else
#define LOGRETRYINTERVAL 
#endif

/*
 *  checkConnWatchdogs()
 */
void checkConnWatchdogs (cac *pcac)
{
    tcpiiu *piiu;
    ca_real delay;

    LOCK (pcac);

    piiu = (tcpiiu *) ellFirst (&pcac->ca_iiuList);
    while (piiu) {
        tcpiiu *pnextiiu = (tcpiiu *) ellNext (&piiu->node);

        /* 
         * mark connection for shutdown if outgoing messages
         * are not accepted by TCP/IP for EPICS_CA_CONN_TMO seconds
         */
        if (piiu->sendPending) {
            delay = tsStampDiffInSeconds (&pcac->currentTime, 
                            &piiu->timeAtSendBlock);
            if (delay>pcac->ca_connectTMO) {
                initiateShutdownTCPIIU (piiu);
            }
        }
        /* 
         * otherwise if we dont already have a send timer pending
         * mark the connection for shutdown if an echo response
         * times out
         */
        else if (piiu->echoPending) {

            delay = tsStampDiffInSeconds (&pcac->currentTime, 
                        &piiu->timeAtEchoRequest);
            if (delay > CA_ECHO_TIMEOUT) {
                initiateShutdownTCPIIU (piiu);
            }
        }

        piiu = pnextiiu;
    }

    UNLOCK (pcac);
}

/*
 *
 *  MANAGE_CONN
 *
 *  manages
 *  o retry of disconnected channels
 *  o connection heart beats
 *
 *
 */
void manage_conn (cac *pcac)
{
    tcpiiu *piiu;
    ca_real delay;

    /*
     * prevent recursion
     */
    if(pcac->ca_manage_conn_active){
        return;
    }

    pcac->ca_manage_conn_active = TRUE;

    /*
     * issue connection heartbeat
     * (if we dont see a beacon)
     */
    LOCK (pcac);
    for (piiu = (tcpiiu *) ellFirst (&pcac->ca_iiuList);
            piiu; piiu = (tcpiiu *) ellNext (&piiu->node) ) {

        if (piiu->state!=iiu_connected) {
            continue;
        }

        /*
         * remain backwards compatible with old servers
         * ( this isnt an echo request )
         */
        if(!CA_V43(CA_PROTOCOL_VERSION, piiu->minor_version_number)){
            int stmo;
            int rtmo;

            delay = tsStampDiffInSeconds (&pcac->currentTime, 
                        &piiu->timeAtEchoRequest);
            stmo = delay > CA_RETRY_PERIOD;
            delay = tsStampDiffInSeconds (&pcac->currentTime, 
                        &piiu->timeAtLastRecv);
            rtmo = delay > CA_RETRY_PERIOD;
            if(stmo && rtmo && !piiu->sendPending){
                piiu->timeAtEchoRequest = pcac->currentTime;
                noop_msg (piiu);
            }
            continue;
        }

        if (!piiu->echoPending) {
            /*
             * check delay since last beacon
             */
            delay = tsStampDiffInSeconds (&pcac->currentTime, 
                        &piiu->pBHE->timeStamp);
            if (delay>pcac->ca_connectTMO) {
                /*
                 * check delay since last virtual circuit receive
                 */
                delay = tsStampDiffInSeconds (&pcac->currentTime, 
                            &piiu->timeAtLastRecv);
                if (delay>pcac->ca_connectTMO) {
                    /*
                     * no activity - so ping through the virtual circuit
                     */
                    echo_request (piiu);
                }
            }
        }
    }
    UNLOCK (pcac);

    /*
     * Stop here if there are not any disconnected channels
     */
    if (!pcac->pudpiiu) {
        pcac->ca_manage_conn_active = FALSE;
        return;
    }
    if (pcac->pudpiiu->niiu.chidList.count == 0) {
        pcac->ca_manage_conn_active = FALSE;
        return;
    }

    pcac->ca_manage_conn_active = FALSE;
}

/*
 * update beacon period
 *
 * updates beacon period, and looks for beacon anomalies
 */
LOCAL int updateBeaconPeriod (cac *pcac, bhe *pBHE)
{
    ca_real currentPeriod;
    int netChange = FALSE;

    if (pBHE->timeStamp.secPastEpoch==0 && pBHE->timeStamp.nsec==0) {
        /* 
         * this is the 1st beacon seen - the beacon time stamp
         * was not initialized during BHE create because
         * a TCP/IP connection created the beacon.
         * (nothing to do but set the beacon time stamp and return)
         */
        pBHE->timeStamp = pcac->currentTime;

        /*
         * be careful about using beacons to reset the connection
         * time out watchdog until we have received a ping response 
         * from the IOC (this makes the software detect reconnects
         * faster when the server is rebooted twice in rapid 
         * succession before a 1st or 2nd beacon has been received)
         */
        if (pBHE->piiu) {
            pBHE->piiu->beaconAnomaly = TRUE;
        }

        return netChange;
    }

    /*
     * compute the beacon period (if we have seen at least two beacons)
     */
    currentPeriod = tsStampDiffInSeconds (&pcac->currentTime, 
                        &pBHE->timeStamp);
    if (pBHE->averagePeriod<0.0) {
        ca_real totalRunningTime;

        /*
         * this is the 2nd beacon seen. We cant tell about
         * the change in period at this point so we just
         * initialize the average period and return.
         */
        pBHE->averagePeriod = currentPeriod;

        /*
         * be careful about using beacons to reset the connection
         * time out watchdog until we have received a ping response 
         * from the IOC (this makes the software detect reconnects
         * faster when the server is rebooted twice in rapid 
         * succession before a 2nd beacon has been received)
         */
        if (pBHE->piiu) {
            pBHE->piiu->beaconAnomaly = TRUE;
        }

        /*
         * ignore beacons seen for the first time shortly after
         * init, but do not ignore beacons arriving with a short
         * period because the IOC was rebooted soon after the 
         * client starts up.
         */
        totalRunningTime = tsStampDiffInSeconds (&pBHE->timeStamp, 
                            &pcac->programBeginTime);
        if (currentPeriod<=totalRunningTime) {
            netChange = TRUE;
#           ifdef DEBUG
            {
                char name[64];

                ipAddrToA (&pBHE->inetAddr, name, sizeof(name));
                ca_printf (pcac, 
                    "new beacon from %s with period=%f running time to first beacon=%f\n", 
                        name, currentPeriod, totalRunningTime);
            }
#           endif
        }
    }
    else {

        /*
         * Is this an IOC seen because of a restored
         * network segment? 
         *
         * It may be possible to get false triggers here 
         * if the client is busy, but this does not cause
         * problems because the echo response will tell us 
         * that the server is available
         */
        if (currentPeriod >= pBHE->averagePeriod*1.25) {

            if (pBHE->piiu) {
                /* 
                 * trigger on any missing beacon 
                 * if connected to this server
                 */
                pBHE->piiu->beaconAnomaly = TRUE;
            }

            if (currentPeriod >= pBHE->averagePeriod*3.25) {
                /* 
                 * trigger on any 3 contiguous missing beacons 
                 * if not connected to this server
                 */
                netChange = TRUE;
            }
        }


#       ifdef   DEBUG
            if (netChange) {
                char name[64];

                ipAddrToA (&pBHE->inetAddr, name, sizeof(name));
                ca_printf (pcac,  
                    "net resume seen %s cur=%f avg=%f\n", 
                    name, currentPeriod, pBHE->averagePeriod);
            }
#       endif

        /*
         * Is this an IOC seen because of an IOC reboot
         * (beacon come at a higher rate just after the
         * IOC reboots). Lower tolarance here because we
         * dont have to worry about lost beacons.
         *
         * It may be possible to get false triggers here 
         * if the client is busy, but this does not cause
         * problems because the echo response will tell us 
         * that the server is available
         */
        if (currentPeriod <= pBHE->averagePeriod * 0.80) {
#           ifdef DEBUG
            {
                char name[64];

                ipAddrToA (&pBHE->inetAddr, name, sizeof(name));
                ca_printf (pcac,
                    "reboot seen %s cur=%f avg=%f\n", 
                    name, currentPeriod, pBHE->averagePeriod);
            }
#           endif
            netChange = TRUE;
            if (pBHE->piiu) {
                pBHE->piiu->beaconAnomaly = TRUE;
            }
        }
    
        /*
         * update a running average period
         */
        pBHE->averagePeriod = currentPeriod*0.125 + pBHE->averagePeriod*0.875;
    }

    /*
     * update beacon time stamp 
     */
    pBHE->timeStamp = pcac->currentTime;

    return netChange;
}

/*
 *  MARK_SERVER_AVAILABLE
 */
void mark_server_available (cac *pcac, const struct sockaddr_in *pnet_addr)
{
    nciu *chan;
    bhe *pBHE;
    unsigned port;  
    int netChange;

    if (!pcac->pudpiiu) {
        return;
    }

    LOCK (pcac);
    /*
     * look for it in the hash table
     */
    pBHE = lookupBeaconInetAddr (pcac, pnet_addr);
    if (pBHE) {

        netChange = updateBeaconPeriod (pcac, pBHE);

        /*
         * update state of health for active virtual circuits 
         * (only if we are not suspicious about past beacon changes
         * until the next echo reply)
         */
        if (pBHE->piiu) {
            if (!pBHE->piiu->beaconAnomaly) {
                    pBHE->piiu->timeAtLastRecv = pcac->currentTime;
            }
        }
    }
    else {
        /*
         * This is the first beacon seen from this server.
         * Wait until 2nd beacon is seen before deciding
         * if it is a new server (or just the first
         * time that we have seen a server's beacon
         * shortly after the program started up)
         */
        netChange = FALSE;
        createBeaconHashEntry (pcac, pnet_addr, TRUE);
    }

    if (!netChange) {
        UNLOCK (pcac);
        return;
    }

    /*
     * This part is needed when many machines
     * have channels in a disconnected state that 
     * dont exist anywhere on the network. This insures
     * that we dont have many CA clients synchronously
     * flooding the network with broadcasts (and swamping
     * out requests for valid channels).
     *
     * I fetch the local port number and use the low order bits
     * as a pseudo random delay to prevent every one 
     * from replying at once.
     */
    {
        struct sockaddr_in  saddr;
        int                 saddr_length = sizeof(saddr);
        int                 status;

        status = getsockname (
                pcac->pudpiiu->sock,
                (struct sockaddr *)&saddr,
                &saddr_length);
        assert (status>=0);
        port = ntohs(saddr.sin_port);
    }

    {
        ca_real     delay;

        delay = (port&CA_RECAST_PORT_MASK);
        delay /= MSEC_PER_SEC;
        delay += CA_RECAST_DELAY;

        pcac->pudpiiu->searchTmr.reset (delay);
    }

    /*
     * set retry count of all disconnected channels
     * to zero
     */
    chan = (nciu *) ellFirst (&pcac->pudpiiu->niiu.chidList);
    while (chan) {
        chan->retry = 0u;
        chan = (nciu *) ellNext (&chan->node);
    }

    UNLOCK (pcac);

#   if DEBUG
    {
        char buf[64];
        ipAddrToA (pnet_addr, buf, sizeof(buf));
        printf ("new server available: %s\n", buf);
    }
#   endif

}

/*
 * bhtHashIP ()
 */
LOCAL unsigned bhtHashIP (cac *pcac, const struct sockaddr_in *pina)
{
    unsigned index;

#if BHT_INET_ADDR_MASK != 0xff
#   error BHT_INET_ADDR_MASK changed - recode this routine !
#endif

    index = pina->sin_addr.s_addr;
    index ^= pina->sin_port;
    index = (index>>16u) ^ index;
    index = (index>>8u) ^ index;
    index &= BHT_INET_ADDR_MASK;
    assert(index<NELEMENTS(pcac->ca_beaconHash));
    return index;
}

/*
 * createBeaconHashEntry()
 *
 * LOCK must be applied
 */
bhe *createBeaconHashEntry(
cac *pcac,
const struct sockaddr_in *pina,
unsigned sawBeacon)
{
    bhe     *pBHE;
    unsigned    index;

    pBHE = lookupBeaconInetAddr(pcac, pina);
    if(pBHE){
        return pBHE;
    }

    index = bhtHashIP (pcac,pina);

    pBHE = (bhe *)calloc (1,sizeof(*pBHE));
    if(!pBHE){
        return NULL;
    }

#ifdef DEBUG
    {
        char name[64];

        ipAddrToA (pina, name, sizeof(name));
        ca_printf (pcac, "created beacon entry for %s\n", name);
    }
#endif

    /*
     * store the inet address
     */
    pBHE->inetAddr = *pina;

    /*
     * set average to -1.0 so that when the next beacon
     * occurs we can distinguish between:
     * o new server
     * o existing server's beacon we are seeing
     *  for the first time shortly after program
     *  start up
     */
    pBHE->averagePeriod = -1.0;

    /*
     * if creating this in response to a search reply
     * and not in response to a beacon then sawBeacon
     * is false and we set the beacon time stamp to
     * zero (so we can correctly compute the period
     * between the 1st and 2nd beacons)
     */
    if (sawBeacon) {
        LOCK (pcac)
        pBHE->timeStamp = pcac->currentTime;
        UNLOCK (pcac)
    }
    else {
        pBHE->timeStamp.secPastEpoch = 0;
        pBHE->timeStamp.nsec = 0;
    }

    /*
     * install in the hash table
     */
    pBHE->pNext = pcac->ca_beaconHash[index];
    pcac->ca_beaconHash[index] = pBHE;

    return pBHE;
}

/*
 * lookupBeaconInetAddr()
 *
 * LOCK must be applied
 */
bhe *lookupBeaconInetAddr (
cac *pcac,
const struct sockaddr_in *pina)
{
    bhe     *pBHE;
    unsigned    index;

    index = bhtHashIP (pcac,pina);

    pBHE = pcac->ca_beaconHash[index];
    while (pBHE) {
        if (    pBHE->inetAddr.sin_addr.s_addr == pina->sin_addr.s_addr &&
            pBHE->inetAddr.sin_port == pina->sin_port) {
            break;
        }
        pBHE = pBHE->pNext;
    }
    return pBHE;
}

/*
 * removeBeaconInetAddr()
 *
 * LOCK must be applied
 */
void removeBeaconInetAddr (
cac *pcac,
const struct sockaddr_in *pina)
{
    bhe     *pBHE;
    bhe     **ppBHE;
    unsigned    index;

    index = bhtHashIP (pcac,pina);

    ppBHE = &pcac->ca_beaconHash[index];
    pBHE = *ppBHE;
    while (pBHE) {
        if (    pBHE->inetAddr.sin_addr.s_addr == pina->sin_addr.s_addr &&
            pBHE->inetAddr.sin_port == pina->sin_port) {
            *ppBHE = pBHE->pNext;
            free (pBHE);
            return;
        }
        ppBHE = &pBHE->pNext;
        pBHE = *ppBHE;
    }
    assert (0);
}

/*
 * freeBeaconHash()
 *
 * LOCK must be applied
 */
void freeBeaconHash(struct cac *ca_temp)
{
    bhe     *pBHE;
    bhe     **ppBHE;
    int     len;

    len = NELEMENTS(ca_temp->ca_beaconHash);
    for(    ppBHE = ca_temp->ca_beaconHash;
        ppBHE < &ca_temp->ca_beaconHash[len];
        ppBHE++){

        pBHE = *ppBHE;
        while(pBHE){
            bhe     *pOld;

            pOld = pBHE;
            pBHE = pBHE->pNext;
            free(pOld);
        }
    }
}

/*
 * Add chan to iiu and guarantee that
 * one chan on the B cast iiu list is pointed to by
 * ca_pEndOfBCastList 
 */
void addToChanList (nciu *chan, netIIU *piiu)
{
    LOCK (piiu->iiu.pcas);
    if ( piiu == &piiu->iiu.pcas->pudpiiu->niiu ) {
        /*
         * add to the beginning of the list so that search requests for
         * this channel will be sent first (since the retry count is zero)
         */
        if (ellCount(&piiu->chidList)==0) {
            piiu->iiu.pcas->ca_pEndOfBCastList = chan;
        }
        /*
         * add to the front of the list so that
         * search requests for new channels will be sent first
         */
        chan->retry = 0u;
        ellInsert (&piiu->chidList, NULL, &chan->node);
    }
    else {
        /*
         * add to the beginning of the list until we
         * have sent the claim message (after which we
         * move it to the end of the list)
         */
        chan->claimPending = TRUE;
        ellInsert (&piiu->chidList, NULL, &chan->node);
    }
    chan->ciu.piiu = &piiu->iiu;
    UNLOCK (piiu->iiu.pcas);
}

/*
 * Remove chan from B-cast IIU and guarantee that
 * one chan on the list is pointed to by
 * ca_pEndOfBCastList 
 */
void removeFromChanList (nciu *chan)
{
    if ( chan->ciu.piiu == &chan->ciu.piiu->pcas->pudpiiu->niiu.iiu ) {
        if (chan->ciu.piiu->pcas->ca_pEndOfBCastList == chan) {
            if (ellPrevious(&chan->node)) {
                chan->ciu.piiu->pcas->ca_pEndOfBCastList = (nciu *)
                    ellPrevious (&chan->node);
            }
            else {
                chan->ciu.piiu->pcas->ca_pEndOfBCastList = (nciu *)
                    ellLast (&chan->ciu.piiu->pcas->pudpiiu->niiu.chidList);
            }
        }
        ellDelete (&chan->ciu.piiu->pcas->pudpiiu->niiu.chidList, &chan->node);
    }
    else {
        tcpiiu *piiu = iiuToTCPIIU (chan->ciu.piiu);
        if ( ellCount (&piiu->niiu.chidList) == 1 ) {
            initiateShutdownTCPIIU (piiu);
        }
        ellDelete (&piiu->niiu.chidList, &chan->node);
    }
    chan->ciu.piiu = NULL;
}

