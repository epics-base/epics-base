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

#ifndef bheh
#define bheh

#ifdef epicsExportSharedSymbols
#   define bhehEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsTime.h"
#include "compilerDependencies.h"

#ifdef bhehEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "inetAddrID.h"
#include "caProto.h"

class tcpiiu;
class bheMemoryManager;

// using a pure abstract wrapper class around the free list avoids
// Tornado 2.0.1 GNU compiler bugs
class epicsShareClass bheMemoryManager {
public:
    virtual ~bheMemoryManager ();
    virtual void * allocate ( size_t ) = 0;
    virtual void release ( void * ) = 0;
};

class bhe : public tsSLNode < bhe >, public inetAddrID {
public:
    epicsShareFunc bhe ( 
        epicsMutex &, const epicsTime & initialTimeStamp, 
        unsigned initialBeaconNumber, const inetAddrID & addr );
    epicsShareFunc ~bhe (); 
    epicsShareFunc bool updatePeriod ( 
        epicsGuard < epicsMutex > &,
        const epicsTime & programBeginTime, 
        const epicsTime & currentTime, ca_uint32_t beaconNumber, 
        unsigned protocolRevision );
    epicsShareFunc double period ( epicsGuard < epicsMutex > & ) const;
    epicsShareFunc epicsTime updateTime ( epicsGuard < epicsMutex > & ) const;
    epicsShareFunc void show ( unsigned level ) const;
    epicsShareFunc void show ( epicsGuard < epicsMutex > &, unsigned /* level */ ) const;
    epicsShareFunc void registerIIU ( epicsGuard < epicsMutex > &, tcpiiu & );
    epicsShareFunc void unregisterIIU ( epicsGuard < epicsMutex > &, tcpiiu & );
    epicsShareFunc void * operator new ( size_t size, bheMemoryManager & );
#ifdef CXX_PLACEMENT_DELETE
    epicsShareFunc void operator delete ( void *, bheMemoryManager & );
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
    epicsShareFunc void operator delete ( void * );
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

#endif // ifdef bheh


