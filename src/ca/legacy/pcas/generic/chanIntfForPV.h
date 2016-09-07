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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef chanIntfForPVh
#define chanIntfForPVh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_chanIntfForPVh
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "tsDLList.h"
#include "caProto.h"

#ifdef epicsExportSharedSymbols_chanIntfForPVh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casStrmClient.h"
#include "casPVI.h"

class casMonitor;
class casPVI;
class casEventMask;
class gdd;

class casChannelDestroyFromPV {
public:
    virtual void postDestroyEvent () = 0;
protected:
    virtual ~casChannelDestroyFromPV() {}
};

class chanIntfForPV : public tsDLNode < chanIntfForPV > {
public:
    chanIntfForPV ( class casCoreClient &, casChannelDestroyFromPV & );
    ~chanIntfForPV ();
    class casCoreClient & client () const;
    void installMonitor ( casPVI & pv, casMonitor & mon );
    casMonitor * removeMonitor ( casPVI &, ca_uint32_t monId );
    void removeSelfFromPV ( casPVI &, 
        tsDLList < casMonitor > & dest );
    void postEvent ( const casEventMask &, const gdd & );
    void show ( unsigned level ) const;
    void postDestroyEvent ();
private:
	tsDLList < casMonitor > monitorList;
	class casCoreClient & clientRef;
    casChannelDestroyFromPV & destroyRef;
	chanIntfForPV ( const chanIntfForPV & );
	chanIntfForPV & operator = ( const chanIntfForPV & );
};

inline void chanIntfForPV::postEvent (
    const casEventMask & select, const gdd & event )
{
    this->clientRef.postEvent ( this->monitorList, select, event );
}

inline casMonitor * chanIntfForPV::removeMonitor ( 
    casPVI & pv, ca_uint32_t clientIdIn )
{
    return pv.removeMonitor ( this->monitorList, clientIdIn );
}

inline void chanIntfForPV::removeSelfFromPV ( 
    casPVI & pv, tsDLList < casMonitor > & dest )
{
    pv.removeChannel ( *this, this->monitorList, dest );
}

inline void chanIntfForPV::postDestroyEvent ()
{
    this->destroyRef.postDestroyEvent ();
}

#endif // chanIntfForPVh
