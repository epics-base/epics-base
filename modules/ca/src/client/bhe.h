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
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#ifndef INC_bhe_H
#define INC_bhe_H

#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsTime.h"
#include "compilerDependencies.h"

#include "libCaAPI.h"
#include "inetAddrID.h"
#include "caProto.h"

class tcpiiu;
class bheMemoryManager;

// using a pure abstract wrapper class around the free list avoids
// Tornado 2.0.1 GNU compiler bugs
class LIBCA_API bheMemoryManager {
public:
    virtual ~bheMemoryManager ();
    virtual void * allocate ( size_t ) = 0;
    virtual void release ( void * ) = 0;
};

class bhe : public tsSLNode < bhe >, public inetAddrID {
public:
    LIBCA_API bhe (
        epicsMutex &, const epicsTime & initialTimeStamp,
        unsigned initialBeaconNumber, const inetAddrID & addr );
    LIBCA_API ~bhe ();
    LIBCA_API bool updatePeriod (
        epicsGuard < epicsMutex > &,
        const epicsTime & programBeginTime,
        const epicsTime & currentTime, ca_uint32_t beaconNumber,
        unsigned protocolRevision );
    LIBCA_API double period ( epicsGuard < epicsMutex > & ) const;
    LIBCA_API epicsTime updateTime ( epicsGuard < epicsMutex > & ) const;
    LIBCA_API void show ( unsigned level ) const;
    LIBCA_API void show ( epicsGuard < epicsMutex > &, unsigned /* level */ ) const;
    LIBCA_API void registerIIU ( epicsGuard < epicsMutex > &, tcpiiu & );
    LIBCA_API void unregisterIIU ( epicsGuard < epicsMutex > &, tcpiiu & );
    LIBCA_API void * operator new ( size_t size, bheMemoryManager & );
#ifdef CXX_PLACEMENT_DELETE
    LIBCA_API void operator delete ( void *, bheMemoryManager & );
#endif
private:
    epicsTime timeStamp;
    double averagePeriod;
    epicsMutex & mutex;
    tcpiiu * pIIU;
    ca_uint32_t lastBeaconNumber;
    void beaconAnomalyNotify ( epicsGuard < epicsMutex > & );
    void logBeacon ( const char * pDiagnostic,
                     const double & currentPeriod,
                     const epicsTime & currentTime );
    void logBeaconDiscard ( unsigned beaconAdvance,
                     const epicsTime & currentTime );
    bhe ( const bhe & );
    bhe & operator = ( const bhe & );
    LIBCA_API void operator delete ( void * );
};

// using a wrapper class around the free list avoids
// Tornado 2.0.1 GNU compiler bugs
class bheFreeStore : public bheMemoryManager {
public:
    bheFreeStore () {}
    void * allocate ( size_t );
    void release ( void * );
private:
    tsFreeList < bhe, 0x100 > freeList;
    bheFreeStore ( const bheFreeStore & );
    bheFreeStore & operator = ( const bheFreeStore & );
};

inline void * bhe::operator new ( size_t size,
        bheMemoryManager & mgr )
{
    return mgr.allocate ( size );
}

#ifdef CXX_PLACEMENT_DELETE
inline void bhe::operator delete ( void * pCadaver,
        bheMemoryManager & mgr )
{
    mgr.release ( pCadaver );
}
#endif

#endif // ifndef INC_bhe_H


