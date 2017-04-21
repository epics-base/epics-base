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

#include <stdarg.h>
#include <stdexcept>

#include "epicsGuard.h"
#include "epicsVersion.h"
#include "errlog.h"

#include "addrList.h"

#define epicsExportSharedSymbols
#define caServerGlobal
#include "caServerI.h"
#include "beaconTimer.h"
#include "beaconAnomalyGovernor.h"
#include "casStreamOS.h"
#include "casIntfOS.h"

// include a version string for POSIX systems
static const char pVersionCAS[] = 
    "@(#) " EPICS_VERSION_STRING 
    ", CA Portable Server Library ";

caServerI::caServerI ( caServer & tool ) :
    adapter (tool),
    beaconTmr ( * new beaconTimer ( *this ) ),
    beaconAnomalyGov ( * new beaconAnomalyGovernor ( *this ) ),
    debugLevel ( 0u ),
    nEventsProcessed ( 0u ),
    nEventsPosted ( 0u ),
    ioInProgressCount ( 0u )
{
    assert ( & adapter != NULL );

    // create predefined event types
    this->valueEvent = registerEvent ( "value" );
    this->logEvent = registerEvent ( "log" );
    this->alarmEvent = registerEvent ( "alarm" );
    this->propertyEvent = registerEvent ( "property" );

    this->locateInterfaces ();

    if (this->intfList.count()==0u) {
        errMessage (S_cas_noInterface, 
            "- CA server internals init unable to continue");
        throw S_cas_noInterface;
    }

    return;
}

caServerI::~caServerI()
{
    delete & this->beaconAnomalyGov;
    delete & this->beaconTmr;

    // delete all clients
    while ( casStrmClient * pClient = this->clientList.get() ) {
        delete pClient;
    }

    casIntfOS *pIF;
    while ( ( pIF = this->intfList.get() ) ) {
        delete pIF;
    }
}

void caServerI::destroyClient ( casStrmClient & client )
{
    {
        epicsGuard < epicsMutex > locker ( this->mutex );
        this->clientList.remove ( client );
    }
    delete & client;
}

void caServerI::connectCB ( casIntfOS & intf )
{
    casStreamOS * pClient = intf.newStreamClient ( *this, this->clientBufMemMgr );
    if ( pClient ) {
        {
            epicsGuard < epicsMutex > locker ( this->mutex );
            this->clientList.add ( *pClient );
        }
        pClient->sendVersion ();
        pClient->flush ();
    }
}

void casVerifyFunc ( const char * pFile, unsigned line, const char * pExp )
{
    fprintf(stderr, "the expression \"%s\" didnt evaluate to boolean true \n",
        pExp);
    fprintf(stderr,
        "and therefore internal problems are suspected at line %u in \"%s\"\n",
        line, pFile);
    fprintf(stderr,
        "Please forward above text to johill@lanl.gov - thanks\n");
}

void serverToolDebugFunc (const char *pFile, unsigned line, const char *pComment)
{
    fprintf (stderr,
"Bad server tool response detected at line %u in \"%s\" because \"%s\"\n",
                line, pFile, pComment);
}

caStatus caServerI::attachInterface ( const caNetAddr & addrIn, 
        bool autoBeaconAddr, bool addConfigBeaconAddr)
{    
    try {
        casIntfOS * pIntf = new casIntfOS ( *this, this->clientBufMemMgr, 
            addrIn, autoBeaconAddr, addConfigBeaconAddr );
        
        {
            epicsGuard < epicsMutex > locker ( this->mutex );
            this->intfList.add ( *pIntf );
        }
    }
    catch ( ... ) {
        return S_cas_bindFail;
    }

    return S_cas_success;
}

void caServerI::addMCast(const osiSockAddr& addr)
{
#ifdef IP_ADD_MEMBERSHIP
    epicsGuard < epicsMutex > locker ( this->mutex );
    tsDLIter < casIntfOS > iter = this->intfList.firstIter ();
    while ( iter.valid () ) {
        struct ip_mreq mreq;

        memset(&mreq, 0, sizeof(mreq));
        mreq.imr_interface = iter->serverAddress().getSockIP().sin_addr;
        mreq.imr_multiaddr = addr.ia.sin_addr;

        if ( setsockopt ( iter->casDGIntfIO::getFD (), IPPROTO_IP,
                          IP_ADD_MEMBERSHIP, (char *) &mreq,
                          sizeof ( mreq ) ) < 0) {
            struct sockaddr_in temp;
            char name[40];
            char sockErrBuf[64];
            temp.sin_family = AF_INET;
            temp.sin_addr = mreq.imr_multiaddr;
            temp.sin_port = addr.ia.sin_port;
            epicsSocketConvertErrnoToString (
                sockErrBuf, sizeof ( sockErrBuf ) );
            ipAddrToDottedIP (&temp, name, sizeof(name));
            fprintf(stderr, "CAS: Socket mcast join %s failed with \"%s\"\n",
                name, sockErrBuf );
        }

        iter++;
    }
#endif
}

void caServerI::sendBeacon ( ca_uint32_t beaconNo )
{
    epicsGuard < epicsMutex > locker ( this->mutex );
    tsDLIter < casIntfOS > iter = this->intfList.firstIter ();
    while ( iter.valid () ) {
        iter->sendBeacon ( beaconNo );
        iter++;
    }
}

void caServerI::generateBeaconAnomaly ()
{
    this->beaconAnomalyGov.start ();
}

void caServerI::destroyMonitor ( casMonitor &  mon )
{
    mon.~casMonitor ();
    this->casMonitorFreeList.release ( & mon );
}

void caServerI::updateEventsPostedCounter ( unsigned nNewPosts )
{
    epicsGuard < epicsMutex > guard ( this->diagnosticCountersMutex );
    this->nEventsPosted += nNewPosts;
}

unsigned caServerI::subscriptionEventsPosted () const
{
    epicsGuard < epicsMutex > guard ( this->diagnosticCountersMutex );
    return this->nEventsPosted;
}

void caServerI::incrEventsProcessedCounter ()
{
    epicsGuard < epicsMutex > guard ( this->diagnosticCountersMutex );
    this->nEventsProcessed ++;
}

unsigned caServerI::subscriptionEventsProcessed () const
{
    epicsGuard < epicsMutex > guard ( this->diagnosticCountersMutex );
    return this->nEventsProcessed;
}

void caServerI::show (unsigned level) const
{
    int bytes_reserved;
    
    printf ( "Channel Access Server V%s\n",
        CA_VERSION_STRING ( CA_MINOR_PROTOCOL_REVISION ) );
    printf ( "\trevision %s\n", pVersionCAS );

    this->mutex.show(level);
    
    {
        epicsGuard < epicsMutex > locker ( this->mutex );
        tsDLIterConst < casStrmClient > iterCl = this->clientList.firstIter ();
        while ( iterCl.valid () ) {
            iterCl->show ( level );
            ++iterCl;
        }
    
        tsDLIterConst < casIntfOS > iterIF = this->intfList.firstIter ();
        while ( iterIF.valid () ) {
            iterIF->casIntfOS::show ( level );
            ++iterIF;
        }
    }
    
    bytes_reserved = 0u;
#if 0
    bytes_reserved += sizeof(casClient) *
        ellCount(&this->freeClientQ);
    bytes_reserved += sizeof(casChannel) *
        ellCount(&this->freeChanQ);
    bytes_reserved += sizeof(casEventBlock) *
        ellCount(&this->freeEventQ);
    bytes_reserved += sizeof(casAsyncIIO) *
        ellCount(&this->freePendingIO);
#endif
    if (level>=1) {
        printf(
            "There are currently %d bytes on the server's free list\n",
            bytes_reserved);
#if 0
        printf(
            "%d client(s), %d channel(s), %d event(s) (monitors), and %d IO blocks\n",
            ellCount(&this->freeClientQ),
            ellCount(&this->freeChanQ),
            ellCount(&this->freeEventQ),
            ellCount(&this->freePendingIO));
#endif
        printf( 
            "The server's integer resource id conversion table:\n");
    }
        
    return;
}

casMonitor & caServerI::casMonitorFactory ( 
    casChannelI & chan, caResId clientId, 
    const unsigned long count, const unsigned type, 
    const casEventMask & mask, 
    casMonitorCallbackInterface & cb )
{
    casMonitor * pMon = 
        new ( this->casMonitorFreeList ) casMonitor 
            ( clientId, chan, count, type, mask, cb );
    return *pMon;
}

void caServerI::casMonitorDestroy ( casMonitor & cm )
{
    cm.~casMonitor ();
    this->casMonitorFreeList.release ( & cm );
}

//
//  caServerI::dumpMsg()
//
//  Debug aid - print the header part of a message.
//
//  dp arg allowed to be null
//
//
void caServerI::dumpMsg ( const char * pHostName, const char * pUserName,
    const caHdrLargeArray * mp, const void * /* dp */, const char * pFormat, ... )
{
    va_list theArgs;
    if ( pFormat ) {
        va_start ( theArgs, pFormat );
        errlogPrintf ( "CAS: " );
        errlogVprintf ( pFormat, theArgs );
        va_end ( theArgs );
    }

    fprintf ( stderr, 
"CAS Request: %s on %s: cmd=%u cid=%u typ=%u cnt=%u psz=%u avail=%x\n",
        pUserName,
        pHostName,
        mp->m_cmmd,
        mp->m_cid,
        mp->m_dataType,
        mp->m_count,
        mp->m_postsize,
        mp->m_available);

    //if ( mp->m_cmmd == CA_PROTO_WRITE && mp->m_dataType == DBR_STRING && dp ) {
    //    errlogPrintf("CAS: The string written: %s \n", (char *)dp);
    //}
}

casEventRegistry::~casEventRegistry()
{
    this->traverse ( &casEventMaskEntry::destroy );
}

