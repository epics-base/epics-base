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

#ifndef INC_tcpSendWatchdog_H
#define INC_tcpSendWatchdog_H

#include "epicsTimer.h"

#include "libCaAPI.h"

class tcpSendWatchdog : private epicsTimerNotify {
public:
    tcpSendWatchdog (
        epicsMutex & cbMutex, cacContextNotify & ctxNotify,
        epicsMutex & mutex, tcpiiu &,
        double periodIn, epicsTimerQueue & queueIn );
    virtual ~tcpSendWatchdog ();
    void start ( const epicsTime & );
    void cancel ();
private:
    const double period;
    epicsTimer & timer;
    epicsMutex & cbMutex;
    cacContextNotify & ctxNotify;
    epicsMutex & mutex;
    tcpiiu & iiu;
    expireStatus expire ( const epicsTime & currentTime );
    tcpSendWatchdog ( const tcpSendWatchdog & );
    tcpSendWatchdog & operator = ( const tcpSendWatchdog & );
};

#endif // #ifndef INC_tcpSendWatchdog_H
