/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 *
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include <new>
#include <stdexcept>
#include <string> // vxWorks 6.0 requires this include

#include "dbDefs.h"
#include "epicsGuard.h"
#include "epicsVersion.h"
#include "osiProcess.h"
#include "epicsSignal.h"
#include "envDefs.h"
#include "locationException.h"
#include "errlog.h"
#include "epicsExport.h"

#define epicsExportSharedSymbols
#include "addrList.h"
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
#include "noopiiu.h"

static const char pVersionCAC[] =
    "@(#) " EPICS_VERSION_STRING
    ", CA Client Library " __DATE__;

// TCP response dispatch table
const cac::pProtoStubTCP cac::tcpJumpTableCAC [] =
{
    &cac::versionAction,
    &cac::eventRespAction,
    &cac::badTCPRespAction,
    &cac::readRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::searchRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    &cac::badTCPRespAction,
    // legacy CA_PROTO_READ_SYNC used as an echo with legacy server
    &cac::echoRespAction,
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

//
// cac::cac ()
//
cac::cac (
    epicsMutex & mutualExclusionIn,
    epicsMutex & callbackControlIn,
    cacContextNotify & notifyIn ) :
    _refLocalHostName ( localHostNameCache.getReference () ),
    programBeginTime ( epicsTime::getCurrent() ),
    connTMO ( CA_CONN_VERIFY_PERIOD ),
    mutex ( mutualExclusionIn ),
    cbMutex ( callbackControlIn ),
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
    maxContigFrames ( contiguousMsgCountWhichTriggersFlowControl ),
    beaconAnomalyCount ( 0u ),
    iiuExistenceCount ( 0u ),
    cacShutdownInProgress ( false )
{
    if ( ! osiSockAttach () ) {
        throwWithLocation ( udpiiu :: noSocket () );
    }

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

        this->_serverPort =
            envGetInetPortConfigParam ( &EPICS_CA_SERVER_PORT,
                                        static_cast <unsigned short> (CA_SERVER_PORT) );

        status = envGetDoubleConfigParam ( &EPICS_CA_CONN_TMO, &this->connTMO );
        if ( status ) {
            this->connTMO = CA_CONN_VERIFY_PERIOD;
            epicsGuard < epicsMutex > cbGuard ( this->cbMutex );
            errlogPrintf ( "EPICS \"%s\" double fetch failed\n", EPICS_CA_CONN_TMO.name );
            errlogPrintf ( "Defaulting \"%s\" = %f\n", EPICS_CA_CONN_TMO.name, this->connTMO );
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

        int autoMaxBytes;
        if(envGetBoolConfigParam(&EPICS_CA_AUTO_ARRAY_BYTES, &autoMaxBytes))
            autoMaxBytes = 1;

        if(!autoMaxBytes) {
            freeListInitPvt ( &this->tcpLargeRecvBufFreeList, this->maxRecvBytesTCP, 1 );
            if ( ! this->tcpLargeRecvBufFreeList ) {
                throw std::bad_alloc ();
            }
        }
        unsigned bufsPerArray = this->maxRecvBytesTCP / comBuf::capacityBytes ();
        if ( bufsPerArray > 1u ) {
            maxContigFrames = bufsPerArray *
                contiguousMsgCountWhichTriggersFlowControl;
        }
    }
    catch ( ... ) {
        osiSockRelease ();
        delete [] this->pUserName;
        freeListCleanup ( this->tcpSmallRecvBufFreeList );
        if ( this->tcpLargeRecvBufFreeList ) {
            freeListCleanup ( this->tcpLargeRecvBufFreeList );
        }
        this->timerQueue.release ();
        throw;
    }

    /*
     * load user configured tcp name server address list,
     * create virtual circuits, and add them to server table
     */
    ELLLIST dest, tmpList;

    ellInit ( & dest );
    ellInit ( & tmpList );

    addAddrToChannelAccessAddressList ( &tmpList, &EPICS_CA_NAME_SERVERS, this->_serverPort, false );
    removeDuplicateAddresses ( &dest, &tmpList, 0 );

    epicsGuard < epicsMutex > guard ( this->mutex );

    while ( osiSockAddrNode *
        pNode = reinterpret_cast < osiSockAddrNode * > ( ellGet ( & dest ) ) ) {
        tcpiiu * piiu = NULL;
        SearchDestTCP * pdst = new SearchDestTCP ( *this, pNode->addr );
        this->registerSearchDest ( guard, * pdst );
        /* Initially assume that servers listed in EPICS_CA_NAME_SERVERS support at least minor
         * version 11.  This causes tcpiiu to send the user and host name authentication
         * messages.  When the actual Version message is received from the server it will
         * be overwrite this assumption.
         */
        bool newIIU = findOrCreateVirtCircuit (
            guard, pNode->addr, cacChannel::priorityDefault,
            piiu, 11, pdst );
        free ( pNode );
        if ( newIIU ) {
            piiu->start ( guard );
        }
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
    {
        epicsGuard < epicsMutex > cbGuard ( this->cbMutex );
        epicsGuard < epicsMutex > guard ( this->mutex );
        if ( this->pudpiiu ) {
            this->pudpiiu->shutdown ( cbGuard, guard );

            // make sure no new tcp circuits are created
            this->cacShutdownInProgress = true;

            //
            // shutdown all tcp circuits
            //
            tsDLIter < tcpiiu > iter = this->circuitList.firstIter ();
            while ( iter.valid() ) {
                // this causes a clean shutdown to occur
                iter->unlinkAllChannels ( cbGuard, guard );
                iter++;
            }
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
        while ( this->iiuExistenceCount > 0 ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->iiuUninstall.wait ();
        }
    }

    if ( this->pudpiiu ) {
        delete this->pudpiiu;
    }

    freeListCleanup ( this->tcpSmallRecvBufFreeList );
    if ( this->tcpLargeRecvBufFreeList ) {
        freeListCleanup ( this->tcpLargeRecvBufFreeList );
    }

    delete [] this->pUserName;

    tsSLList < bhe > tmpBeaconList;
    this->beaconTable.removeAll ( tmpBeaconList );
    while ( bhe * pBHE = tmpBeaconList.get() ) {
        pBHE->~bhe ();
        this->bheFreeList.release ( pBHE );
    }

    this->timerQueue.release ();

    this->ipToAEngine.release ();

    // clean-up the list of un-notified msg objects
    while ( msgForMultiplyDefinedPV * msg = this->msgMultiPVList.get() ) {
        msg->~msgForMultiplyDefinedPV ();
        this->mdpvFreeList.release ( msg );
    }

    errlogFlush ();

    osiSockRelease ();

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
    tsDLIter < tcpiiu > iter = this->circuitList.firstIter ();
    while ( iter.valid () ) {
        iter->flushRequest ( guard );
        iter++;
    }
}

unsigned cac::circuitCount (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->circuitList.count ();
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

    this->pudpiiu->beaconAnomalyNotify ( guard );

#   ifdef DEBUG
    {
        char buf[128];
        addr.name ( buf, sizeof ( buf ) );
        ::printf ( "New server available: %s\n", buf );
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
            guard, this->timerQueue, this->cbMutex,
            this->mutex, this->notify, *this, this->_serverPort,
            this->searchDestList );
    }

    nciu * pNetChan = new ( this->channelFreeList )
            nciu ( *this, noopIIU, chan, pName, pri );
    this->chanTable.idAssignAdd ( *pNetChan );
    return *pNetChan;
}

bool cac::findOrCreateVirtCircuit (
    epicsGuard < epicsMutex > & guard, const osiSockAddr & addr,
    unsigned priority, tcpiiu *& piiu, unsigned minorVersionNumber,
    SearchDestTCP * pSearchDest )
{
    guard.assertIdenticalMutex ( this->mutex );
    bool newIIU = false;

    if ( piiu ) {
        if ( ! piiu->alive ( guard ) ) {
            return newIIU;
        }
    }
    else {
        try {
            autoPtrFreeList < tcpiiu, 32, epicsMutexNOOP > pnewiiu (
                    this->freeListVirtualCircuit,
                    new ( this->freeListVirtualCircuit ) tcpiiu (
                        *this, this->mutex, this->cbMutex, this->notify, this->connTMO,
                        this->timerQueue, addr, this->comBufMemMgr, minorVersionNumber,
                        this->ipToAEngine, priority, pSearchDest ) );

            bhe * pBHE = this->beaconTable.lookup ( addr.ia );
            if ( ! pBHE ) {
                pBHE = new ( this->bheFreeList )
                                    bhe ( this->mutex, epicsTime (), 0u, addr.ia );
                if ( this->beaconTable.add ( *pBHE ) < 0 ) {
                    return newIIU;
                }
            }
            this->serverTable.add ( *pnewiiu );
            this->circuitList.add ( *pnewiiu );
            this->iiuExistenceCount++;
            pBHE->registerIIU ( guard, *pnewiiu );
            piiu = pnewiiu.release ();
            newIIU = true;
        }
        catch ( std :: exception & except ) {
            errlogPrintf (
                "CAC: exception during virtual circuit creation \"%s\"\n",
                except.what () );
            return newIIU;
        }
        catch ( ... ) {
            errlogPrintf (
                "CAC: Nonstandard exception during virtual circuit creation\n" );
            return newIIU;
        }
    }
    return newIIU;
}

void cac::transferChanToVirtCircuit (
    unsigned cid, unsigned sid,
    ca_uint16_t typeCode, arrayElementCount count,
    unsigned minorVersionNumber, const osiSockAddr & addr,
    const epicsTime & currentTime )
{
    if ( addr.sa.sa_family != AF_INET ) {
        return;
    }

    epicsGuard < epicsMutex > guard ( this->mutex );

    /*
     * Do not open new circuits while the cac is shutting down
     */
    if ( this->cacShutdownInProgress ) {
        return;
    }

    /*
     * ignore search replies for deleted channels
     */
    nciu * pChan = this->chanTable.lookup ( cid );
    if ( ! pChan ) {
        return;
    }

    /*
     * Ignore duplicate search replies
     */
    osiSockAddr chanAddr = pChan->getPIIU(guard)->getNetworkAddress (guard);

    if ( chanAddr.sa.sa_family != AF_UNSPEC ) {
        if ( ! sockAddrAreIdentical ( &addr, &chanAddr ) ) {
            char acc[64];
            pChan->getPIIU(guard)->getHostName ( guard, acc, sizeof ( acc ) );
            msgForMultiplyDefinedPV * pMsg = new ( this->mdpvFreeList )
                msgForMultiplyDefinedPV ( this->ipToAEngine,
                    *this, pChan->pName ( guard ), acc );
            // cac keeps a list of these objects for proper clean-up in ~cac
            this->msgMultiPVList.add ( *pMsg );
            // It is possible for the ioInitiate call below to
            // call the callback directly if queue quota is exceeded.
            // This callback takes the callback lock and therefore we
            // must release the primary mutex here to avoid a lock
            // hierarchy inversion.
            epicsGuardRelease < epicsMutex > unguard ( guard );
            pMsg->ioInitiate ( addr );
        }
        return;
    }

    caServerID servID ( addr.ia, pChan->getPriority(guard) );
    tcpiiu * piiu = this->serverTable.lookup ( servID );

    bool newIIU = findOrCreateVirtCircuit (
        guard, addr,
        pChan->getPriority(guard), piiu, minorVersionNumber );

    // must occur before moving to new iiu
    pChan->getPIIU(guard)->uninstallChanDueToSuccessfulSearchResponse (
        guard, *pChan, currentTime );
    if ( piiu ) {
        piiu->installChannel (
            guard, *pChan, sid, typeCode, count );

        if ( newIIU ) {
            piiu->start ( guard );
        }
    }
}

void cac::destroyChannel (
    epicsGuard < epicsMutex > & guard,
    nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );

    // uninstall channel so that recv threads
    // will not start a new callback for this channel's IO.
    if ( this->chanTable.remove ( chan ) != & chan ) {
        throw std::logic_error ( "Invalid channel identifier" );
    }
    chan.~nciu ();
    this->channelFreeList.release ( & chan );
}

void cac::disconnectAllIO (
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard,
    nciu & chan, tsDLList < baseNMIU > & ioList )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );
    char buf[128];
    chan.getHostName ( guard, buf, sizeof ( buf ) );

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

int cac :: printFormated (
    epicsGuard < epicsMutex > & callbackControl,
        const char * pformat, ... ) const
{
    va_list theArgs;
    va_start ( theArgs, pformat );
    int status = this->varArgsPrintFormated ( callbackControl, pformat, theArgs );
    va_end ( theArgs );
    return status;
}

netWriteNotifyIO & cac::writeNotifyRequest (
    epicsGuard < epicsMutex > & guard, nciu & chan, privateInterfaceForIO & icni,
    unsigned type, arrayElementCount nElem, const void * pValue, cacWriteNotify & notifyIn )
{
    guard.assertIdenticalMutex ( this->mutex );
    autoPtrRecycle  < netWriteNotifyIO > pIO (
        guard, this->ioTable, *this,
        netWriteNotifyIO::factory ( this->freeListWriteNotifyIO, icni, notifyIn ) );
    this->ioTable.idAssignAdd ( *pIO );
    chan.getPIIU(guard)->writeNotifyRequest (
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
    this->ioTable.idAssignAdd ( *pIO );
    chan.getPIIU(guard)->readNotifyRequest ( guard, chan, *pIO, type, nElem );
    return *pIO.release();
}

bool cac::destroyIO (
    CallbackGuard & callbackGuard,
    epicsGuard < epicsMutex > & guard,
    const cacChannel::ioid & idIn, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );

    baseNMIU * pIO = this->ioTable.remove ( idIn );
    if ( pIO ) {
        class netSubscription * pSubscr = pIO->isSubscription ();
        if ( pSubscr ) {
            pSubscr->unsubscribeIfRequired ( guard, chan );
        }

        // this uninstalls from the list and destroys the IO
        pIO->exception ( guard, *this,
            ECA_CHANDESTROY, chan.pName ( guard ) );
        return true;
    }
    return false;
}

void cac::ioShow (
    epicsGuard < epicsMutex > & guard,
    const cacChannel::ioid & idIn, unsigned level ) const
{
    baseNMIU * pmiu = this->ioTable.lookup ( idIn );
    if ( pmiu ) {
        pmiu->show ( guard, level );
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
    unsigned type,
    arrayElementCount nElem, unsigned mask,
    cacStateNotify & notifyIn,
    bool chanIsInstalled )
{
    guard.assertIdenticalMutex ( this->mutex );
    autoPtrRecycle  < netSubscription > pIO (
        guard, this->ioTable, *this,
        netSubscription::factory ( this->freeListSubscription,
                                   privChan, type, nElem, mask, notifyIn ) );
    this->ioTable.idAssignAdd ( *pIO );
    if ( chanIsInstalled ) {
        pIO->subscribeIfRequired ( guard, chan );
    }
    return *pIO.release ();
}

bool cac::versionAction ( callbackManager &, tcpiiu & iiu,
    const epicsTime &, const caHdrLargeArray & msg, void * )
{
    iiu.versionRespNotify ( msg );
    return true;
}

bool cac::echoRespAction (
    callbackManager & mgr, tcpiiu & iiu,
    const epicsTime & /* current */, const caHdrLargeArray &, void * )
{
    iiu.probeResponseNotify ( mgr.cbGuard );
    return true;
}

bool cac::writeNotifyRespAction (
    callbackManager &, tcpiiu &,
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

bool cac::readNotifyRespAction ( callbackManager &, tcpiiu & iiu,
    const epicsTime &, const caHdrLargeArray & hdr, void * pMsgBdy )
{
    epicsGuard < epicsMutex > guard ( this->mutex );

    /*
     * the channel id field is abused for
     * read notify status starting with CA V4.1
     */
    int caStatus;
    if ( iiu.ca_v41_ok ( guard ) ) {
        caStatus = hdr.m_cid;
    }
    else {
        caStatus = ECA_NORMAL;
    }

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
            // this does *not* assign a new resource id
            this->ioTable.add ( *pmiu );
        }
        if ( caStatus == ECA_NORMAL ) {
            /*
             * convert the data buffer from net
             * format to host format
             */
            caStatus = caNetConvert (
                hdr.m_dataType, pMsgBdy, pMsgBdy, false, hdr.m_count );
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

bool cac::searchRespAction ( callbackManager &, tcpiiu & iiu,
    const epicsTime & currentTime, const caHdrLargeArray & msg,
        void * /* pMsgBdy */ )
{
    assert ( this->pudpiiu );
    iiu.searchRespNotify ( currentTime, msg );
    return true;
}

bool cac::eventRespAction ( callbackManager &, tcpiiu &iiu,
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

    epicsGuard < epicsMutex > guard ( this->mutex );

    /*
     * the channel id field is abused for
     * read notify status starting with CA V4.1
     */
    if ( iiu.ca_v41_ok ( guard ) ) {
        caStatus = hdr.m_cid;
    }
    else {
        caStatus = ECA_NORMAL;
    }

    //
    // The IO destroy routines take the call back mutex
    // when uninstalling and deleting the baseNMIU so there is
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    baseNMIU * pmiu = this->ioTable.lookup ( hdr.m_available );
    if ( pmiu ) {
        /*
         * convert the data buffer from net format to host format
         */
        if ( caStatus == ECA_NORMAL ) {
            caStatus = caNetConvert (
                hdr.m_dataType, pMsgBdy, pMsgBdy, false, hdr.m_count );
        }
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

bool cac::readRespAction ( callbackManager &, tcpiiu &,
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

bool cac::clearChannelRespAction ( callbackManager &, tcpiiu &,
    const epicsTime &, const caHdrLargeArray &, void * /* pMsgBody */ )
{
    return true; // currently a noop
}

bool cac::defaultExcep (
    callbackManager &, tcpiiu & iiu,
    const caHdrLargeArray &, const char * pCtx, unsigned status )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    char buf[512];
    char hostName[64];
    iiu.getHostName ( guard, hostName, sizeof ( hostName ) );
    sprintf ( buf, "host=%s ctx=%.400s", hostName, pCtx );
    this->notify.exception ( guard, status, buf, 0, 0u );
    return true;
}

void cac::exception (
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard, int status,
    const char * pContext, const char * pFileName, unsigned lineNo )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );
    this->notify.exception ( guard, status, pContext,
        pFileName, lineNo );
}

bool cac::eventAddExcep (
    callbackManager &, tcpiiu &,
    const caHdrLargeArray &hdr,
    const char *pCtx, unsigned status )
{
    this->ioExceptionNotify ( hdr.m_available, status, pCtx,
        hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::readExcep ( callbackManager &, tcpiiu &,
                     const caHdrLargeArray & hdr,
                     const char * pCtx, unsigned status )
{
    this->ioExceptionNotifyAndUninstall ( hdr.m_available,
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::writeExcep (
    callbackManager & mgr,
    tcpiiu &, const caHdrLargeArray & hdr,
    const char * pCtx, unsigned status )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( hdr.m_available );
    if ( pChan ) {
        pChan->writeException ( mgr.cbGuard, guard, status, pCtx,
            hdr.m_dataType, hdr.m_count );
    }
    return true;
}

bool cac::readNotifyExcep ( callbackManager &, tcpiiu &,
                           const caHdrLargeArray &hdr,
                           const char *pCtx, unsigned status )
{
    this->ioExceptionNotifyAndUninstall ( hdr.m_available,
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::writeNotifyExcep ( callbackManager &, tcpiiu &,
                            const caHdrLargeArray &hdr,
                            const char *pCtx, unsigned status )
{
    this->ioExceptionNotifyAndUninstall ( hdr.m_available,
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::exceptionRespAction ( callbackManager & cbMutexIn, tcpiiu & iiu,
    const epicsTime &, const caHdrLargeArray & hdr, void * pMsgBdy )
{
    const caHdr * pReq = reinterpret_cast < const caHdr * > ( pMsgBdy );
    unsigned bytesSoFar = sizeof ( *pReq );
    if ( hdr.m_postsize < bytesSoFar ) {
        return false;
    }
    caHdrLargeArray req;
    req.m_cmmd = AlignedWireRef < const epicsUInt16 > ( pReq->m_cmmd );
    req.m_postsize = AlignedWireRef < const epicsUInt16 > ( pReq->m_postsize );
    req.m_dataType = AlignedWireRef < const epicsUInt16 > ( pReq->m_dataType );
    req.m_count = AlignedWireRef < const epicsUInt16 > ( pReq->m_count );
    req.m_cid = AlignedWireRef < const epicsUInt32 > ( pReq->m_cid );
    req.m_available = AlignedWireRef < const epicsUInt32 > ( pReq->m_available );
    const ca_uint32_t * pLW = reinterpret_cast < const ca_uint32_t * > ( pReq + 1 );
    if ( req.m_postsize == 0xffff ) {
        static const unsigned annexSize =
            sizeof ( req.m_postsize ) + sizeof ( req.m_count );
        bytesSoFar += annexSize;
        if ( hdr.m_postsize < bytesSoFar ) {
            return false;
        }
        req.m_postsize = AlignedWireRef < const epicsUInt32 > ( pLW[0] );
        req.m_count = AlignedWireRef < const epicsUInt32 > ( pLW[1] );
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
    callbackManager & mgr, tcpiiu &,
    const epicsTime &, const caHdrLargeArray & hdr, void * /* pMsgBody */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( pChan ) {
        unsigned ar = hdr.m_available;
        caAccessRights accessRights (
            ( ar & CA_PROTO_ACCESS_RIGHT_READ ) ? true : false,
            ( ar & CA_PROTO_ACCESS_RIGHT_WRITE ) ? true : false);
        pChan->accessRightsStateChange ( accessRights, mgr.cbGuard, guard );
    }

    return true;
}

bool cac::createChannelRespAction (
    callbackManager & mgr, tcpiiu & iiu,
    const epicsTime &, const caHdrLargeArray & hdr, void * /* pMsgBody */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( pChan ) {
        unsigned sidTmp;
        if ( iiu.ca_v44_ok ( guard ) ) {
            sidTmp = hdr.m_available;
        }
        else {
            sidTmp = pChan->getSID (guard);
        }
        bool wasExpected = iiu.connectNotify ( guard, *pChan );
        if ( wasExpected ) {
            pChan->connect ( hdr.m_dataType, hdr.m_count, sidTmp,
                mgr.cbGuard, guard );
        }
        else {
            errlogPrintf (
                "CA Client Library: Ignored duplicate create channel "
                "response from CA server?\n" );
        }
    }
    else if ( iiu.ca_v44_ok ( guard ) ) {
        // this indicates a claim response for a resource that does
        // not exist in the client - so just remove it from the server
        iiu.clearChannelRequest ( guard, hdr.m_available, hdr.m_cid );
    }

    return true;
}

bool cac::verifyAndDisconnectChan (
    callbackManager & mgr, tcpiiu &,
    const epicsTime &, const caHdrLargeArray & hdr, void * /* pMsgBody */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( ! pChan ) {
        return true;
    }
    this->disconnectChannel ( mgr.cbGuard, guard, *pChan );
    return true;
}

void cac::disconnectChannel (
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );
    assert ( this->pudpiiu );
    chan.disconnectAllIO ( cbGuard, guard );
    chan.getPIIU(guard)->uninstallChan ( guard, chan );
    this->pudpiiu->installDisconnectedChannel ( guard, chan );
    chan.unresponsiveCircuitNotify ( cbGuard, guard );
}

bool cac::badTCPRespAction ( callbackManager &, tcpiiu & iiu,
    const epicsTime &, const caHdrLargeArray & hdr, void * /* pMsgBody */ )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    char hostName[64];
    iiu.getHostName ( guard, hostName, sizeof ( hostName ) );
    errlogPrintf ( "CAC: Undecipherable TCP message ( bad response type %u ) from %s\n",
        hdr.m_cmmd, hostName );
    return false;
}

bool cac::executeResponse ( callbackManager & mgr, tcpiiu & iiu,
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
    return ( this->*pStub ) ( mgr, iiu, currentTime, hdr, pMshBody );
}

void cac::selfTest (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    this->chanTable.verify ();
    this->ioTable.verify ();
    this->beaconTable.verify ();
}

void cac::destroyIIU ( tcpiiu & iiu )
{
    {
        callbackManager mgr ( this->notify, this->cbMutex );
        epicsGuard < epicsMutex > guard ( this->mutex );

        if ( iiu.channelCount ( guard ) ) {
            char hostNameTmp[64];
            iiu.getHostName ( guard, hostNameTmp, sizeof ( hostNameTmp ) );
            genLocalExcep ( mgr.cbGuard, guard, *this, ECA_DISCONN, hostNameTmp );
        }
        osiSockAddr addr = iiu.getNetworkAddress ( guard );
        if ( addr.sa.sa_family == AF_INET ) {
            inetAddrID tmp ( addr.ia );
            bhe * pBHE = this->beaconTable.lookup ( tmp );
            if ( pBHE ) {
                pBHE->unregisterIIU ( guard, iiu );
            }
        }

        assert ( this->pudpiiu );
        iiu.disconnectAllChannels ( mgr.cbGuard, guard, *this->pudpiiu );

        this->serverTable.remove ( iiu );
        this->circuitList.remove ( iiu );
    }

    // this destroys a timer that takes the primary mutex
    // so we must not hold the primary mutex here
    //
    // this waits for send/recv threads to exit
    // this also uses the cac free lists so cac must wait
    // for this to finish before it shuts down

    iiu.~tcpiiu ();

    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->freeListVirtualCircuit.release ( & iiu );
        this->iiuExistenceCount--;
        // signal iiu uninstall event so that cac can properly shut down
        this->iiuUninstall.signal();
    }
    // do not touch "this" after lock is released above
}

double cac::beaconPeriod (
    epicsGuard < epicsMutex > & guard,
    const nciu & chan ) const
{
    const netiiu * pIIU = chan.getConstPIIU ( guard );
    if ( pIIU ) {
        osiSockAddr addr = pIIU->getNetworkAddress ( guard );
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
    epicsGuard < epicsMutex > & guard,
    nciu & chan, netiiu * & piiu )
{
    guard.assertIdenticalMutex ( this->mutex );
    assert ( this->pudpiiu );
    this->pudpiiu->installNewChannel ( guard, chan, piiu );
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
        callbackManager mgr ( this->notify, this->cbMutex );
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->exception ( mgr.cbGuard, guard, ECA_DBLCHNL, buf, __FILE__, __LINE__ );

        // remove from the list under lock
        this->msgMultiPVList.remove ( mfmdpv );
    }
    // delete msg object
    mfmdpv.~msgForMultiplyDefinedPV ();
    this->mdpvFreeList.release ( & mfmdpv );
}

void cac::registerSearchDest (
    epicsGuard < epicsMutex > & guard,
    SearchDest & req )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->searchDestList.add ( req );
}
