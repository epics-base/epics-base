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

#ifndef casChannelIh
#define casChannelIh

#include "casPVI.h"
#include "casEvent.h"
#include "chanIntfForPV.h"
#include "casCoreClient.h"

class casMonitor;
class casAsyncIOI;

class casChannelI : public tsDLNode < casChannelI >, 
    public chronIntIdRes < casChannelI >, public casEvent, 
    private casChannelDestroyFromPV {
public:
    casChannelI ( casCoreClient & clientIn, casChannel & chanIn, 
        casPVI & pvIn, ca_uint32_t cidIn );
    ~casChannelI ();
    void casChannelDestroyFromInterfaceNotify ();
    const caResId getCID ();
    const caResId getSID ();
    void uninstallFromPV ( casEventSys & eventSys );
    void installIntoPV ();
    void installIO ( casAsyncIOI & );
    void uninstallIO ( casAsyncIOI & );
    void installMonitor ( casMonitor & mon );
    casMonitor * removeMonitor ( ca_uint32_t clientIdIn );
    casPVI & getPVI () const;
    void clearOutstandingReads ();
    void postAccessRightsEvent ();
    const gddEnumStringTable & enumStringTable () const;
    ca_uint32_t getMaxElem () const;
    void setOwner ( const char * const pUserName, 
        const char * const pHostName );
    bool readAccess () const;
    bool writeAccess () const;
    bool confirmationRequested () const;
    caStatus read ( const casCtx & ctx, gdd & prototype );
    caStatus write ( const casCtx & ctx, const gdd & value );
    caStatus writeNotify ( const casCtx & ctx, const gdd & value );
    void show ( unsigned level ) const;
private:
    chanIntfForPV privateForPV;
    tsDLList < casAsyncIOI > ioList;
    casPVI & pv;
    ca_uint32_t maxElem;
    casChannel & chan;
    caResId cid; // client id 
    bool serverDeletePending;
    bool accessRightsEvPending;
    //epicsShareFunc virtual void destroy ();
    caStatus cbFunc ( 
        casCoreClient &, 
        epicsGuard < casClientMutex > &,
        epicsGuard < evSysMutex > & ); 
    void postDestroyEvent ();
    casChannelI ( const casChannelI & );
    casChannelI & operator = ( const casChannelI & );
};

inline casPVI & casChannelI::getPVI () const 
{
    return this->pv;
}

inline  ca_uint32_t casChannelI::getMaxElem () const 
{
    return this->maxElem;
}

inline const caResId casChannelI::getCID () 
{
    return this->cid;
}

inline const caResId casChannelI::getSID ()
{
    return this->chronIntIdRes < casChannelI >::getId ();
}

inline void casChannelI::postAccessRightsEvent ()
{
    this->privateForPV.client().addToEventQueue ( *this, this->accessRightsEvPending );
}

inline const gddEnumStringTable & casChannelI::enumStringTable () const
{
    return this->pv.enumStringTable ();
}

inline void casChannelI::installIntoPV ()
{
    this->pv.installChannel ( this->privateForPV );
}

inline void casChannelI::clearOutstandingReads ()
{
    this->pv.clearOutstandingReads ( this->ioList );
}

inline void casChannelI::setOwner ( const char * const pUserName, 
    const char * const pHostName )
{
    this->chan.setOwner ( pUserName, pHostName );
}

inline bool casChannelI::readAccess () const
{
    return this->chan.readAccess ();
}

inline bool casChannelI::writeAccess () const
{
    return this->chan.writeAccess ();
}

inline bool casChannelI::confirmationRequested () const
{
    return this->chan.confirmationRequested ();
}

inline void casChannelI::installIO ( casAsyncIOI & io )
{
    this->pv.installIO ( this->ioList, io );
}

inline void casChannelI::uninstallIO ( casAsyncIOI & io )
{
    this->pv.uninstallIO ( this->ioList, io );
}

inline void casChannelI::casChannelDestroyFromInterfaceNotify ()
{
    if ( ! this->serverDeletePending ) {
        this->privateForPV.client().casChannelDestroyFromInterfaceNotify ( 
            *this, true );
    }
}

inline void casChannelI::installMonitor ( casMonitor & mon )
{
    this->privateForPV.installMonitor ( this->pv, mon );
}

inline casMonitor * casChannelI::removeMonitor (
    ca_uint32_t clientIdIn )
{
    return this->privateForPV.removeMonitor ( this->pv, clientIdIn );
}

#endif // casChannelIh
