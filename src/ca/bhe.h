
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
    tcpiiu *getIIU () const;
    void bindToIIU ( tcpiiu & );
    void unbindFromIIU ();
    epicsShareFunc void destroy ();
    epicsShareFunc bool updatePeriod ( const epicsTime &programBeginTime, 
                        const epicsTime & currentTime );
    epicsShareFunc double period () const;
    epicsShareFunc void show ( unsigned level) const;
    epicsShareFunc void * operator new ( size_t size );
    epicsShareFunc void operator delete ( void *pCadaver, size_t size );
protected:
    epicsShareFunc ~bhe (); // force allocation from freeList
private:
    tcpiiu *piiu;
    epicsTime timeStamp;
    double averagePeriod;
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
inline bhe::bhe ( const epicsTime &initialTimeStamp, const inetAddrID &addr ) :
    inetAddrID ( addr ), piiu ( 0 ), 
    timeStamp ( initialTimeStamp ), averagePeriod ( - DBL_MAX )
{
#   ifdef DEBUG
    {
        char name[64];
        addr.name ( name, sizeof ( name ) );
        ::printf ( "created beacon entry for %s\n", name );
    }
#   endif
}

inline tcpiiu *bhe::getIIU ()const
{
    return this->piiu;
}

inline void bhe::bindToIIU ( tcpiiu &iiuIn )
{
    if ( this->piiu != &iiuIn ) {
        assert ( this->piiu == 0 );
        this->piiu = &iiuIn;
    }
}

inline void bhe::unbindFromIIU ()
{
    this->piiu = 0;
    this->timeStamp = epicsTime();
    this->averagePeriod = - DBL_MAX;
}

#endif // ifdef bheh


