
/*  
 * $Id$
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

#include "shareLib.h"

#include <float.h>

#ifdef epicsExportSharedSymbols
#   define bhehEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "tsSLList.h"
#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsTime.h"

#ifdef bhehEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

#include "inetAddrID.h"

class tcpiiu;

class bhe : public tsSLNode < bhe >, public inetAddrID {
public:
    epicsShareFunc bhe ( const epicsTime &initialTimeStamp, const inetAddrID &addr );
    epicsShareFunc void destroy ();
   epicsShareFunc bool updatePeriod ( const epicsTime & programBeginTime, 
                        const epicsTime & currentTime );
    epicsShareFunc double period () const;
    epicsShareFunc void show ( unsigned level) const;
    epicsShareFunc void registerIIU ( tcpiiu & );
    epicsShareFunc void unregisterIIU ( tcpiiu & );
    epicsShareFunc void * operator new ( size_t size );
    epicsShareFunc void operator delete ( void *pCadaver, size_t size );
protected:
    epicsShareFunc ~bhe (); // force allocation from freeList
private:
    tsDLList < tcpiiu > iiuList;
    epicsTime timeStamp;
    double averagePeriod;
    void beaconAnomalyNotify ();
    static tsFreeList < class bhe, 1024 > freeList;
    static epicsMutex freeListMutex;
};

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
inline bhe::bhe ( const epicsTime & initialTimeStamp, const inetAddrID & addr ) :
    inetAddrID ( addr ), timeStamp ( initialTimeStamp ), averagePeriod ( - DBL_MAX )
{
#   ifdef DEBUG
    {
        char name[64];
        addr.name ( name, sizeof ( name ) );
        ::printf ( "created beacon entry for %s\n", name );
    }
#   endif
}

#endif // ifdef bheh


