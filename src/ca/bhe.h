
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

#ifdef epicsExportSharedSymbols
#   define bhehEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsSingleton.h"
#include "epicsTime.h"

#ifdef bhehEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

#include "inetAddrID.h"

#include "shareLib.h"

class tcpiiu;

class bhe : public tsSLNode < bhe >, public inetAddrID {
public:
    epicsShareFunc bhe ( const epicsTime &initialTimeStamp, const inetAddrID &addr );
    epicsShareFunc void destroy ();
    epicsShareFunc bool updatePeriod ( const epicsTime & programBeginTime, 
                        const epicsTime & currentTime, unsigned beaconNumber, 
                        unsigned protocolRevision );
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
    unsigned lastBeaconNumber;
    void beaconAnomalyNotify ();
    static epicsSingleton < tsFreeList < class bhe, 1024 > > pFreeList;
	bhe ( const bhe & );
	bhe & operator = ( const bhe & );
};

#endif // ifdef bheh


