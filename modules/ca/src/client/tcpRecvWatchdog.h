/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 *  505 665 1831
 */

#ifndef INC_tcpRecvWatchdog_H
#define INC_tcpRecvWatchdog_H

#include "epicsTimer.h"

#include "libCaAPI.h"

class tcpiiu;

class tcpRecvWatchdog : private epicsTimerNotify {
public:
    tcpRecvWatchdog ( epicsMutex & cbMutex,
        cacContextNotify & ctxNotify,
        epicsMutex & mutex, tcpiiu &,
        double periodIn, epicsTimerQueue & );
    virtual ~tcpRecvWatchdog ();
    void sendBacklogProgressNotify (
        epicsGuard < epicsMutex > & );
    void messageArrivalNotify (
        epicsGuard < epicsMutex > & guard );
    void probeResponseNotify (
        epicsGuard < epicsMutex > & );
    void beaconArrivalNotify (
        epicsGuard < epicsMutex > & );
    void beaconAnomalyNotify ( epicsGuard < epicsMutex > & );
    void connectNotify (
        epicsGuard < epicsMutex > & );
    void sendTimeoutNotify (
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void cancel ();
    void shutdown ();
    void show ( unsigned level ) const;
    double delay () const;
private:
    const double period;
    epicsTimer & timer;
    epicsMutex & cbMutex;
    cacContextNotify & ctxNotify;
    epicsMutex & mutex;
    tcpiiu & iiu;
    bool probeResponsePending;
    bool beaconAnomaly;
    bool probeTimeoutDetected;
    bool shuttingDown;
    expireStatus expire ( const epicsTime & currentTime );
    tcpRecvWatchdog ( const tcpRecvWatchdog & );
    tcpRecvWatchdog & operator = ( const tcpRecvWatchdog & );
};

#endif // #ifndef INC_tcpRecvWatchdog_H

