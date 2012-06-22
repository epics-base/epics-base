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
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include <string>
#include <stdexcept>
#include <limits.h>
#include <float.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "virtualCircuit.h"
#include "bhe.h"

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
bhe::bhe ( epicsMutex & mutexIn, const epicsTime & initialTimeStamp, 
          unsigned initialBeaconNumber, const inetAddrID & addr ) :
    inetAddrID ( addr ), timeStamp ( initialTimeStamp ), averagePeriod ( - DBL_MAX ),
    mutex ( mutexIn ), pIIU ( 0 ), lastBeaconNumber ( initialBeaconNumber )
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

void bhe::beaconAnomalyNotify ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->pIIU ) {
        this->pIIU->beaconAnomalyNotify ( guard );
    }
}

#ifdef DEBUG
void bhe::logBeacon ( const char * pDiagnostic, 
                     const double & currentPeriod,
                     const epicsTime & currentTime )
{
    if ( this->pIIU ) {
        char name[64];
        this->name ( name, sizeof ( name ) );
        char date[64];
        currentTime.strftime ( date, sizeof ( date ), 
            "%a %b %d %Y %H:%M:%S.%f");
        ::printf ( "%s cp=%g ap=%g %s %s\n",
            pDiagnostic, currentPeriod, 
            this->averagePeriod, name, date );
    }
}
#else
inline void bhe::logBeacon ( const char * /* pDiagnostic */,
    const double & /* currentPeriod */,
    const epicsTime & /* currentTime */ )
{
}
#endif

#ifdef DEBUG
void bhe::logBeaconDiscard ( unsigned beaconAdvance,
                     const epicsTime & currentTime )
{
    if ( this->pIIU ) {
        char name[64];
        this->name ( name, sizeof ( name ) );
        char date[64];
        currentTime.strftime ( date, sizeof ( date ), 
            "%a %b %d %Y %H:%M:%S.%f");
        ::printf ( "bb %u %s %s\n",
            beaconAdvance, name, date );
    }
}
#else
void bhe::logBeaconDiscard ( unsigned /* beaconAdvance */,
                     const epicsTime & /* currentTime */ )
{
}
#endif

/*
 * update beacon period
 *
 * updates beacon period, and looks for beacon anomalies
 */
bool bhe::updatePeriod ( 
    epicsGuard < epicsMutex > & guard, const epicsTime & programBeginTime, 
    const epicsTime & currentTime, ca_uint32_t beaconNumber, 
    unsigned protocolRevision )
{
    guard.assertIdenticalMutex ( this->mutex );

    //
    // this block is enetered if the beacon was created as a side effect of
    // creating a connection and so we dont yet know the first beacon time 
    // and  sequence number
    //
    if ( this->timeStamp == epicsTime () ) {
        if ( CA_V410 ( protocolRevision ) ) {
            this->lastBeaconNumber = beaconNumber;
        }

        this->beaconAnomalyNotify ( guard );

        /* 
         * this is the 1st beacon seen - the beacon time stamp
         * was not initialized during BHE create because
         * a TCP/IP connection created the beacon.
         * (nothing to do but set the beacon time stamp and return)
         */
        this->timeStamp = currentTime;

        logBeacon ( "fb", - DBL_MAX, currentTime );

        return false;
    }

    // 1) detect beacon duplications due to redundant routes
    // 2) detect lost beacons due to input queue overrun or damage
    if ( CA_V410 ( protocolRevision ) ) {
        unsigned beaconSeqAdvance;
        if ( beaconNumber >= this->lastBeaconNumber ) {
            beaconSeqAdvance = beaconNumber - this->lastBeaconNumber;
        }
        else {
            beaconSeqAdvance = ( ca_uint32_max - this->lastBeaconNumber ) + beaconNumber;
        }
        this->lastBeaconNumber = beaconNumber;

        // throw out sequence numbers just prior to, or the same as, the last one received 
        // (this situation is probably caused by a temporary duplicate route )
        if ( beaconSeqAdvance == 0 ||  beaconSeqAdvance > ca_uint32_max - 256 ) {
            logBeaconDiscard ( beaconSeqAdvance, currentTime );
            return false;
        }

        // throw out sequence numbers that jump forward by only a few numbers 
        // (this situation is probably caused by a duplicate route 
        // or a beacon due to input queue overun)
        if ( beaconSeqAdvance > 1 &&  beaconSeqAdvance < 4 ) {
            logBeaconDiscard ( beaconSeqAdvance, currentTime );
            return false;
        }
    }

    // compute the beacon period (if we have seen at least two beacons)
    bool netChange = false;
    double currentPeriod = currentTime - this->timeStamp;

    if ( this->averagePeriod < 0.0 ) {
        double totalRunningTime;

        this->beaconAnomalyNotify ( guard );

        /*
         * this is the 2nd beacon seen. We cant tell about
         * the change in period at this point so we just
         * initialize the average period and return.
         */
        this->averagePeriod = currentPeriod;

        logBeacon ( "fp", currentPeriod, currentTime );


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
            this->beaconAnomalyNotify ( guard );

            if ( currentPeriod >= this->averagePeriod * 3.25 ) {
                /* 
                 * trigger on any 3 contiguous missing beacons 
                 * if not connected to this server
                 */
                netChange = true;
            }
            logBeacon ( "bah", currentPeriod, currentTime );
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
            this->beaconAnomalyNotify ( guard );
            netChange = true;
            logBeacon ( "bal", currentPeriod, currentTime );
        }
        else if ( this->pIIU ) {
            // update state of health for active virtual circuits 
            // if the beacon looks ok
            this->pIIU->beaconArrivalNotify ( guard );
            logBeacon ( "vb", currentPeriod, currentTime );
        }

        // update a running average period
        this->averagePeriod = currentPeriod * 0.125 + 
            this->averagePeriod * 0.875;
    }

    this->timeStamp = currentTime;

    return netChange;
}

void bhe::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->show ( guard, level );
}

void bhe::show ( epicsGuard < epicsMutex > &, unsigned level ) const
{
    char host [64];
    this->name ( host, sizeof ( host ) );
    if ( this->averagePeriod == -DBL_MAX ) {
        ::printf ( "CA beacon hash entry for %s <no period estimate>\n", 
            host );
    }
    else {
        ::printf ( "CA beacon hash entry for %s with period estimate %f\n", 
            host, this->averagePeriod );
    }
    if ( level > 0u ) {
        char date[64];
        this->timeStamp.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S");
        ::printf ( "\tbeacon number %u, on %s\n", 
            this->lastBeaconNumber, date );
    }
}

double bhe::period ( epicsGuard < epicsMutex > & guard ) const 
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->averagePeriod;
}

epicsTime bhe::updateTime ( epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->timeStamp;
}

void bhe::registerIIU ( 
    epicsGuard < epicsMutex > & guard, tcpiiu & iiu )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->pIIU = & iiu;
}

void bhe::unregisterIIU ( 
    epicsGuard < epicsMutex > & guard, tcpiiu & iiu )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->pIIU == & iiu ) {
        this->pIIU = 0;
        this->timeStamp = epicsTime();
        this->averagePeriod = - DBL_MAX;
        logBeacon ( "ui", this->averagePeriod, epicsTime::getCurrent () );
    }
}

void bhe::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void * bheFreeStore::allocate ( size_t size )
{
    return freeList.allocate ( size );
}

void bheFreeStore::release ( void * pCadaver )
{
    freeList.release ( pCadaver );
}

bheMemoryManager::~bheMemoryManager () {}


