/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
    
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


#include <limits.h>
#include <float.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "virtualCircuit.h"
#include "bhe.h"

epicsSingleton < tsFreeList < class bhe, 1024 > > bhe::pFreeList;

void * bhe::operator new ( size_t size )
{ 
    return bhe::pFreeList->allocate ( size );
}

void bhe::operator delete ( void *pCadaver, size_t size )
{ 
    bhe::pFreeList->release ( pCadaver, size );
}

/*
 * set average to -1.0 so that when the next beacon
 * occurs we can distinguish between:
 * o new server
 * o existing server's beacon we are seeing
 *  for the first time shortly after program
 *  start up
 *
 * if creating this in response to a search reply
 * and not in response to a beacon then 
 * we set the beacon time stamp to
 * zero (so we can correctly compute the period
 * between the 1st and 2nd beacons)
 */
bhe::bhe ( const epicsTime & initialTimeStamp, const inetAddrID & addr ) :
    inetAddrID ( addr ), timeStamp ( initialTimeStamp ), averagePeriod ( - DBL_MAX ),
    lastBeaconNumber ( UINT_MAX )
{
#   ifdef DEBUG
    {
        char name[64];
        addr.name ( name, sizeof ( name ) );
        ::printf ( "created beacon entry for %s\n", name );
    }
#   endif
}

bhe::~bhe ()
{
}

void bhe::beaconAnomalyNotify ()
{
    tsDLIter < tcpiiu > iter = this->iiuList.firstIter ();
    while ( iter.valid() ) {
        iter->beaconAnomalyNotify ();
        iter++;
    }
}

/*
 * update beacon period
 *
 * updates beacon period, and looks for beacon anomalies
 */
bool bhe::updatePeriod ( const epicsTime & programBeginTime, 
    const epicsTime & currentTime, unsigned beaconNumber, 
    unsigned protocolRevision )
{
    if ( this->timeStamp == epicsTime () ) {

        if ( CA_V410 ( protocolRevision ) ) {
            this->lastBeaconNumber = beaconNumber;
        }

        this->beaconAnomalyNotify ();

        /* 
         * this is the 1st beacon seen - the beacon time stamp
         * was not initialized during BHE create because
         * a TCP/IP connection created the beacon.
         * (nothing to do but set the beacon time stamp and return)
         */
        this->timeStamp = currentTime;

        return false;
    }

    // detect beacon duplications due to IP redundant packet routes
    if ( CA_V410 ( protocolRevision ) ) {
        unsigned lastBeaconNumberSave = this->lastBeaconNumber;
        this->lastBeaconNumber = beaconNumber;
        if ( beaconNumber != lastBeaconNumberSave + 1 ) {
            return false;
        }
    }

    /*
     * compute the beacon period (if we have seen at least two beacons)
     */
    bool netChange = false;
    double currentPeriod = currentTime - this->timeStamp;
    if ( this->averagePeriod < 0.0 ) {
        double totalRunningTime;

        this->beaconAnomalyNotify ();

        /*
         * this is the 2nd beacon seen. We cant tell about
         * the change in period at this point so we just
         * initialize the average period and return.
         */
        this->averagePeriod = currentPeriod;

        /*
         * ignore beacons seen for the first time shortly after
         * init, but do not ignore beacons arriving with a short
         * period because the IOC was rebooted soon after the 
         * client starts up.
         */
        totalRunningTime = this->timeStamp - programBeginTime;
        if ( currentPeriod <= totalRunningTime ) {
            netChange = true;
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

            /* 
             * trigger on any missing beacon 
             * if connected to this server
             */    
            this->beaconAnomalyNotify ();

            if ( currentPeriod >= this->averagePeriod * 3.25 ) {
                /* 
                 * trigger on any 3 contiguous missing beacons 
                 * if not connected to this server
                 */
                netChange = true;
            }
        }

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
        else if ( currentPeriod <= this->averagePeriod * 0.80 ) {
            this->beaconAnomalyNotify ();
            netChange = true;
        }
        else {
            /*
             * update state of health for active virtual circuits 
             * if the beacon looks ok
             */
            tsDLIter < tcpiiu > iter = this->iiuList.firstIter ();
            while ( iter.valid() ) {
                iter->beaconArrivalNotify ();
                iter++;
            }
        }
    
        /*
         * update a running average period
         */
        this->averagePeriod = currentPeriod * 0.125 + this->averagePeriod * 0.875;
    }

    //{
    //    char name[64];
    //    this->name ( name, sizeof ( name ) );
    //
    //    printf ( "new beacon period %f for %s\n", 
    //        this->averagePeriod, name );
    //}

    this->timeStamp = currentTime;

    return netChange;
}

void bhe::show ( unsigned /* level */ ) const
{
    ::printf ( "CA beacon hash entry at %p with average period %f\n", 
        static_cast <const void *> ( this ), this->averagePeriod );
}

void bhe::destroy ()
{
    delete this;
}

double bhe::period () const
{
    return this->averagePeriod;
}

void bhe::registerIIU ( tcpiiu & iiu )
{
    this->iiuList.add ( iiu );
}

void bhe::unregisterIIU ( tcpiiu & iiu )
{
    this->iiuList.remove ( iiu );
    this->timeStamp = epicsTime();
    this->averagePeriod = - DBL_MAX;
}

