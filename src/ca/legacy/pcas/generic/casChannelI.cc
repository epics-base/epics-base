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

#define epicsExportSharedSymbols
#include "casChannelI.h"
#include "casAsyncIOI.h"

casChannelI::casChannelI ( casCoreClient & clientIn,
    casChannel & chanIn, casPVI & pvIn, ca_uint32_t cidIn ) :
        privateForPV ( clientIn, *this ),
        pv ( pvIn ), 
        maxElem( pvIn.nativeCount() ),
        chan ( chanIn ), 
        cid ( cidIn ), 
        serverDeletePending ( false ), 
        accessRightsEvPending ( false )
{
}

casChannelI::~casChannelI ()
{
    this->privateForPV.client().removeFromEventQueue ( 
        *this, this->accessRightsEvPending );

    this->pv.destroyAllIO ( this->ioList );

    this->serverDeletePending = true;
    this->chan.destroyRequest ();
    
    // force PV delete if this is the last channel attached
    this->pv.deleteSignal ();
}

void casChannelI::uninstallFromPV ( casEventSys & eventSys )
{
    tsDLList < casMonitor > dest;
    this->privateForPV.removeSelfFromPV ( this->pv, dest );
    while ( casMonitor * pMon = dest.get () ) {
        eventSys.prepareMonitorForDestroy ( *pMon );
    }
}

void casChannelI::show ( unsigned level ) const
{
    printf ( "casChannelI: client id %u PV %s\n", 
        this->cid, this->pv.getName() );
    if ( level > 0 ) {
        this->privateForPV.show ( level - 1 );
        this->chan.show ( level - 1 );
    }
}

caStatus casChannelI::cbFunc ( 
    casCoreClient &, 
    epicsGuard < casClientMutex > & clientGuard,
    epicsGuard < evSysMutex > & )
{
    caStatus stat = S_cas_success;
    {
        stat = this->privateForPV.client().accessRightsResponse ( 
                    clientGuard, this );
    }
    if ( stat == S_cas_success ) {
        this->accessRightsEvPending = false;
    }
    return stat;
}

caStatus casChannelI::read ( const casCtx & ctx, gdd & prototype )
{
    caStatus status = this->chan.beginTransaction ();
    if ( status != S_casApp_success ) {
        return status;
    }
    status = this->chan.read ( ctx, prototype );
    this->chan.endTransaction ();
    return status;
}

caStatus casChannelI::write ( const casCtx & ctx, const gdd & value )
{
    caStatus status = this->chan.beginTransaction ();
    if ( status != S_casApp_success ) {
        return status;
    }
    status = this->chan.write ( ctx, value );
    this->chan.endTransaction ();
    return status;
}

caStatus casChannelI::writeNotify ( const casCtx & ctx, const gdd & value )
{
    caStatus status = this->chan.beginTransaction ();
    if ( status != S_casApp_success ) {
        return status;
    }
    status = this->chan.writeNotify ( ctx, value );
    this->chan.endTransaction ();
    return status;
}

void casChannelI::postDestroyEvent ()
{
    if ( ! this->serverDeletePending ) {
        this->privateForPV.client().casChannelDestroyFromInterfaceNotify ( 
            *this, false );
    }
}
