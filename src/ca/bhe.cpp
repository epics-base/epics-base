    
/*  $Id$
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

tsFreeList < class bhe > bhe::freeList;

/*
 * update beacon period
 *
 * updates beacon period, and looks for beacon anomalies
 */
bool bhe::updateBeaconPeriod (osiTime programBeginTime)
{
    double currentPeriod;
    bool netChange = false;
    osiTime current = osiTime::getCurrent ();

    if ( this->timeStamp == osiTime () ) {
        /* 
         * this is the 1st beacon seen - the beacon time stamp
         * was not initialized during BHE create because
         * a TCP/IP connection created the beacon.
         * (nothing to do but set the beacon time stamp and return)
         */
        this->timeStamp = current;

        /*
         * be careful about using beacons to reset the connection
         * time out watchdog until we have received a ping response 
         * from the IOC (this makes the software detect reconnects
         * faster when the server is rebooted twice in rapid 
         * succession before a 1st or 2nd beacon has been received)
         */
        if (this->piiu) {
            this->piiu->beaconAnomaly = TRUE;
        }

        return netChange;
    }

    /*
     * compute the beacon period (if we have seen at least two beacons)
     */
    currentPeriod = current - this->timeStamp;
    if ( this->averagePeriod < 0.0 ) {
        ca_real totalRunningTime;

        /*
         * this is the 2nd beacon seen. We cant tell about
         * the change in period at this point so we just
         * initialize the average period and return.
         */
        this->averagePeriod = currentPeriod;

        /*
         * be careful about using beacons to reset the connection
         * time out watchdog until we have received a ping response 
         * from the IOC (this makes the software detect reconnects
         * faster when the server is rebooted twice in rapid 
         * succession before a 2nd beacon has been received)
         */
        if (this->piiu) {
            this->piiu->beaconAnomaly = TRUE;
        }

        /*
         * ignore beacons seen for the first time shortly after
         * init, but do not ignore beacons arriving with a short
         * period because the IOC was rebooted soon after the 
         * client starts up.
         */
        totalRunningTime = this->timeStamp - programBeginTime;
        if ( currentPeriod <= totalRunningTime ) {
            netChange = true;
#           ifdef DEBUG
            {
                char name[64];

                ipAddrToA ( &this->inetAddr, name, sizeof (name) );
                ca_printf ( 
                    "new beacon from %s with period=%f running time to first beacon=%f\n", 
                        name, currentPeriod, totalRunningTime );
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
        if ( currentPeriod >= this->averagePeriod * 1.25 ) {

            if ( this->piiu ) {
                /* 
                 * trigger on any missing beacon 
                 * if connected to this server
                 */
                this->piiu->beaconAnomaly = true;
            }

            if ( currentPeriod >= this->averagePeriod * 3.25 ) {
                /* 
                 * trigger on any 3 contiguous missing beacons 
                 * if not connected to this server
                 */
                netChange = true;
            }
        }


#       ifdef   DEBUG
            if (netChange) {
                char name[64];

                ipAddrToA (&pBHE->inetAddr, name, sizeof(name));
                ca_printf (
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
        if ( currentPeriod <= this->averagePeriod * 0.80 ) {
#           ifdef DEBUG
            {
                char name[64];

                ipAddrToA ( &this->inetAddr, name, sizeof (name) );
                ca_printf (
                    "reboot seen %s cur=%f avg=%f\n", 
                    name, currentPeriod, this->averagePeriod);
            }
#           endif
            netChange = true;
            if ( this->piiu ) {
                this->piiu->beaconAnomaly = true;
            }
        }
    
        /*
         * update a running average period
         */
        this->averagePeriod = currentPeriod * 0.125 + this->averagePeriod * 0.875;
    }

    this->timeStamp = current;

    return netChange;
}

