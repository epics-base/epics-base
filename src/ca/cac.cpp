
/*  $Id$
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

#include "epicsMemory.h"
#include "osiProcess.h"
#include "osiSigPipeIgnore.h"
#include "envDefs.h"

#include "iocinf.h"
#include "cac.h"
#include "inetAddrID.h"
#include "caServerID.h"
#include "virtualCircuit.h"
#include "syncGroup.h"
#include "nciu.h"
#include "autoPtrRecycle.h"
#include "searchTimer.h"
#include "repeaterSubscribeTimer.h"

#define epicsExportSharedSymbols
#include "udpiiu.h"
#include "bhe.h"
#include "net_convert.h"
#undef epicsExportSharedSymbols

// TCP response dispatch table
const cac::pProtoStubTCP cac::tcpJumpTableCAC [] = 
{
    &cac::noopAction,
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
    &cac::claimCIURespAction,
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
    &cac::defaultExcep,     // CA_PROTO_CLAIM_CIU 
    &cac::writeNotifyExcep, // CA_PROTO_WRITE_NOTIFY
    &cac::defaultExcep,     // CA_PROTO_CLIENT_NAME
    &cac::defaultExcep,     // CA_PROTO_HOST_NAME
    &cac::defaultExcep,     // CA_PROTO_ACCESS_RIGHTS
    &cac::defaultExcep,     // CA_PROTO_ECHO
    &cac::defaultExcep,     // REPEATER_REGISTER
    &cac::defaultExcep,     // CA_PROTO_SIGNAL
    &cac::defaultExcep,     // CA_PROTO_CLAIM_CIU_FAILED
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
    if ( caClientCallbackThreadId ) {
        atexit ( cacExitHandler );
    }
    else {
        throw std::bad_alloc ();
    }
}

//
// cac::cac ()
//
cac::cac ( cacNotify & notifyIn, bool enablePreemptiveCallbackIn ) :
    ipToAEngine ( "caIPAddrToAsciiEngine" ), 
    pudpiiu ( 0 ),
    pSearchTmr ( 0 ),
    pRepeaterSubscribeTmr ( 0 ),
    tcpSmallRecvBufFreeList ( 0 ),
    tcpLargeRecvBufFreeList ( 0 ),
    pCallbackLocker ( 0 ),
    notify ( notifyIn ),
    initializingThreadsPriority ( epicsThreadGetPrioritySelf () ),
    maxRecvBytesTCP ( MAX_TCP ),
    pndRecvCnt ( 0u ), 
    readSeq ( 0u ),
    recvThreadsPendingCount ( 0u )
{
	long status;
    unsigned abovePriority;

    epicsThreadOnce ( &cacOnce, cacOnceFunc, 0 );

	if ( ! osiSockAttach () ) {
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
	}

    {
        epicsThreadBooleanStatus tbs;

        tbs = epicsThreadLowestPriorityLevelAbove ( this->initializingThreadsPriority, &abovePriority );
        if ( tbs != epicsThreadBooleanStatusSuccess ) {
            abovePriority = this->initializingThreadsPriority;
        }
    }

    installSigPipeIgnore ();

    {
        char tmp[256];
        size_t len;
        osiGetUserNameReturn gunRet;

        gunRet = osiGetUserName ( tmp, sizeof (tmp) );
        if ( gunRet != osiGetUserNameSuccess ) {
            tmp[0] = '\0';
        }
        len = strlen ( tmp ) + 1;
        // Tornado II doesnt like new ( std::nothrow )
        this->pUserName = new /*( std::nothrow )*/ char [ len ];
        if ( ! this->pUserName ) {
            throw std::bad_alloc ();
        }
        strncpy ( this->pUserName, tmp, len );
    }

    this->programBeginTime = epicsTime::getCurrent ();

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
        osiSockRelease ();
        delete [] this->pUserName;
        throw std::bad_alloc ();
    }

    freeListInitPvt ( &this->tcpLargeRecvBufFreeList, this->maxRecvBytesTCP, 1 );
    if ( ! this->tcpLargeRecvBufFreeList ) {
        osiSockRelease ();
        delete [] this->pUserName;
        freeListCleanup ( this->tcpSmallRecvBufFreeList );
        throw std::bad_alloc ();
    }

    this->pTimerQueue = & epicsTimerQueueActive::allocate ( false, abovePriority );
    if ( ! this->pTimerQueue ) {
        osiSockRelease ();
        delete [] this->pUserName;
        freeListCleanup ( this->tcpSmallRecvBufFreeList );
        freeListCleanup ( this->tcpLargeRecvBufFreeList );
        throw std::bad_alloc ();
    }

    if ( ! enablePreemptiveCallbackIn ) {
        // Tornado II doesnt like new ( std::nothrow )
        this->pCallbackLocker = new /*( std::nothrow )*/ callbackAutoMutex ( *this );
        if ( ! this->pCallbackLocker ) {
            osiSockRelease ();
            delete [] this->pUserName;
            freeListCleanup ( this->tcpSmallRecvBufFreeList );
            freeListCleanup ( this->tcpLargeRecvBufFreeList );
            this->pTimerQueue->release ();
            throw std::bad_alloc ();
        }
    }
}

cac::~cac ()
{
    //
    // release callback lock
    //
    delete this->pCallbackLocker;

    //
    // lock intentionally not held here so that we dont deadlock 
    // waiting for the UDP thread to exit while it is waiting to 
    // get the lock.
    if ( this->pudpiiu ) {
        // this blocks until the UDP thread exits so that
        // it will not sneak in any new clients
        this->pudpiiu->shutdown ();
    }

    //
    // shutdown all tcp connections
    //
    {
        epicsAutoMutex autoMutex ( this->mutex );
        this->serverTable.traverse ( & tcpiiu::cleanShutdown );
    }

    //
    // wait for tcp threads to exit
    //
    while ( this->serverTable.numEntriesInstalled() ) {
        this->iiuUninstal.wait ();
    }

    delete this->pRepeaterSubscribeTmr;
    delete this->pSearchTmr;

    freeListCleanup ( this->tcpSmallRecvBufFreeList );
    freeListCleanup ( this->tcpLargeRecvBufFreeList );

    {
        epicsAutoMutex autoMutex ( this->mutex );
        if ( this->pudpiiu ) {
            tsDLList < nciu > tmpList;
            this->pudpiiu->uninstallAllChan ( tmpList );
            while ( nciu *pChan = tmpList.get () ) {
                this->disconnectAllIO ( *pChan, false );
                pChan->disconnect ( limboIIU );
                // no need to call disconnect notify or 
                // access rights notify here
                limboIIU.attachChannel ( *pChan );
            }
        }
    }

    delete this->pudpiiu;
    delete [] this->pUserName;

    this->beaconTable.traverse ( &bhe::destroy );

    // if we get here and the IO is still attached then we have a
    // leaked io block that was not registered with a channel.
    if ( this->ioTable.numEntriesInstalled () ) {
        this->printf ( "CAC %u orphaned IO items?\n",
            this->ioTable.numEntriesInstalled () );
    }

    osiSockRelease ();

    this->pTimerQueue->release ();
}

//
// set the push pending flag on all virtual circuits
//
void cac::flushRequest ()
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->flushRequestPrivate ();
}

// lock must be applied
void cac::flushRequestPrivate ()
{
    this->serverTable.traverse ( & tcpiiu::flushRequest );
}

unsigned cac::connectionCount () const
{
    epicsAutoMutex autoMutex ( this->mutex );
    return this->serverTable.numEntriesInstalled ();
}

void cac::show ( unsigned level ) const
{
    epicsAutoMutex autoMutex2 ( this->mutex );

    ::printf ( "Channel Access Client Context at %p for user %s\n", 
        static_cast <const void *> ( this ), this->pUserName );
    if ( level > 0u ) {
        this->serverTable.show ( level - 1u );
        ::printf ( "\tconnection time out watchdog period %f\n", this->connTMO );
        ::printf ( "\tpreemptive calback is %s\n",
            this->pCallbackLocker ? "disabled" : "enabled" );
        ::printf ( "list of installed services:\n" );
        this->services.show ( level - 1u );
    }

    if ( level > 1u ) {
        if ( this->pudpiiu ) {
            this->pudpiiu->show ( level - 2u );
        }
        ::printf ( "\tthere are %u unsatisfied IO operations blocking ca_pend_io()\n",
                this->pndRecvCnt );
    }

    if ( level > 2u ) {
        ::printf ( "Program begin time:\n");
        this->programBeginTime.show ( level - 3u );
        ::printf ( "Channel identifier hash table:\n" );
        this->chanTable.show ( level - 3u );
        ::printf ( "IO identifier hash table:\n" );
        this->ioTable.show ( level - 3u );
        ::printf ( "Synchronous group identifier hash table:\n" );
        this->sgTable.show ( level - 3u );
        ::printf ( "Beacon source identifier hash table:\n" );
        this->beaconTable.show ( level - 3u );
        if ( this->pTimerQueue ) {
            ::printf ( "Timer queue:\n" );
            this->pTimerQueue->show ( level - 3u );
        }
        if ( this->pSearchTmr ) {
            ::printf ( "search message timer:\n" );
            this->pSearchTmr->show ( level - 3u );
        }
        if ( this->pRepeaterSubscribeTmr ) {
            ::printf ( "repeater subscribee timer:\n" );
            this->pRepeaterSubscribeTmr->show ( level - 3u );
        }
        ::printf ( "IP address to name conversion engine:\n" );
        this->ipToAEngine.show ( level - 3u );
        ::printf ( "\tthe current read sequence number is %u\n",
                this->readSeq );
    }

    if ( level > 3u ) {
        ::printf ( "Default mutex:\n");
        this->mutex.show ( level - 4u );
        ::printf ( "IO done event:\n");
        this->ioDone.show ( level - 3u );
        ::printf ( "mutex:\n" );
        this->mutex.show ( level - 3u );
    }
}

/*
 *  cac::beaconNotify
 */
void cac::beaconNotify ( const inetAddrID & addr, const epicsTime & currentTime )
{
    epicsAutoMutex autoMutex ( this->mutex );

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
        if ( ! pBHE->updatePeriod ( this->programBeginTime, currentTime ) ) {
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
        pBHE = new bhe ( currentTime, addr );
        if ( pBHE ) {
            if ( this->beaconTable.add ( *pBHE ) < 0 ) {
                pBHE->destroy ();
            }
        }
        return;
    }

    /*
     * This part is needed when many machines
     * have channels in a disconnected state that 
     * dont exist anywhere on the network. This insures
     * that we dont have many CA clients synchronously
     * flooding the network with broadcasts (and swamping
     * out requests for valid channels).
     *
     * I fetch the local UDP port number and use the low 
     * order bits as a pseudo random delay to prevent every 
     * one from replying at once.
     */
    if ( this->pSearchTmr ) {
        static const double portTicksPerSec = 1000u;
        static const unsigned portBasedDelayMask = 0xff;
        unsigned port = this->pudpiiu->getPort ();
        double delay = ( port & portBasedDelayMask );
        delay /= portTicksPerSec; 
        this->pSearchTmr->resetPeriod ( delay );
    }

    this->pudpiiu->resetChannelRetryCounts ();

#   if DEBUG
    {
        char buf[64];
        ipAddrToDottedIP (pnet_addr, buf, sizeof ( buf ) );
        printf ("new server available: %s\n", buf);
    }
#   endif
}

int cac::pendIO ( const double & timeout )
{
    // prevent recursion nightmares by disabling calls to 
    // pendIO () from within a CA callback. 
    if ( epicsThreadPrivateGet ( caClientCallbackThreadId ) ) {
        return ECA_EVDISALLOW;
    }

    int status = ECA_NORMAL;
    epicsTime beg_time = epicsTime::getCurrent ();
    double remaining = timeout;

    {
        epicsAutoMutex autoMutex ( this->mutex );
        this->flushRequestPrivate ();
    }
   
    while ( this->pndRecvCnt > 0 ) {
        if ( remaining < CAC_SIGNIFICANT_DELAY ) {
            status = ECA_TIMEOUT;
            break;
        }

        {
            // serialize access the blocking mechanism below
            epicsAutoMutex autoMutex ( this->serializePendIO );

            if ( this->pCallbackLocker ) {
                epicsAutoMutexRelease autoRelease ( this->callbackMutex );
                this->ioDone.wait ( remaining );
            }
            else {
                this->ioDone.wait ( remaining );
            }
        }

        double delay = epicsTime::getCurrent () - beg_time;
        if ( delay < timeout ) {
            remaining = timeout - delay;
        }
        else {
            remaining = 0.0;
        }
    }

    {
        epicsAutoMutex autoMutex ( this->mutex );
        this->readSeq++;
        this->pndRecvCnt = 0u;
        if ( this->pudpiiu ) {
            this->pudpiiu->connectTimeoutNotify ();
        }
    }

    return status;
}

int cac::blockForEventAndEnableCallbacks ( epicsEvent &event, double timeout )
{
    epicsAutoMutex autoMutex ( this->serializeCallbackMutexUsage );

    if ( this->pCallbackLocker ) {
        epicsAutoMutexRelease autoMutexRelease ( this->callbackMutex );
        event.wait ( timeout );
    }
    else {
        event.wait ( timeout );
    }

    return ECA_NORMAL;
}

int cac::pendEvent ( const double & timeout )
{
    // prevent recursion nightmares by disabling calls to 
    // pendIO () from within a CA callback. 
    if ( epicsThreadPrivateGet ( caClientCallbackThreadId ) ) {
        return ECA_EVDISALLOW;
    }

    epicsTime current = epicsTime::getCurrent ();

    {
        epicsAutoMutex autoMutex ( this->mutex );
        this->flushRequestPrivate ();
    }

    {
        // serialize access the blocking mechanism below
        epicsAutoMutex autoMutex ( this->serializeCallbackMutexUsage );

        // process at least once if preemptive callback
        // isnt enabled
        if ( this->pCallbackLocker ) {
            epicsAutoMutexRelease autoMutexRelease ( this->callbackMutex );
            while ( this->recvThreadsPendingCount > 1 ) {
                this->noRecvThreadsPending.wait ();
            }
        }
    }

    double elapsed = epicsTime::getCurrent() - current;
    double delay;

    if ( timeout > elapsed ) {
        delay = timeout - elapsed;
    }
    else {
        delay = 0.0;
    }

    if ( delay >= CAC_SIGNIFICANT_DELAY ) {
        if ( this->pCallbackLocker ) {
            epicsAutoMutexRelease autoMutexRelease ( this->callbackMutex );
            epicsThreadSleep ( delay );
        }
        else {
            epicsThreadSleep ( delay );
        }
    }

    return ECA_TIMEOUT;
}

void cac::installCASG ( CASG &sg )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->sgTable.add ( sg );
}

void cac::uninstallCASG ( CASG &sg )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->sgTable.remove ( sg );
}

CASG * cac::lookupCASG ( unsigned id )
{
    epicsAutoMutex autoMutex ( this->mutex );
    CASG * psg = this->sgTable.lookup ( id );
    if ( psg ) {
        if ( ! psg->verify () ) {
            psg = 0;
        }
    }
    return psg;
}

void cac::registerService ( cacService &service )
{
    this->services.registerService ( service );
}

cacChannel & cac::createChannel ( const char * pName, 
    cacChannelNotify & chan, cacChannel::priLev pri )
{
    cacChannel *pIO;

    if ( pri > cacChannel::priorityMax ) {
        throw cacChannel::badPriority ();
    }

    if ( pName == 0 || pName[0] == '\0' ) {
        throw cacChannel::badString ();
    }

    pIO = this->services.createChannel ( pName, chan, pri );
    if ( ! pIO ) {
        pIO = cacGlobalServiceList.createChannel ( pName, chan, pri );
        if ( ! pIO ) {
            if ( ! this->pudpiiu || ! this->pSearchTmr ) {
                if ( ! this->setupUDP () ) {
                    throw ECA_INTERNAL;
                }
            }
            epics_auto_ptr < cacChannel > pNetChan 
                ( new nciu ( *this, limboIIU, chan, pName, pri ) );
            if ( pNetChan.get() ) {
                return *pNetChan.release ();
            }
            else {
                throw std::bad_alloc ();
            }
        }
    }
    return *pIO;
}

void cac::installNetworkChannel ( nciu & chan, netiiu * & piiu )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->chanTable.add ( chan );
    this->pudpiiu->attachChannel ( chan );
    piiu = this->pudpiiu;
    this->pSearchTmr->resetPeriod ( 0.0 );
}

bool cac::setupUDP ()
{
    epicsAutoMutex autoMutex ( this->mutex );

    if ( ! this->pudpiiu ) {
        this->pudpiiu = new udpiiu ( *this );
        if ( ! this->pudpiiu ) {
            return false;
        }
    }

    if ( ! this->pSearchTmr ) {
        this->pSearchTmr = new searchTimer ( *this->pudpiiu, *this->pTimerQueue, this->mutex );
        if ( ! this->pSearchTmr ) {
            return false;
        }
    }

    if ( ! this->pRepeaterSubscribeTmr ) {
        this->pRepeaterSubscribeTmr = new repeaterSubscribeTimer ( *this->pudpiiu, *this->pTimerQueue );
        if ( ! this->pRepeaterSubscribeTmr ) {
            return false;
        }
    }

    return true;
}

void cac::repeaterSubscribeConfirmNotify ()
{
    if ( this->pRepeaterSubscribeTmr ) {
        this->pRepeaterSubscribeTmr->confirmNotify ();
    }
}

bool cac::lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             ca_uint16_t typeCode, arrayElementCount count, 
             unsigned minorVersionNumber, const osiSockAddr & addr,
             const epicsTime & currentTime )
{
    unsigned  retrySeqNumber;

    if ( addr.sa.sa_family != AF_INET ) {
        return false;
    }

    bool v41Ok, v42Ok;
    nciu *chan;
    {
        epicsAutoMutex autoMutex ( this->mutex );

        /*
         * ignore search replies for deleted channels
         */
        chan = this->chanTable.lookup ( cid );
        if ( ! chan ) {
            return true;
        }

        retrySeqNumber = chan->getRetrySeqNo ();

        /*
         * Ignore duplicate search replies
         */
        if ( chan->getPIIU()->isVirtaulCircuit( chan->pName(), addr ) ) {
            return true;
        }

        /*
         * look for an existing virtual circuit
         */
        caServerID servID ( addr.ia, chan->getPriority() );
        tcpiiu * piiu = this->serverTable.lookup ( servID );
        if ( piiu ) {
            if ( ! piiu->alive () ) {
                return true;
            }
        }
        else {
            try {
                piiu = new tcpiiu ( *this, this->connTMO, *this->pTimerQueue,
                            addr, minorVersionNumber, this->ipToAEngine,
                            chan->getPriority() );
                if ( ! piiu ) {
                    return true;
                }
                this->serverTable.add ( *piiu );
                bhe * pBHE = this->beaconTable.lookup ( addr.ia );
                if ( ! pBHE ) {
                    pBHE = new bhe ( epicsTime (), addr.ia );
                    if ( pBHE ) {
                        if ( this->beaconTable.add ( *pBHE ) < 0 ) {
                            pBHE->destroy ();
                            return true;
                        }
                    }
                    else {
                        return true;
                    }
                }
                pBHE->registerIIU ( *piiu );
            }
            catch ( ... ) {
                this->printf ( "CAC: Exception during virtual circuit creation\n" );
                return true;
            }
        }

        this->pudpiiu->detachChannel ( *chan );
        chan->searchReplySetUp ( *piiu, sid, typeCode, count );
        piiu->attachChannel ( *chan );

        chan->createChannelRequest ();
        piiu->flushRequest ();

        v41Ok = piiu->ca_v41_ok ();
        v42Ok = piiu->ca_v42_ok ();

        if ( ! v42Ok ) {
            // connect to old server with lock applied
            chan->connect ();
            // resubscribe for monitors from this channel 
            this->connectAllIO ( *chan );
        }

        if ( this->pSearchTmr ) {
            this->pSearchTmr->notifySearchResponse ( retrySeqNumber, currentTime );
        }
    }

    if ( ! v42Ok ) {
        // channel uninstal routine grabs the callback lock so
        // a channel will not be deleted while a call back is 
        // in progress
        //
        // the callback lock is also taken when a channel 
        // disconnects to prevent a race condition with the 
        // code below
        chan->connectStateNotify ();

        /*
         * if less than v4.1 then the server will never
         * send access rights and we know that there
         * will always be access and also need to call 
         * their call back here
         */
        if ( ! v41Ok ) {
            chan->accessRightsNotify ();
        }
    }

    return true;
}

void cac::uninstallChannel ( nciu & chan )
{
    //
    // dont block on the call back lock if this isnt the 
    // primary thread
    this->udpWakeup ();

    epicsAutoMutex autoMutex ( this->serializeCallbackMutexUsage );

    if ( this->pCallbackLocker ) {
        this->uninstallChannelPrivate ( chan );
    }
    else {
        // taking this mutex guarantees that we will not delete 
        // a channel out from under a callback
        epicsAutoMutex autoCallbackMutex ( this->callbackMutex );
        this->uninstallChannelPrivate ( chan );
    }
}

void cac::uninstallChannelPrivate ( nciu & chan )
{
    epicsAutoMutex autoMutex ( this->mutex );
    nciu * pChan = this->chanTable.remove ( chan );
    assert ( pChan = &chan );
    // flush prior to taking the callback lock
    this->flushIfRequired ( *chan.getPIIU() );
    chan.getPIIU()->clearChannelRequest ( chan );
    chan.getPIIU()->detachChannel ( chan );
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

// lock must be applied before calling this cac private routine
void cac::flushIfRequired ( netiiu & iiu )
{
    if ( iiu.flushBlockThreshold() ) {
        iiu.flushRequest ();
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
            if ( this->pCallbackLocker ) {
                iiu.blockUntilSendBacklogIsReasonable 
                    ( &this->callbackMutex, this->mutex );
            }
            else {
                iiu.blockUntilSendBacklogIsReasonable ( 0, this->mutex );
            }
        }
    }
    else {
        iiu.flushRequestIfAboveEarlyThreshold ();
    }
}

void cac::writeRequest ( nciu &chan, unsigned type, unsigned nElem, const void *pValue )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->flushIfRequired ( *chan.getPIIU() );
    chan.getPIIU()->writeRequest ( chan, type, nElem, pValue );
}

cacChannel::ioid cac::writeNotifyRequest ( nciu &chan, unsigned type, unsigned nElem, 
                                    const void *pValue, cacWriteNotify &notifyIn )
{
    epicsAutoMutex autoMutex ( this->mutex );
    autoPtrRecycle  < netWriteNotifyIO > pIO ( this->ioTable, chan.cacPrivateListOfIO::eventq,
        *this, netWriteNotifyIO::factory ( this->freeListWriteNotifyIO, chan, notifyIn ) );
    if ( pIO.get() ) {
        this->ioTable.add ( *pIO );
        chan.cacPrivateListOfIO::eventq.add ( *pIO );
        this->flushIfRequired ( *chan.getPIIU() );
        chan.getPIIU()->writeNotifyRequest ( 
            chan, *pIO, type, nElem, pValue );
        return pIO.release()->getId ();
    }
    else {
        throw std::bad_alloc ();
    }
}

cacChannel::ioid cac::readNotifyRequest ( nciu &chan, unsigned type, 
                                         unsigned nElem, cacReadNotify &notifyIn )
{
    epicsAutoMutex autoMutex ( this->mutex );
    autoPtrRecycle  < netReadNotifyIO > pIO ( this->ioTable, chan.cacPrivateListOfIO::eventq, *this,
        netReadNotifyIO::factory ( this->freeListReadNotifyIO, chan, notifyIn ) );
    if ( pIO.get() ) {
        this->ioTable.add ( *pIO );
        chan.cacPrivateListOfIO::eventq.add ( *pIO );
        this->flushIfRequired ( *chan.getPIIU() );
        chan.getPIIU()->readNotifyRequest ( chan, *pIO, type, nElem );
        return pIO.release()->getId ();
    }
    else {
        throw std::bad_alloc ();
    }
}

void cac::ioCancel ( nciu &chan, const cacChannel::ioid &id )
{   
    if ( ! epicsThreadPrivateGet ( caClientCallbackThreadId ) &&
        this->pCallbackLocker ) {
        this->ioCancelPrivate ( chan, id );
    }
    else {
        // wait for any IO callbacks in progress to complete
        // prior to destroying the IO object
        epicsAutoMutex autoMutex ( this->callbackMutex );
        this->ioCancelPrivate ( chan, id );
    }
}

void cac::ioCancelPrivate ( nciu &chan, const cacChannel::ioid &id )
{   
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( pmiu ) {
        chan.cacPrivateListOfIO::eventq.remove ( *pmiu );
        class netSubscription *pSubscr = pmiu->isSubscription ();
        if ( pSubscr ) {
            this->flushIfRequired ( *chan.getPIIU() );
            chan.getPIIU()->subscriptionCancelRequest ( chan, *pSubscr );
        }
        pmiu->destroy ( *this );
    }
}

void cac::ioShow ( const cacChannel::ioid &id, unsigned level ) const
{
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( pmiu ) {
        pmiu->show ( level );
    }
}

void cac::ioCompletionNotify ( unsigned id, unsigned type, 
                              arrayElementCount count, const void *pData )
{
    baseNMIU * pmiu;
    
    {
        epicsAutoMutex autoMutex ( this->mutex );
        pmiu = this->ioTable.lookup ( id );
        if ( ! pmiu ) {
            return;
        }
    }

    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    pmiu->completion ( type, count, pData );
}

void cac::ioExceptionNotify ( unsigned id, int status, const char *pContext )
{
    baseNMIU * pmiu;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        pmiu = this->ioTable.lookup ( id );
    }

    if ( ! pmiu ) {
        return;
    }

    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //

    pmiu->exception ( status, pContext );
}

void cac::ioExceptionNotify ( unsigned id, int status, 
                   const char *pContext, unsigned type, arrayElementCount count )
{
    baseNMIU * pmiu;
    
    {
        epicsAutoMutex autoMutex ( this->mutex );
        pmiu = this->ioTable.lookup ( id );
        if ( ! pmiu ) {
            return;
        }
    }

    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    pmiu->exception ( status, pContext, type, count );
}

void cac::ioCompletionNotifyAndDestroy ( unsigned id )
{
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return;
    }

    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );

    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    {
        epicsAutoMutexRelease autoMutexRelease ( this->mutex );
        pmiu->completion ();
    }

    pmiu->destroy ( *this );
}

void cac::ioCompletionNotifyAndDestroy ( unsigned id, 
    unsigned type, arrayElementCount count, const void *pData )
{
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return;
    }
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );

    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    {
        epicsAutoMutexRelease autoMutexRelease ( this->mutex );
        pmiu->completion ( type, count, pData );
    }

    pmiu->destroy ( *this );
}

void cac::ioExceptionNotifyAndDestroy ( unsigned id, int status, 
                                       const char *pContext )
{
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return;
    }
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );

    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //
    {
        epicsAutoMutexRelease autoMutexRelease ( this->mutex );
        pmiu->exception ( status, pContext );
    }

    pmiu->destroy ( *this );
}

void cac::ioExceptionNotifyAndDestroy ( unsigned id, int status, 
                        const char *pContext, unsigned type, arrayElementCount count )
{
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return;
    }
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );

    //
    // The IO destroy routines take the call back mutex 
    // when uninstalling and deleting the baseNMIU so there is 
    // no need to worry here about the baseNMIU being deleted while
    // it is in use here.
    //

    {
        epicsAutoMutexRelease autoMutexRelease ( this->mutex );
        pmiu->exception ( status, pContext, type, count );
    }

    pmiu->destroy ( *this );
}

// resubscribe for monitors from this channel
// (lock must be applied)
void cac::connectAllIO ( nciu & chan )
{
    tsDLIterBD < baseNMIU > pNetIO = 
        chan.cacPrivateListOfIO::eventq.firstIter ();
    while ( pNetIO.valid () ) {
        tsDLIterBD < baseNMIU > next = pNetIO;
        next++;
        class netSubscription *pSubscr = pNetIO->isSubscription ();
        // disconnected channels should have only subscription IO attached
        assert ( pSubscr );
        try {
            chan.getPIIU()->subscriptionRequest ( chan, *pSubscr );
        }
        catch ( ... ) {
            this->printf ( "cac: invalid subscription request ignored\n" );
        }
        pNetIO = next;
    }
    chan.getPIIU()->requestRecvProcessPostponedFlush ();
}

// cancel IO operations and monitor subscriptions
// (lock must be applied here)
void cac::disconnectAllIO ( nciu & chan, bool enableCallbacks )
{
    tsDLIterBD<baseNMIU> pNetIO = chan.cacPrivateListOfIO::eventq.firstIter();
    while ( pNetIO.valid() ) {
        tsDLIterBD<baseNMIU> pNext = pNetIO;
        pNext++;
        bool isSubscr = pNetIO->isSubscription() ? true : false;
        if ( ! isSubscr ) {
            // no use after disconnected - so uninstall it
            this->ioTable.remove ( *pNetIO );
            chan.cacPrivateListOfIO::eventq.remove ( *pNetIO );
        }
        if ( enableCallbacks ) {
            epicsAutoMutexRelease unlocker ( this->mutex );
            char buf[128];
            sprintf ( buf, "host = %100s", chan.pHostName() );
            // callbacks are locked at a higher level
            pNetIO->exception ( ECA_DISCONN, buf );
        }
        if ( ! isSubscr ) {
            pNetIO->destroy ( *this );
        }
        pNetIO = pNext;
    }
}

// this gets called when the user destroys a channel
void cac::destroyAllIO ( nciu & chan )
{
    if ( ! epicsThreadPrivateGet ( caClientCallbackThreadId ) &&
        this->pCallbackLocker ) {
        this->privateDestroyAllIO ( chan );
    }
    else {
        // force any callbacks in progress to complete
        // before deleting the IO
        epicsAutoMutex autoMutex ( this->callbackMutex );
        this->privateDestroyAllIO ( chan );
    }
}

void cac::privateDestroyAllIO ( nciu & chan )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->flushIfRequired ( *chan.getPIIU() );
    while ( baseNMIU *pIO = chan.cacPrivateListOfIO::eventq.get() ) {
        this->ioTable.remove ( *pIO );
        class netSubscription *pSubscr = pIO->isSubscription ();
        if ( pSubscr ) {
            chan.getPIIU()->subscriptionCancelRequest ( chan, *pSubscr );
        }
        {
            epicsAutoMutexRelease autoMutexRelease ( this->mutex );
            // If they call ioCancel() here it will be ignored
            // because the IO has been unregistered above
            pIO->exception ( ECA_CHANDESTROY, chan.pName() );
        }
        pIO->destroy ( *this );
    }        
}

void cac::recycleReadNotifyIO ( netReadNotifyIO &io )
{
    this->freeListReadNotifyIO.release ( &io, sizeof ( io ) );
}

void cac::recycleWriteNotifyIO ( netWriteNotifyIO &io )
{
    this->freeListWriteNotifyIO.release ( &io, sizeof ( io ) );
}

void cac::recycleSubscription ( netSubscription &io )
{
    this->freeListSubscription.release ( &io, sizeof ( io ) );
}

cacChannel::ioid cac::subscriptionRequest ( nciu &chan, unsigned type, 
    arrayElementCount nElem, unsigned mask, cacStateNotify &notifyIn )
{
    epicsAutoMutex autoMutex ( this->mutex );
    autoPtrRecycle  < netSubscription > pIO ( this->ioTable, chan.cacPrivateListOfIO::eventq, *this, 
        netSubscription::factory ( this->freeListSubscription, chan, type, nElem, mask, notifyIn ) );
    if ( pIO.get() ) {
        this->ioTable.add ( *pIO );
        chan.cacPrivateListOfIO::eventq.add ( *pIO );
        if ( chan.connected () ) {
            this->flushIfRequired ( *chan.getPIIU() );
            chan.getPIIU()->subscriptionRequest ( chan, *pIO );
        }
        cacChannel::ioid id = pIO->getId ();
        pIO.release ();
        return id;
    }
    else {
        throw std::bad_alloc();
    }
}

bool cac::noopAction ( tcpiiu &, const caHdrLargeArray &, void * /* pMsgBdy */ )
{
    return true;
}
 
bool cac::echoRespAction ( tcpiiu &, const caHdrLargeArray &, void * /* pMsgBdy */ )
{
    return true;
}

bool cac::writeNotifyRespAction ( tcpiiu &, const caHdrLargeArray &hdr, void * /* pMsgBdy */ )
{
    int caStatus = hdr.m_cid;
    if ( caStatus == ECA_NORMAL ) {
        this->ioCompletionNotifyAndDestroy ( hdr.m_available );
    }
    else {
        this->ioExceptionNotifyAndDestroy ( hdr.m_available, 
                    caStatus, "write notify request rejected" );
    }
    return true;
}

bool cac::readNotifyRespAction ( tcpiiu &iiu, const caHdrLargeArray &hdr, void *pMsgBdy )
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

    if ( caStatus == ECA_NORMAL ) {
        this->ioCompletionNotifyAndDestroy ( hdr.m_available,
            hdr.m_dataType, hdr.m_count, pMsgBdy );
    }
    else {
        this->ioExceptionNotifyAndDestroy ( hdr.m_available,
            caStatus, "read failed", hdr.m_dataType, hdr.m_count );
    }
    return true;
}

bool cac::eventRespAction ( tcpiiu &iiu, const caHdrLargeArray &hdr, void *pMsgBdy )
{   
    int caStatus;

    /*
     * m_postsize = 0 used to be a confirmation, but is
     * now a noop because the IO block is immediately
     * deleted
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
            caStatus = htonl ( ECA_BADTYPE );
        }
#   endif

    if ( caStatus == ECA_NORMAL ) {
        this->ioCompletionNotify ( hdr.m_available,
            hdr.m_dataType, hdr.m_count, pMsgBdy );
    }
    else {
        this->ioExceptionNotify ( hdr.m_available,
                caStatus, "subscription update failed", 
                hdr.m_dataType, hdr.m_count );
    }
    return true;
}

bool cac::readRespAction ( tcpiiu &, const caHdrLargeArray &hdr, void *pMsgBdy )
{
    this->ioCompletionNotifyAndDestroy ( hdr.m_available,
        hdr.m_dataType, hdr.m_count, pMsgBdy );
    return true;
}

bool cac::clearChannelRespAction ( tcpiiu &, const caHdrLargeArray &, void * /* pMsgBdy */ )
{
    return true; // currently a noop
}

bool cac::defaultExcep ( tcpiiu &iiu, const caHdrLargeArray &, 
                    const char *pCtx, unsigned status )
{
    char buf[512];
    char hostName[64];
    iiu.hostName ( hostName, sizeof ( hostName ) );
    sprintf ( buf, "host=%64s ctx=%400s", hostName, pCtx );
    this->notify.exception ( status, buf, 0, 0u );
    return true;
}

bool cac::eventAddExcep ( tcpiiu & /* iiu */, const caHdrLargeArray &hdr, 
                         const char *pCtx, unsigned status )
{
    this->ioExceptionNotify ( hdr.m_available, status, pCtx, 
        hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::readExcep ( tcpiiu &, const caHdrLargeArray &hdr, 
                     const char *pCtx, unsigned status )
{
    this->ioExceptionNotifyAndDestroy ( hdr.m_available, 
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::writeExcep ( tcpiiu &, const caHdrLargeArray &hdr, 
                      const char *pCtx, unsigned status )
{
    nciu * pChan = this->chanTable.lookup ( hdr.m_available );
    if ( pChan ) {
        pChan->writeException ( status, pCtx, 
            hdr.m_dataType, hdr.m_count );
    }
    return true;
}

bool cac::readNotifyExcep ( tcpiiu &, const caHdrLargeArray &hdr, 
                           const char *pCtx, unsigned status )
{
    this->ioExceptionNotifyAndDestroy ( hdr.m_available, 
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::writeNotifyExcep ( tcpiiu &, const caHdrLargeArray &hdr, 
                            const char *pCtx, unsigned status )
{
    this->ioExceptionNotifyAndDestroy ( hdr.m_available, 
        status, pCtx, hdr.m_dataType, hdr.m_count );
    return true;
}

bool cac::exceptionRespAction ( tcpiiu &iiu, const caHdrLargeArray &hdr, void *pMsgBdy )
{
    const caHdr * pReq = reinterpret_cast < const caHdr * > ( pMsgBdy );
    unsigned bytesSoFar = sizeof ( *pReq );
    if ( hdr.m_postsize < bytesSoFar ) {
        return false;
    }
    caHdrLargeArray req;
    req.m_cmmd = ntohs ( pReq->m_cmmd );
    req.m_postsize = ntohs ( pReq->m_postsize );
    req.m_dataType = ntohs ( pReq->m_dataType );
    req.m_count = ntohs ( pReq->m_count );
    req.m_cid = ntohl ( pReq->m_cid );
    req.m_available = ntohl ( pReq->m_available );
    const ca_uint32_t * pLW = reinterpret_cast < const ca_uint32_t * > ( pReq + 1 );
    if ( req.m_postsize == 0xffff ) {
        static const unsigned annexSize = 
            sizeof ( req.m_postsize ) + sizeof ( req.m_count );
        bytesSoFar += annexSize;
        if ( hdr.m_postsize < bytesSoFar ) {
            return false;
        }
        req.m_postsize = ntohl ( pLW[0] );
        req.m_count = ntohl ( pLW[1] );
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
    return ( this->*pStub ) ( iiu, req, pCtx, hdr.m_available );
}

bool cac::accessRightsRespAction ( tcpiiu &, const caHdrLargeArray &hdr, void * /* pMsgBdy */ )
{
    nciu * pChan;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        pChan = this->chanTable.lookup ( hdr.m_cid );
        if ( pChan ) {
            unsigned ar = hdr.m_available;
            caAccessRights accessRights ( 
                ( ar & CA_PROTO_ACCESS_RIGHT_READ ) ? true : false, 
                ( ar & CA_PROTO_ACCESS_RIGHT_WRITE ) ? true : false); 
            pChan->accessRightsStateChange ( accessRights );
        }
    }

    //
    // the channel delete routine takes the call back lock so
    // that this will not be called when the channel is being
    // deleted.
    //       
    if ( pChan ) {
        pChan->accessRightsNotify ();
    }

    return true;
}

bool cac::claimCIURespAction ( tcpiiu &iiu, 
                              const caHdrLargeArray &hdr, void * /*pMsgBdy */ )
{
    nciu * pChan;

    {
        epicsAutoMutex autoMutex ( this->mutex );
        pChan = this->chanTable.lookup ( hdr.m_cid );
        if ( pChan ) {
            unsigned sidTmp;
            if ( iiu.ca_v44_ok() ) {
                sidTmp = hdr.m_available;
            }
            else {
                sidTmp = pChan->getSID ();
            }
            pChan->connect ( hdr.m_dataType, hdr.m_count, sidTmp, iiu.ca_v41_ok() );
            this->connectAllIO ( *pChan );
        }
    }
    // the callback lock is taken when a channel is unistalled or when
    // is disconnected to prevent race conditions here
    if ( pChan ) {
        pChan->connectStateNotify ();
    }
    return true; 
}

bool cac::verifyAndDisconnectChan ( tcpiiu & /* iiu */, 
              const caHdrLargeArray & hdr, void * /* pMsgBdy */ )
{
    nciu * pChan;

    {
        epicsAutoMutex autoMutex ( this->mutex );
        pChan = this->chanTable.lookup ( hdr.m_cid );
        if ( ! pChan ) {
            return true;
        }
        this->disconnectAllIO ( *pChan, true );
        this->pSearchTmr->resetPeriod ( 0.0 );
        pChan->disconnect ( *this->pudpiiu );
        this->pudpiiu->attachChannel ( *pChan );
    }

    pChan->connectStateNotify ();
    pChan->accessRightsNotify ();

    return true;
}

bool cac::badTCPRespAction ( tcpiiu & iiu, 
                            const caHdrLargeArray & hdr, void * /* pMsgBdy */ )
{
    char hostName[64];
    iiu.hostName ( hostName, sizeof ( hostName ) );
    this->printf ( "CAC: Undecipherable TCP message ( bad response type %u ) from %s\n", 
        hdr.m_cmmd, hostName );
    return false;
}

bool cac::executeResponse ( tcpiiu &iiu, caHdrLargeArray &hdr, char *pMshBody )
{
    // execute the response message
    pProtoStubTCP pStub;
    if ( hdr.m_cmmd >= NELEMENTS ( cac::tcpJumpTableCAC ) ) {
        pStub = &cac::badTCPRespAction;
    }
    else {
        pStub = cac::tcpJumpTableCAC [hdr.m_cmmd];
    }
    return ( this->*pStub ) ( iiu, hdr, pMshBody );
}

void cac::signal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, ... )
{
    va_list theArgs;
    va_start ( theArgs, pFormat );  
    this->vSignal ( ca_status, pfilenm, lineno, pFormat, theArgs);
    va_end ( theArgs );
}

void cac::vSignal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, va_list args )
{
    static const char *severity[] = 
    {
        "Warning",
        "Success",
        "Error",
        "Info",
        "Fatal",
        "Fatal",
        "Fatal",
        "Fatal"
    };
    
    this->printf ( "CA.Client.Exception...............................................\n" );
    
    this->printf ( "    %s: \"%s\"\n", 
        severity[ CA_EXTRACT_SEVERITY ( ca_status ) ], 
        ca_message ( ca_status ) );

    if  ( pFormat ) {
        this->printf ( "    Context: \"" );
        this->vPrintf ( pFormat, args );
        this->printf ( "\"\n" );
    }
        
    if ( pfilenm ) {
        this->printf ( "    Source File: %s line %d\n",
            pfilenm, lineno );    
    } 

    epicsTime current = epicsTime::getCurrent ();
    char date[64];
    current.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S.%f");
    this->printf ( "    Current Time: %s\n", date );
    
    /*
     *  Terminate execution if unsuccessful
     */
    if( ! ( ca_status & CA_M_SUCCESS ) && 
        CA_EXTRACT_SEVERITY ( ca_status ) != CA_K_WARNING ){
        errlogFlush();
        abort();
    }
    
    this->printf ( "..................................................................\n" );
}

void cac::incrementOutstandingIO ()
{
    epicsAutoMutex locker ( this->mutex );
    if ( this->pndRecvCnt < UINT_MAX ) {
        this->pndRecvCnt++;
    }
    else {
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
    }
}

void cac::decrementOutstandingIO ()
{
    bool signalNeeded;

    {
        epicsAutoMutex locker ( this->mutex ); 
        if ( this->pndRecvCnt > 0u ) {
            this->pndRecvCnt--;
            if ( this->pndRecvCnt == 0u ) {
                signalNeeded = true;
            }
            else {
                signalNeeded = false;
            }
        }
        else {
            signalNeeded = true;
        }
    }

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

void cac::decrementOutstandingIO ( unsigned sequenceNo )
{
    bool signalNeeded;

    {
        epicsAutoMutex locker ( this->mutex );
        if ( this->readSeq == sequenceNo ) {
            if ( this->pndRecvCnt > 0u ) {
                this->pndRecvCnt--;
                if ( this->pndRecvCnt == 0u ) {
                    signalNeeded = true;
                }
                else {
                    signalNeeded = false;
                }
            }
            else {
                signalNeeded = true;
            }
        }
        else {
            signalNeeded = false;
        }
    }

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

void cac::selfTest () const
{
    this->chanTable.verify ();
    this->ioTable.verify ();
    this->sgTable.verify ();
    this->beaconTable.verify ();
}

void cac::notifyNewFD ( SOCKET sock ) const
{
    if ( this->pCallbackLocker ) {
        this->notify.fdWasCreated ( sock );
    }
}

void cac::notifyDestroyFD ( SOCKET sock ) const
{
    if ( this->pCallbackLocker ) {
        this->notify.fdWasDestroyed ( sock );
    }
}

void cac::uninstallIIU ( tcpiiu & iiu ) 
{
    epicsAutoMutex autoMutex ( this->mutex );
    if ( iiu.channelCount () ) {
        char hostNameTmp[64];
        iiu.hostName ( hostNameTmp, sizeof ( hostNameTmp ) );
        genLocalExcep ( *this, ECA_DISCONN, hostNameTmp );
    }
    osiSockAddr addr = iiu.getNetworkAddress ();
    if ( addr.sa.sa_family == AF_INET ) {
        inetAddrID tmp ( addr.ia );
        bhe *pBHE = this->beaconTable.lookup ( tmp );
        if ( pBHE ) {
            pBHE->unregisterIIU ( iiu);
        }
    }
    assert ( this->pudpiiu );
    tsDLList < nciu > tmpList;
    iiu.uninstallAllChan ( tmpList );
    while ( nciu *pChan = tmpList.get () ) {
        this->disconnectAllIO ( *pChan, true );
        pChan->disconnect ( *this->pudpiiu );
        this->pudpiiu->attachChannel ( *pChan );
        {
            epicsAutoMutexRelease autoMutexRelease ( this->mutex );
            pChan->connectStateNotify ();
            pChan->accessRightsNotify ();
        }
    }
    this->serverTable.remove ( iiu );
    this->pSearchTmr->resetPeriod ( 0.0 );
    // signal iiu uninstal event so that cac can properly shut down
    this->iiuUninstal.signal();
}

void cac::preemptiveCallbackLock ()
{
    // the count must be incremented prior to taking the lock
    {
        epicsAutoMutex autoMutex ( this->mutex );
        assert ( this->recvThreadsPendingCount < UINT_MAX );
        this->recvThreadsPendingCount++;
    }
    this->callbackMutex.lock ();
}

void cac::preemptiveCallbackUnlock ()
{
    this->callbackMutex.unlock ();
    bool signalRequired;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        assert ( this->recvThreadsPendingCount > 0 );
        this->recvThreadsPendingCount--;
        if ( this->pCallbackLocker ) {
            if ( this->recvThreadsPendingCount == 1u ) {
                signalRequired = true;
            }
            else {
                signalRequired = false;
            }
        }
        else {
            signalRequired = false;
        }
    }
    if ( signalRequired ) {
        this->noRecvThreadsPending.signal ();
    }
}

double cac::beaconPeriod ( const nciu & chan ) const
{
    epicsAutoMutex locker ( this->mutex );
    const netiiu * pIIU = chan.getConstPIIU ();
    if ( pIIU ) {
        osiSockAddr addr = pIIU->getNetworkAddress ();
        if ( addr.sa.sa_family == AF_INET ) {
            inetAddrID tmp ( addr.ia );
            bhe *pBHE = this->beaconTable.lookup ( tmp );
            if ( pBHE ) {
                return pBHE->period ();
            }
        }
    }
    return - DBL_MAX;
}

void cac::udpWakeup ()
{
    epicsAutoMutex locker ( this->mutex );
    if ( this->pudpiiu ) {
        this->pudpiiu->wakeupMsg ();
    }
}
