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
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include <new>

#include "epicsGuard.h"
#include "epicsVersion.h"
#include "osiProcess.h"
#include "epicsSignal.h"
#include "envDefs.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "cac.h"
#include "inetAddrID.h"
#include "caServerID.h"
#include "virtualCircuit.h"
#include "syncGroup.h"
#include "nciu.h"
#include "autoPtrRecycle.h"
#include "msgForMultiplyDefinedPV.h"
#include "udpiiu.h"
#include "bhe.h"
#include "net_convert.h"
#include "autoPtrFreeList.h"

static const char *pVersionCAC = 
    "@(#) " EPICS_VERSION_STRING 
    ", CA Portable Server Library " __DATE__;

// TCP response dispatch table
const cac::pProtoStubTCP cac::tcpJumpTableCAC [] = 
{
    &cac::versionAction,
    &cac::eventRespAction,
    &cac::badTCPRespAction,
    &cac::readRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::exceptionRespAction,
    &cac::clearChannelRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::readNotifyRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::createChannelRespAction,
    &cac::writeNotifyRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::accessRightsRespAction,
    &cac::echoRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::verifyAndDisconnectChan,
    &cac::verifyAndDisconnectChan
};

// TCP exception dispatch table
const cac::pExcepProtoStubTCP cac::tcpExcepJumpTableCAC [] = 
{
    &cac::defaultExcep,     // CA_PROTO_VERSION
    &cac::eventAddExcep,    // CA_PROTO_EVENT_ADD
    &cac::defaultExcep,     // CA_PROTO_EVENT_CANCEL
    &cac::readExcep,        // CA_PROTO_READ
    &cac::writeExcep,       // CA_PROTO_WRITE
    &cac::defaultExcep,     // CA_PROTO_SNAPSHOT
    &cac::defaultExcep,     // CA_PROTO_SEARCH
    &cac::defaultExcep,     // CA_PROTO_BUILD
    &cac::defaultExcep,     // CA_PROTO_EVENTS_OFF
    &cac::defaultExcep,     // CA_PROTO_EVENTS_ON
    &cac::defaultExcep,     // CA_PROTO_READ_SYNC
    &cac::defaultExcep,     // CA_PROTO_ERROR
    &cac::defaultExcep,     // CA_PROTO_CLEAR_CHANNEL
    &cac::defaultExcep,     // CA_PROTO_RSRV_IS_UP
    &cac::defaultExcep,     // CA_PROTO_NOT_FOUND
    &cac::readNotifyExcep,  // CA_PROTO_READ_NOTIFY
    &cac::defaultExcep,     // CA_PROTO_READ_BUILD
    &cac::defaultExcep,     // REPEATER_CONFIRM
    &cac::defaultExcep,     // CA_PROTO_CREATE_CHAN 
    &cac::writeNotifyExcep, // CA_PROTO_WRITE_NOTIFY
    &cac::defaultExcep,     // CA_PROTO_CLIENT_NAME
    &cac::defaultExcep,     // CA_PROTO_HOST_NAME
    &cac::defaultExcep,     // CA_PROTO_ACCESS_RIGHTS
    &cac::defaultExcep,     // CA_PROTO_ECHO
    &cac::defaultExcep,     // REPEATER_REGISTER
    &cac::defaultExcep,     // CA_PROTO_SIGNAL
    &cac::defaultExcep,     // CA_PROTO_CREATE_CH_FAIL
    &cac::defaultExcep      // CA_PROTO_SERVER_DISCONN
};

epicsThreadPrivateId caClientCallbackThreadId;

static epicsThreadOnceId cacOnce = EPICS_THREAD_ONCE_INIT;

extern "C" void cacExitHandler ()
{
    epicsThreadPrivateDelete ( caClientCallbackThreadId );
}

// runs once only for each process
extern "C" void cacOnceFunc ( void * )
{
    caClientCallbackThreadId = epicsThreadPrivateCreate ();
    assert ( caClientCallbackThreadId );
    atexit ( cacExitHandler );
}

//
// cac::cac ()
//
cac::cac ( epicsMutex & mutexIn, cacContextNotify & notifyIn ) :
    programBeginTime ( epicsTime::getCurrent() ),
    connTMO ( CA_CONN_VERIFY_PERIOD ),
    cbMutex ( notifyIn ),
    mutex ( mutexIn ),
    ipToAEngine ( ipAddrToAsciiEngine::allocate () ),
    timerQueue ( epicsTimerQueueActive::allocate ( false, 
        lowestPriorityLevelAbove(epicsThreadGetPrioritySelf()) ) ),
    pUserName ( 0 ),
    pudpiiu ( 0 ),
    tcpSmallRecvBufFreeList ( 0 ),
    tcpLargeRecvBufFreeList ( 0 ),
    notify ( notifyIn ),
    initializingThreadsId ( epicsThreadGetIdSelf() ),
    initializingThreadsPriority ( epicsThreadGetPrioritySelf() ),
    maxRecvBytesTCP ( MAX_TCP ),
    beaconAnomalyCount ( 0u )
{
	if ( ! osiSockAttach () ) {
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
	}

    epicsThreadOnce ( &cacOnce, cacOnceFunc, 0 );

    try {
	    long status;

        /*
         * Certain os, such as HPUX, do not unblock a socket system call 
         * when another thread asynchronously calls both shutdown() and 
         * close(). To solve this problem we need to employ OS specific
         * mechanisms.
         */
        epicsSignalInstallSigAlarmIgnore ();
        epicsSignalInstallSigPipeIgnore ();

        {
            char tmp[256];
            size_t len;
            osiGetUserNameReturn gunRet;

            gunRet = osiGetUserName ( tmp, sizeof (tmp) );
            if ( gunRet != osiGetUserNameSuccess ) {
                tmp[0] = '\0';
            }
            len = strlen ( tmp ) + 1;
            this->pUserName = new char [ len ];
            strncpy ( this->pUserName, tmp, len );
        }

        status = envGetDoubleConfigParam ( &EPICS_CA_CONN_TMO, &this->connTMO );
        if ( status ) {
            this->connTMO = CA_CONN_VERIFY_PERIOD;
            this->printf ( "EPICS \"%s\" double fetch failed\n", EPICS_CA_CONN_TMO.name);
            this->printf ( "Defaulting \"%s\" = %f\n", EPICS_CA_CONN_TMO.name, this->connTMO);
        }

        long maxBytesAsALong;
        status =  envGetLongConfigParam ( &EPICS_CA_MAX_ARRAY_BYTES, &maxBytesAsALong );
        if ( status || maxBytesAsALong < 0 ) {
            errlogPrintf ( "cac: EPICS_CA_MAX_ARRAY_BYTES was not a positive integer\n" );
        }
        else {
            /* allow room for the protocol header so that they get the array size they requested */
            static const unsigned headerSize = sizeof ( caHdr ) + 2 * sizeof ( ca_uint32_t );
            ca_uint32_t maxBytes = ( unsigned ) maxBytesAsALong;
            if ( maxBytes < 0xffffffff - headerSize ) {
                maxBytes += headerSize;
            }
            else {
                maxBytes = 0xffffffff;
            }
            if ( maxBytes < MAX_TCP ) {
                errlogPrintf ( "cac: EPICS_CA_MAX_ARRAY_BYTES was rounded up to %u\n", MAX_TCP );
            }
            else {
                this->maxRecvBytesTCP = maxBytes;
            }
        }
        freeListInitPvt ( &this->tcpSmallRecvBufFreeList, MAX_TCP, 1 );
        if ( ! this->tcpSmallRecvBufFreeList ) {
            throw std::bad_alloc ();
        }

        freeListInitPvt ( &this->tcpLargeRecvBufFreeList, this->maxRecvBytesTCP, 1 );
        if ( ! this->tcpLargeRecvBufFreeList ) {
            throw std::bad_alloc ();
        }
    }
    catch ( ... ) {
        osiSockRelease ();
        delete [] this->pUserName;
        if ( this->tcpSmallRecvBufFreeList ) {
            freeListCleanup ( this->tcpSmallRecvBufFreeList );
        }
        if ( this->tcpLargeRecvBufFreeList ) {
            freeListCleanup ( this->tcpLargeRecvBufFreeList );
        }
        this->timerQueue.release ();
        throw;
    }
}

cac::~cac ()
{
    // this blocks until the UDP thread exits so that
    // it will not sneak in any new clients
    //
    // lock intentionally not held here so that we dont deadlock 
    // waiting for the UDP thread to exit while it is waiting to 
    // get the lock.
    if ( this->pudpiiu ) {
        this->pudpiiu->shutdown ();

        //
        // shutdown all tcp circuits
        //
        epicsGuard < callbackMutex > cbGuard ( this->cbMutex );
        epicsGuard < epicsMutex > guard ( this->mutex );
        tsDLIter < tcpiiu > iter = this->serverList.firstIter ();
        while ( iter.valid() ) {
            // this causes a clean shutdown to occur
            iter->removeAllChannels ( cbGuard, guard, *this->pudpiiu );
            iter++;
        }
    }

    //
    // wait for all tcp threads to exit
    //
    // this will block for oustanding sends to go out so dont 
    // hold a lock while waiting
    //
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        while ( this->serverList.count() ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->iiuUninstall.wait ();
        }
    }


    if ( this->pudpiiu ) {
        delete this->pudpiiu;
    }

    freeListCleanup ( this->tcpSmallRecvBufFreeList );
    freeListCleanup ( this->tcpLargeRecvBufFreeList );

    delete [] this->pUserName;

    tsSLList < bhe > tmpBeaconList;
    this->beaconTable.removeAll ( tmpBeaconList ); 
    while ( bhe * pBHE = tmpBeaconList.get() ) {
        pBHE->~bhe ();
        this->bheFreeList.release ( pBHE );
    }

    osiSockRelease ();

    this->timerQueue.release ();

    this->ipToAEngine.release ();

    errlogFlush ();

    // its ok for channels and subscriptions to still
    // exist at this point. The user created them and 
    // its his responsibility to clean them up.
}

unsigned cac::lowestPriorityLevelAbove ( unsigned priority )
{
    unsigned abovePriority;
    epicsThreadBooleanStatus tbs;
    tbs = epicsThreadLowestPriorityLevelAbove ( 
        priority, & abovePriority );
    if ( tbs != epicsThreadBooleanStatusSuccess ) {
        abovePriority = priority;
    }
    return abovePriority;
}

unsigned cac::highestPriorityLevelBelow ( unsigned priority )
{
    unsigned belowPriority;
    epicsThreadBooleanStatus tbs;
    tbs = epicsThreadHighestPriorityLevelBelow ( 
        priority, & belowPriority );
    if ( tbs != epicsThreadBooleanStatusSuccess ) {
        belowPriority = priority;
    }
    return belowPriority;
}

//
// set the push pending flag on all virtual circuits
//
void cac::flush ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    tsDLIter < tcpiiu > iter = this->serverList.firstIter ();
    while ( iter.valid () ) {
        iter->flushRequest ( guard );
        iter++;
    }
}

unsigned cac::circuitCount ( 
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->serverList.count ();
}

void cac::show ( 
    epicsGuard < epicsMutex > & guard, unsigned level ) const
{
    guard.assertIdenticalMutex ( this->mutex );

    ::printf ( "Channel Access Client Context at %p for user %s\n", 
        static_cast <const void *> ( this ), this->pUserName );
    // this also supresses the "defined, but not used" 
    // warning message
    ::printf ( "\trevision \"%s\"\n", pVersionCAC );

    if ( level > 0u ) {
        this->serverTable.show ( level - 1u );
        ::printf ( "\tconnection time out watchdog period %f\n", this->connTMO );
    }

    if ( level > 1u ) {
        if ( this->pudpiiu ) {
            this->pudpiiu->show ( level - 2u );
        }
    }

    if ( level > 2u ) {
        ::printf ( "Program begin time:\n");
        this->programBeginTime.show ( level - 3u );
        ::printf ( "Channel identifier hash table:\n" );
        this->chanTable.show ( level - 3u );
        ::printf ( "IO identifier hash table:\n" );
        this->ioTable.show ( level - 3u );
        ::printf ( "Beacon source identifier hash table:\n" );
        this->beaconTable.show ( level - 3u );
        ::printf ( "Timer queue:\n" );
        this->timerQueue.show ( level - 3u );
        ::printf ( "IP address to name conversion engine:\n" );
        this->ipToAEngine.show ( level - 3u );
    }

    if ( level > 3u ) {
        ::printf ( "Default mutex:\n");
        this->mutex.show ( level - 4u );
        ::printf ( "mutex:\n" );
        this->mutex.show ( level - 4u );
    }
}

/*
 *  cac::beaconNotify
 */
void cac::beaconNotify ( const inetAddrID & addr, const epicsTime & currentTime,
                        ca_uint32_t beaconNumber, unsigned protocolRevision  )
{
    epicsGuard < epicsMutex > guard ( this->mutex );

    if ( ! this->pudpiiu ) {
        return;
    }

    /*
     * look for it in the hash table
     */
    bhe *pBHE = this->beaconTable.lookup ( addr );
    if ( pBHE ) {
        /*
         * return if the beacon period has not changed significantly
         */
        if ( ! pBHE->updatePeriod ( guard, this->programBeginTime, 
                currentTime, beaconNumber, protocolRevision ) ) {
            return;
        }
    }
    else {
        /*
         * This is the first beacon seen from this server.
         * Wait until 2nd beacon is seen before deciding
         * if it is a new server (or just the first
         * time that we have seen a server's beacon
         * shortly after the program started up)
         */
        pBHE = new ( this->bheFreeList )
                bhe ( this->mutex, currentTime, beaconNumber, addr );
        if ( pBHE ) {
            if ( this->beaconTable.add ( *pBHE ) < 0 ) {
                pBHE->~bhe ();
                this->bheFreeList.release ( pBHE );
            }
        }
        return;
    }

    this->beaconAnomalyCount++;

    this->pudpiiu->beaconAnomalyNotify ( currentTime );

#   if DEBUG
    {
        char buf[64];
        ipAddrToDottedIP (pnet_addr, buf, sizeof ( buf ) );
        printf ("new server available: %s\n", buf);
    }
#   endif
}

cacChannel & cac::createChannel ( 
    epicsGuard < epicsMutex > & guard, const char * pName, 
    cacChannelNotify & chan, cacChannel::priLev pri )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( pri > cacChannel::priorityMax ) {
        throw cacChannel::badPriority ();
    }

    if ( pName == 0 || pName[0] == '\0' ) {
        throw cacChannel::badString ();
    }

    if ( ! this->pudpiiu ) {
        this->pudpiiu = new udpiiu ( 
            this->timerQueue, this->cbMutex, *this );
    }

    nciu * pNetChan = new ( this->channelFreeList ) 
            nciu ( *this, *this->pudpiiu, chan, pName, pri );
    this->chanTable.add ( *pNetChan );
    return *pNetChan;
}

void cac::repeaterSubscribeConfirmNotify ()
{
    if ( this->pudpiiu ) {
        this->pudpiiu->repeaterConfirmNotify ();
    }
}

bool cac::transferChanToVirtCircuit ( 
    epicsGuard < callbackMutex > & cbGuard, unsigned cid, unsigned sid, // X aCC 431
    ca_uint16_t typeCode, arrayElementCount count, 
    unsigned minorVersionNumber, const osiSockAddr & addr )
{
    bool newIIU = false;
    tcpiiu * piiu = 0;

    if ( addr.sa.sa_family != AF_INET ) {
        return false;
    }

    {
        epicsGuard < epicsMutex > guard ( this->mutex );

        /*
         * ignore search replies for deleted channels
         */
        nciu * pChan = this->chanTable.lookup ( cid );
        if ( ! pChan ) {
            return false;
        }

        /*
         * Ignore duplicate search replies
         */
        osiSockAddr chanAddr = pChan->getPIIU()->getNetworkAddress ();
        if ( chanAddr.sa.sa_family != AF_UNSPEC ) {
            if ( ! sockAddrAreIdentical ( &addr, &chanAddr ) ) {
                char acc[64];
                pChan->getPIIU()->hostName ( acc, sizeof ( acc ) );
                msgForMultiplyDefinedPV * pMsg = new ( this->mdpvFreeList )
                    msgForMultiplyDefinedPV ( this->ipToAEngine, 
                        *this, pChan->pName (), acc );
                pMsg->ioInitiate ( addr );
            }
            return false;
        }

        /*
         * look for an existing virtual circuit
         */
        caServerID servID ( addr.ia, pChan->getPriority() );
        piiu = this->serverTable.lookup ( servID );
        if ( piiu ) {
            if ( ! piiu->alive () ) {
                return false;
            }
        }
        else {
            try {
                autoPtrFreeList < tcpiiu, 32, epicsMutexNOOP > pnewiiu (
                            this->freeListVirtualCircuit,
                            new ( this->freeListVirtualCircuit ) tcpiiu ( 
                            *this, this->cbMutex, this->connTMO, this->timerQueue,
                            addr, this->comBufMemMgr, minorVersionNumber, 
                            this->ipToAEngine, pChan->getPriority() ) );
                bhe * pBHE = this->beaconTable.lookup ( addr.ia );
                if ( ! pBHE ) {
                    pBHE = new ( this->bheFreeList ) 
                                        bhe ( this->mutex, epicsTime (), 0u, addr.ia );
                    if ( this->beaconTable.add ( *pBHE ) < 0 ) {
                        return false;
                    }
                }
                this->serverTable.add ( *pnewiiu );
                this->serverList.add ( *pnewiiu );
                pBHE->registerIIU ( guard, *pnewiiu );
                piiu = pnewiiu.release ();
                newIIU = true;
            }
            catch ( std::bad_alloc & ) {
                return false;
            }
            catch ( ... ) {
                this->printf ( "CAC: Unexpected exception during virtual circuit creation\n" );
                return false;
            }
        }

        this->pudpiiu->uninstallChan ( guard, *pChan );
        piiu->installChannel ( cbGuard, guard, *pChan, sid, typeCode, count );

        if ( ! piiu->ca_v42_ok () ) {
            // connect to old server with lock applied
            pChan->connect ( cbGuard, guard );
        }
    }

    if ( newIIU ) {
        piiu->start ();
    }

    return true;
}

void cac::destroyChannel ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );

    // uninstall channel so that recv threads
    // will not start a new callback for this channel's IO. 
    if ( this->chanTable.remove ( chan ) != & chan ) {
        errlogPrintf ( 
            "CAC: Attemt to uninstall unregisterred channel ID=%u ignored.\n",
            chan.getId () );
        return;
    }

    // if the claim reply has not returned yet then we will issue
    // the clear channel request to the server when the claim reply
    // arrives and there is no matching nciu in the client
    if ( chan.connected ( guard ) ) {
        chan.getPIIU()->clearChannelRequest ( 
            guard, chan.getSID(), chan.getCID() );
    }

    {
        // reverse the lock order so that we dont botch the lock hierarchy
        epicsGuardRelease < epicsMutex > unguard ( guard );
        {
            // taking the callback mutex prior to deleting the channel 
            // guarantees that we will not delete a channel out from under a callback
            epicsGuard < callbackMutex > cbGuard ( this->cbMutex );
        }
    }
    
    // run channel's destructor and return it to the free list
    chan.destructor ( guard );
    
    // IIU must be valid until after IO is destroyed in the destructor
    chan.getPIIU()->uninstallChan ( guard, chan );

    this->channelFreeList.release ( & chan );
}

void cac::disconnectAllIO ( 
    epicsGuard < callbackMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, tsDLList < baseNMIU > & ioList )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );

    char buf[128];
    sprintf ( buf, "host = %.100s", chan.pHostName() );

    tsDLIter < baseNMIU > pNetIO = ioList.firstIter();
    while ( pNetIO.valid () ) {
        tsDLIter < baseNMIU > pNext = pNetIO;
        pNext++;
        if ( ! pNetIO->isSubscription() ) {
            this->ioTable.remove ( pNetIO->getId () );
        }
        pNetIO->exception ( guard, *this, ECA_DISCONN, buf );
        pNetIO = pNext;
    }
}

int cac::printf ( const char *pformat, ... ) const
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void cac::flushIfRequired ( epicsGuard < epicsMutex > & guard, netiiu & iiu )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( iiu.flushBlockThreshold ( guard ) ) {
        iiu.flushRequest ( guard );
        // the process thread is not permitted to flush as this
        // can result in a push / pull deadlock on the TCP pipe.
        // Instead, the process thread scheduals the flush with the 
        // send thread which runs at a higher priority than the 
        // send thread. The same applies to the UDP thread for
        // locking hierarchy reasons.
        if ( ! epicsThreadPrivateGet ( caClientCallbackThreadId ) ) {
            // enable / disable of call back preemption must occur here
            // because the tcpiiu might disconnect while waiting and its
            // pointer to this cac might become invalid
            iiu.blockUntilSendBacklogIsReasonable ( this->notify, guard );
        }
    }
    else {
        iiu.flushRequestIfAboveEarlyThreshold ( guard );
    }
}

void cac::writeRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, unsigned type, 
    arrayElementCount nElem, const void * pValue )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->flushIfRequired ( guard, *chan.getPIIU() );
    chan.getPIIU()->writeRequest ( guard, chan, type, nElem, pValue );
}

netWriteNotifyIO & cac::writeNotifyRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, privateInterfaceForIO & icni,
    unsigned type, arrayElementCount nElem, const void * pValue, cacWriteNotify & notifyIn )
{
    guard.assertIdenticalMutex ( this->mutex );
    autoPtrRecycle  < netWriteNotifyIO > pIO ( 
        guard, this->ioTable, *this, 
        netWriteNotifyIO::factory ( this->freeListWriteNotifyIO, icni, notifyIn ) );
    this->ioTable.add ( *pIO );
    this->flushIfRequired ( guard, *chan.getPIIU() );
    chan.getPIIU()->writeNotifyRequest ( 
        guard, chan, *pIO, type, nElem, pValue );
    return *pIO.release();
}

netReadNotifyIO & cac::readNotifyRequest ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, privateInterfaceForIO & icni,
    unsigned type, arrayElementCount nElem, cacReadNotify & notifyIn )
{
    guard.assertIdenticalMutex ( this->mutex );
    autoPtrRecycle  < netReadNotifyIO > pIO ( 
        guard, this->ioTable, *this,
        netReadNotifyIO::factory ( this->freeListReadNotifyIO, icni, notifyIn ) );
    this->ioTable.add ( *pIO );
    this->flushIfRequired ( guard, *chan.getPIIU() );
    chan.getPIIU()->readNotifyRequest ( guard, chan, *pIO, type, nElem );
    return *pIO.release();
}

baseNMIU * cac::destroyIO (
    epicsGuard < epicsMutex > & guard, 
    const cacChannel::ioid & idIn, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );
    // unistall the IO object so that a receive thread will not find it,
    // but do _not_ hold the callback lock here because this could result 
    // in deadlock
    baseNMIU * pIO = this->ioTable.remove ( idIn );
    if ( pIO ) {
        class netSubscription * pSubscr = pIO->isSubscription ();
        if ( pSubscr && chan.connected ( guard ) ) {
            chan.getPIIU()->subscriptionCancelRequest ( 
                guard, chan, *pSubscr );  
        }

        {
            // reverse the lock order so that we dont botch the lock hierarchy
            epicsGuardRelease < epicsMutex > unguard ( guard );
            {
                // taking the callback mutex prior to deleting the IO and channel 
                // guarantees that we will not delete a channel out from under a callback
                epicsGuard < callbackMutex > cbGuard ( this->cbMutex );
                epicsGuard < epicsMutex > tmpGuard ( this->mutexRef () );
                // this uninstalls from the list and destroys the IO
                pIO->exception ( tmpGuard, *this,
                    ECA_CHANDESTROY, chan.pName() );
            }
        }
    }       
    return pIO;
}

void cac::ioShow ( const cacChannel::ioid & idIn, unsigned level ) const
{
    epicsGuard < epicsMutex > autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.lookup ( idIn );
    if ( pmiu ) {
        pmiu->show ( level );
    }
}

void cac::ioExceptionNotify ( 
    unsigned idIn, int status, const char * pContext, 
    unsigned type, arrayElementCount count )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    baseNMIU * pmiu = this->ioTable.lookup ( idIn );
    if ( pmiu ) {
        pmiu->exception ( guard, *this, status, pContext, type, count );
    }
}

void cac::ioExceptionNotifyAndUninstall ( 
    unsigned idIn, int status, const char * pContext, 
    unsigned type, arrayElementCount count )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( idIn );
    if ( pmiu ) {
        pmiu->exception ( guard, *this, status, pContext, type, count );
    }
}

void cac::recycleReadNotifyIO ( 
    epicsGuard < epicsMutex > & guard, netReadNotifyIO & io )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->freeListReadNotifyIO.release ( & io );
}

void cac::recycleWriteNotifyIO ( 
    epicsGuard < epicsMutex > & guard, netWriteNotifyIO & io )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->freeListWriteNotifyIO.release ( & io );
}

void cac::recycleSubscription ( 
    epicsGuard < epicsMutex > & guard, netSubscription & io )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->freeListSubscription.release ( & io );
}

netSubscription & cac::subscriptionRequest ( 
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, privateInterfaceForIO & privChan,
    unsigned type, // X aCC 361
    arrayElementCount nElem, unsigned mask, 
    cacStateNotify & notifyIn )
{
    guard.assertIdenticalMutex ( this->mutex );
    autoPtrRecycle  < netSubscription > pIO ( 
        guard, this->ioTable, *this, 
        netSubscription::factory ( this->freeListSubscription,
                                   privChan, type, nElem, mask, notifyIn ) );
    this->ioTable.add ( *pIO );
    if ( chan.connected ( guard ) ) {
        this->flushIfRequired ( guard, *chan.getPIIU() );
        chan.getPIIU()->subscriptionRequest ( guard, chan, *pIO );
    }
    return *pIO.release ();
}

bool cac::versionAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
    const epicsTime &, const caHdrLargeArray &, void * )
{
    return true;
}
 
bool cac::echoRespAction ( 
    epicsGuard < callbackMutex > & cbGuard, tcpiiu & iiu, 
    const epicsTime & current, const caHdrLargeArray &, void * )
{
    iiu.probeResponseNotify ( cbGuard, current );
    return true;
}

bool cac::writeNotifyRespAction ( 
    epicsGuard < callbackMutex > &, tcpiiu &, 
    const epicsTime &, const caHdrLargeArray & hdr, void * )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( hdr.m_available );
    if ( pmiu ) {
        if ( hdr.m_cid == ECA_NORMAL ) {
            pmiu->completion ( guard, *this );
        }
        else {
            pmiu->exception ( guard, *this,
                hdr.m_cid, "write notify request rejected" );
        }
    }
    return true;
}

bool cac::readNotifyRespAction ( epicsGuard < callbackMutex > &, tcpiiu & iiu, 
    const epicsTime &, const caHdrLargeArray & hdr, void * pMsgBdy )
{
    /*
     * the channel id field is abused for
     * read notify status starting with CA V4.1
     */
    int caStatus;
    if ( iiu.ca_v41_ok() ) {
        caStatus = hdr.m_cid;
    }
    else {
        caStatus = ECA_NORMAL;
    }

    /*
     * convert the data buffer from net
     * format to host format
     */
#   ifdef CONVERSION_REQUIRED 
        if ( hdr.m_dataType < NELEMENTS ( cac_dbr_cvrt ) ) {
            ( *cac_dbr_cvrt[ hdr.m_dataType ] ) (
                 pMsgBdy, pMsgBdy, false, hdr.m_count);
        }
        else {
            caStatus = ECA_BADTYPE;
        }
#   endif

    epicsGuard < epicsMutex > guard ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( hdr.m_available );
    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    if ( pmiu ) {
        // if its a circuit-becomes-responsive subscription update 
        // then we need to reinstall the IO into the table
        netSubscription * pSubscr = pmiu->isSubscription ();
        if ( pSubscr ) {
            this->ioTable.add ( *pmiu );
        }
        if ( caStatus == ECA_NORMAL ) {
            pmiu->completion ( guard, *this,
                hdr.m_dataType, hdr.m_count, pMsgBdy );
        }
        else {
            pmiu->exception ( guard, *this, 
                caStatus, "read failed",
                hdr.m_dataType, hdr.m_count );
        }
    }
    return true;
}

bool cac::eventRespAction ( epicsGuard < callbackMutex > &, tcpiiu &iiu, 
    const epicsTime &, const caHdrLargeArray & hdr, void * pMsgBdy )
{   
    int caStatus;

    /*
     * m_postsize = 0 used to be a subscription cancel confirmation, 
     * but is now a noop because the IO block is immediately deleted
     */
    if ( ! hdr.m_postsize ) {
        return true;
    }

    /*
     * the channel id field is abused for
     * read notify status starting with CA V4.1
     */
    if ( iiu.ca_v41_ok() ) {
        caStatus = hdr.m_cid;
    }
    else {
        caStatus = ECA_NORMAL;
    }

    /*
     * convert the data buffer from net format to host format
     */
#   ifdef CONVERSION_REQUIRED 
        if ( hdr.m_dataType < NELEMENTS ( cac_dbr_cvrt ) ) {
            ( *cac_dbr_cvrt [ hdr.m_dataType ] )(
                 pMsgBdy, pMsgBdy, false, hdr.m_count);
        }
        else {
            caStatus = epicsHTON32 ( ECA_BADTYPE );
        }
#   endif

    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    epicsGuard < epicsMutex > guard ( this->mutex );
    baseNMIU * pmiu = this->ioTable.lookup ( hdr.m_available );
    if ( pmiu ) {
        if ( caStatus == ECA_NORMAL ) {
            pmiu->completion ( guard, *this,
                hdr.m_dataType, hdr.m_count, pMsgBdy );
        }
        else {
            pmiu->exception ( guard, *this, caStatus, 
                "subscription update read failed",
                hdr.m_dataType, hdr.m_count );
        }
    }
    return true;
}

bool cac::readRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
    const epicsTime &, const caHdrLargeArray & hdr, void * pMsgBdy )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( hdr.m_available );
    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    if ( pmiu ) {
        pmiu->completion ( guard, *this,
            hdr.m_dataType, hdr.m_count, pMsgBdy );
    }
    return true;
}

bool cac::clearChannelRespAction ( epicsGuard < callbackMutex > &, tcpiiu &, 
    const epicsTime &, const caHdrLargeArray &, void * /* pMsgBdy */ )
{
    return true; // currently a noop
}

bool cac::defaultExcep ( 
    epicsGuard < callbackMutex > &, tcpiiu & iiu, 
    const caHdrLargeArray &, const char * pCtx, unsigned status )
{
    char buf[512];
    char hostName[64];
    iiu.hostName ( hostName, sizeof ( hostName ) );
    sprintf ( buf, "host=%s ctx=%.400s", hostName, pCtx );
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->notify.exception ( guard, status, buf, 0, 0u );
    }
    return true;
}

void cac::exception ( 
    epicsGuard < callbackMutex > & cbGuard, int status, 
    const char *pContext, const char *pFileName, unsigned lineNo )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->notify.exception ( guard, status, pContext, 
        pFileName, lineNo );
}

bool cac::eventAddExcep ( 
    epicsGuard < callbackMutex > &, tcpiiu & /* iiu */, 
    const caHdrLargeArray &hdr, 
    const char *pCtx, unsigned status )
{
    this->ioExceptionNotify ( hdr.m_available, status, pCtx, 
        hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::readExcep ( epicsGuard < callbackMutex > &, tcpiiu &, 
                     const caHdrLargeArray & hdr, 
                     const char * pCtx, unsigned status )
{
    this->ioExceptionNotifyAndUninstall ( hdr.m_available, 
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::writeExcep ( 
    epicsGuard < callbackMutex > & cbGuard, // X aCC 431
    tcpiiu &, const caHdrLargeArray & hdr, 
    const char * pCtx, unsigned status )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( hdr.m_available );
    if ( pChan ) {
        pChan->writeException ( cbGuard, guard, status, pCtx, 
            hdr.m_dataType, hdr.m_count );
    }
    return true;
}

bool cac::readNotifyExcep ( epicsGuard < callbackMutex > &, tcpiiu &, 
                           const caHdrLargeArray &hdr, 
                           const char *pCtx, unsigned status )
{
    this->ioExceptionNotifyAndUninstall ( hdr.m_available, 
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::writeNotifyExcep ( epicsGuard < callbackMutex > &, tcpiiu &, 
                            const caHdrLargeArray &hdr, 
                            const char *pCtx, unsigned status )
{
    this->ioExceptionNotifyAndUninstall ( hdr.m_available, 
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::exceptionRespAction ( epicsGuard < callbackMutex > & cbMutexIn, tcpiiu & iiu, 
    const epicsTime &, const caHdrLargeArray & hdr, void * pMsgBdy )
{
    const caHdr * pReq = reinterpret_cast < const caHdr * > ( pMsgBdy );
    unsigned bytesSoFar = sizeof ( *pReq );
    if ( hdr.m_postsize < bytesSoFar ) {
        return false;
    }
    caHdrLargeArray req;
    req.m_cmmd = epicsNTOH16 ( pReq->m_cmmd );
    req.m_postsize = epicsNTOH16 ( pReq->m_postsize );
    req.m_dataType = epicsNTOH16 ( pReq->m_dataType );
    req.m_count = epicsNTOH16 ( pReq->m_count );
    req.m_cid = epicsNTOH32 ( pReq->m_cid );
    req.m_available = epicsNTOH32 ( pReq->m_available );
    const ca_uint32_t * pLW = reinterpret_cast < const ca_uint32_t * > ( pReq + 1 );
    if ( req.m_postsize == 0xffff ) {
        static const unsigned annexSize = 
            sizeof ( req.m_postsize ) + sizeof ( req.m_count );
        bytesSoFar += annexSize;
        if ( hdr.m_postsize < bytesSoFar ) {
            return false;
        }
        req.m_postsize = epicsNTOH32 ( pLW[0] );
        req.m_count = epicsNTOH32 ( pLW[1] );
        pLW += 2u;
    }

    // execute the exception message
    pExcepProtoStubTCP pStub;
    if ( hdr.m_cmmd >= NELEMENTS ( cac::tcpExcepJumpTableCAC ) ) {
        pStub = &cac::defaultExcep;
    }
    else {
        pStub = cac::tcpExcepJumpTableCAC [req.m_cmmd];
    }
    const char *pCtx = reinterpret_cast < const char * > ( pLW );
    return ( this->*pStub ) ( cbMutexIn, iiu, req, pCtx, hdr.m_available );
}

bool cac::accessRightsRespAction (
    epicsGuard < callbackMutex > & cbGuard, tcpiiu &, // X aCC 431
    const epicsTime &, const caHdrLargeArray & hdr, void * /* pMsgBdy */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( pChan ) {
        unsigned ar = hdr.m_available;
        caAccessRights accessRights ( 
            ( ar & CA_PROTO_ACCESS_RIGHT_READ ) ? true : false, 
            ( ar & CA_PROTO_ACCESS_RIGHT_WRITE ) ? true : false); 
        pChan->accessRightsStateChange ( accessRights, cbGuard, guard );
    }

    return true;
}

bool cac::createChannelRespAction (
    epicsGuard < callbackMutex > &cbGuard, tcpiiu & iiu, // X aCC 431
    const epicsTime &, const caHdrLargeArray & hdr, void * /* pMsgBdy */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( pChan ) {
        unsigned sidTmp;
        if ( iiu.ca_v44_ok() ) {
            sidTmp = hdr.m_available;
        }
        else {
            sidTmp = pChan->getSID ();
        }
        iiu.connectNotify ( guard, *pChan );
        pChan->connect ( hdr.m_dataType, hdr.m_count, sidTmp, 
            cbGuard, guard );
    }
    else if ( iiu.ca_v44_ok() ) {
        // this indicates a claim response for a resource that does
        // not exist in the client - so just remove it from the server
        iiu.clearChannelRequest ( guard, hdr.m_available, hdr.m_cid );
    }

    return true; 
}

bool cac::verifyAndDisconnectChan ( 
    epicsGuard < callbackMutex > & cbGuard, tcpiiu & /* iiu */, 
    const epicsTime & currentTime, const caHdrLargeArray & hdr, void * /* pMsgBdy */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( ! pChan ) {
        return true;
    }
    this->disconnectChannel ( currentTime, cbGuard, guard, *pChan );
    return true;
}

void cac::disconnectChannel (
        const epicsTime & currentTime, 
        epicsGuard < callbackMutex > & cbGuard, // X aCC 431
        epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );
    assert ( this->pudpiiu );
    chan.disconnectAllIO ( cbGuard, guard );
    chan.getPIIU()->uninstallChan ( guard, chan );
    this->pudpiiu->installDisconnectedChannel ( chan );
    chan.setServerAddressUnknown ( *this->pudpiiu, guard );
    chan.unresponsiveCircuitNotify ( cbGuard, guard );
}

bool cac::badTCPRespAction ( epicsGuard < callbackMutex > &, tcpiiu & iiu, 
    const epicsTime &, const caHdrLargeArray & hdr, void * /* pMsgBdy */ )
{
    char hostName[64];
    iiu.hostName ( hostName, sizeof ( hostName ) );
    this->printf ( "CAC: Undecipherable TCP message ( bad response type %u ) from %s\n", 
        hdr.m_cmmd, hostName );
    return false;
}

bool cac::executeResponse ( epicsGuard < callbackMutex > & cbLocker, tcpiiu & iiu, 
    const epicsTime & currentTime, caHdrLargeArray & hdr, char * pMshBody )
{
    // execute the response message
    pProtoStubTCP pStub;
    if ( hdr.m_cmmd >= NELEMENTS ( cac::tcpJumpTableCAC ) ) {
        pStub = &cac::badTCPRespAction;
    }
    else {
        pStub = cac::tcpJumpTableCAC [hdr.m_cmmd];
    }
    return ( this->*pStub ) ( cbLocker, iiu, currentTime, hdr, pMshBody );
}

void cac::selfTest ( 
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    this->chanTable.verify ();
    this->ioTable.verify ();
    this->beaconTable.verify ();
}

void cac::initiateAbortShutdown ( tcpiiu & iiu )
{
    int exception = ECA_DISCONN;
    char hostNameTmp[64];
    bool exceptionNeeded = false;
    epicsGuard < callbackMutex > cbGuard ( this->cbMutex );
    {
        epicsGuard < epicsMutex > guard ( this->mutex );

        if ( iiu.channelCount( cbGuard ) ) {
            iiu.hostName ( hostNameTmp, sizeof ( hostNameTmp ) );
            if ( iiu.connecting () ) {
                exception = ECA_CONNSEQTMO;
            }
            exceptionNeeded = true;
        }

        iiu.initiateAbortShutdown ( cbGuard, guard );

        // Disconnect all channels immediately from the timer thread
        // because on certain OS such as HPUX it's difficult to 
        // unblock a blocking send() call, and we need immediate 
        // disconnect notification.
        iiu.removeAllChannels ( cbGuard, guard, *this->pudpiiu );
    }

    if ( exceptionNeeded ) {
        genLocalExcep ( cbGuard, *this, exception, hostNameTmp );
    }
}

void cac::destroyIIU ( tcpiiu & iiu )
{
    {
        epicsGuard < callbackMutex > cbGuard ( this->cbMutex );
        epicsGuard < epicsMutex > guard ( this->mutex );
        if ( iiu.channelCount ( cbGuard ) ) {
            char hostNameTmp[64];
            iiu.hostName ( hostNameTmp, sizeof ( hostNameTmp ) );
            genLocalExcep ( cbGuard, *this, ECA_DISCONN, hostNameTmp );
        }
        osiSockAddr addr = iiu.getNetworkAddress();
        if ( addr.sa.sa_family == AF_INET ) {
            inetAddrID tmp ( addr.ia );
            bhe * pBHE = this->beaconTable.lookup ( tmp );
            if ( pBHE ) {
                pBHE->unregisterIIU ( guard, iiu );
            }
        }
       
        assert ( this->pudpiiu );
        iiu.removeAllChannels ( cbGuard, guard, *this->pudpiiu );
    }

    {
        // this lock synchronizes with a blocking loop
        // in ~cac waiting until no circuits are installed.
        // After the cac lock is released here we must not 
        // access any part of the cac (including the 
        // callback lock) because ~cac is allowed to
        // complete.
        epicsGuard < epicsMutex > guard ( this->mutex );

        this->serverTable.remove ( iiu );
        this->serverList.remove ( iiu );
        iiu.~tcpiiu ();
        this->freeListVirtualCircuit.release ( & iiu );

        // signal iiu uninstal event so that cac can properly shut down
        this->iiuUninstall.signal();
    }
}

double cac::beaconPeriod ( const nciu & chan ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    const netiiu * pIIU = chan.getConstPIIU ();
    if ( pIIU ) {
        osiSockAddr addr = pIIU->getNetworkAddress ();
        if ( addr.sa.sa_family == AF_INET ) {
            inetAddrID tmp ( addr.ia );
            bhe *pBHE = this->beaconTable.lookup ( tmp );
            if ( pBHE ) {
                return pBHE->period ( guard );
            }
        }
    }
    return - DBL_MAX;
}

void cac::initiateConnect ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );
    assert ( this->pudpiiu );
    this->pudpiiu->installNewChannel ( 
        epicsTime::getCurrent(), chan );
}

void *cacComBufMemoryManager::allocate ( size_t size )
{
    return this->freeList.allocate ( size );
}

void cacComBufMemoryManager::release ( void * pCadaver )
{
    this->freeList.release ( pCadaver );
}

void cac::pvMultiplyDefinedNotify ( msgForMultiplyDefinedPV & mfmdpv, 
    const char * pChannelName, const char * pAcc, const char * pRej )
{
    char buf[256];
    sprintf ( buf, "Channel: \"%.64s\", Connecting to: %.64s, Ignored: %.64s",
            pChannelName, pAcc, pRej );
    {
        epicsGuard < callbackMutex > cbGuard ( this->cbMutex );
        this->exception ( cbGuard, ECA_DBLCHNL, buf, __FILE__, __LINE__ );
    }
    mfmdpv.~msgForMultiplyDefinedPV ();
    this->mdpvFreeList.release ( & mfmdpv );
}

