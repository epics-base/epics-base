
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

#include "epicsMemory.h"
#include "osiProcess.h"
#include "osiSigPipeIgnore.h"
#include "envDefs.h"

#include "iocinf.h"
#include "cac.h"
#include "inetAddrID.h"
#include "virtualCircuit.h"
#include "netIO.h"
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
    &cac::defaultExcep,     // CA_PROTO_NOOP
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

//
// cac::cac ()
//
cac::cac ( cacNotify &notifyIn, bool enablePreemptiveCallbackIn ) :
    ipToAEngine ( "caIPAddrToAsciiEngine" ), 
    pudpiiu ( 0 ),
    pSearchTmr ( 0 ),
    pRepeaterSubscribeTmr ( 0 ),
    tcpSmallRecvBufFreeList ( 0 ),
    tcpLargeRecvBufFreeList ( 0 ),
    pRecvProcessThread ( 0 ),
    notify ( notifyIn ),
    ioNotifyInProgressId ( 0 ),
    initializingThreadsPriority ( epicsThreadGetPrioritySelf () ),
    threadsBlockingOnNotifyCompletion ( 0u ),
    maxRecvBytesTCP ( MAX_TCP ),
    recvProcessEnableRefCount ( 0u ),
    pndRecvCnt ( 0u ), 
    readSeq ( 0u ),
    enablePreemptiveCallback ( enablePreemptiveCallbackIn ),
    ioInProgress ( false ),
    recvProcessThreadExitRequest ( false ),
    recvProcessPending ( false )
{
	long status;
    unsigned abovePriority;

	if ( ! osiSockAttach () ) {
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
	}

    {
        epicsThreadBooleanStatus tbs;

        tbs = epicsThreadLowestPriorityLevelAbove ( this->initializingThreadsPriority, &abovePriority);
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
        this->pUserName = new char [len];
        if ( ! this->pUserName ) {
            throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
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
        free ( this->pUserName );
        throwWithLocation ( caErrorCode ( ECA_ALLOCMEM ) );
    }

    freeListInitPvt ( &this->tcpLargeRecvBufFreeList, this->maxRecvBytesTCP, 1 );
    if ( ! this->tcpLargeRecvBufFreeList ) {
        free ( this->pUserName );
        freeListCleanup ( this->tcpSmallRecvBufFreeList );
        throwWithLocation ( caErrorCode ( ECA_ALLOCMEM ) );
    }

    this->pTimerQueue = & epicsTimerQueueActive::allocate ( false, abovePriority );
    if ( ! this->pTimerQueue ) {
        free ( this->pUserName );
        freeListCleanup ( this->tcpSmallRecvBufFreeList );
        freeListCleanup ( this->tcpLargeRecvBufFreeList );
        throwWithLocation ( caErrorCode ( ECA_ALLOCMEM ) );
    }
    this->preemptiveCallbackLock.lock ();
}

cac::~cac ()
{
    //
    // make certain that process thread isnt deleting 
    // tcpiiu objects at the same that this thread is
    //
    if ( this->pRecvProcessThread ) {
        this->recvProcessThreadExitRequest = true;
        this->recvProcessActivityEvent.signal ();
        this->enableCallbackPreemption ();
        this->recvProcessThreadExit.wait ();
        delete this->pRecvProcessThread;
    }

    {
        epicsAutoMutex autoMutex ( this->mutex );
        if ( this->pudpiiu ) {
            // this blocks until the UDP thread exits so that
            // it will not sneak in any new clients
            this->pudpiiu->shutdown ();
        }
    }

    //
    // shutdown all tcp connections and wait for threads to exit
    //
    while ( true ) {
        tcpiiu * piiu;
        {
            epicsAutoMutex autoMutex ( this->mutex );
            if ( ( piiu = this->iiuList.get() ) ) {
                piiu->disconnectAllChan ( limboIIU );
            }
        }
        if ( ! piiu ) {
            break;
        }
        piiu->destroy ();
    }

    delete this->pRepeaterSubscribeTmr;
    delete this->pSearchTmr;

    freeListCleanup ( this->tcpSmallRecvBufFreeList );
    freeListCleanup ( this->tcpLargeRecvBufFreeList );

    {
        epicsAutoMutex autoMutex ( this->mutex );
        if ( this->pudpiiu ) {

            //
            // make certain that the UDP thread isnt starting 
            // up  new clients. 
            //
            this->pudpiiu->disconnectAllChan ( limboIIU );
        }
    }

    if ( ! this->enablePreemptiveCallback && this->pudpiiu ) {
        this->notify.fdWasDestroyed ( this->pudpiiu->getSock() );
    }
    delete this->pudpiiu;
    delete this->pUserName;

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

// lock must be applied
void cac::processRecvBacklog ()
{
    tsDLList < tcpiiu > deadIIU;
    {
        tsDLIterBD < tcpiiu > piiu = this->iiuList.firstIter ();
        while ( piiu.valid () ) {
            tsDLIterBD < tcpiiu > pNext = piiu;
            pNext++;
            if ( ! piiu->alive () ) {
                if ( piiu->channelCount () ) {
                    char hostNameTmp[64];
                    piiu->hostName ( hostNameTmp, sizeof ( hostNameTmp ) );
                    genLocalExcep ( *this, ECA_DISCONN, hostNameTmp );
                }
                piiu->getBHE().unbindFromIIU ();
                assert ( this->pudpiiu );
                piiu->disconnectAllChan ( *this->pudpiiu );
                this->iiuList.remove ( *piiu );
                deadIIU.add ( *piiu ); // postpone destroy and avoid deadlock
            }
            else {
                // make certain that:
                // 1) this is called from the appropriate thread
                // 2) lock is not held while in call back
                if ( piiu->trueOnceOnly() && ! this->enablePreemptiveCallback ) {
                    epicsAutoMutexRelease autoRelease ( this->mutex );
                    this->notify.fdWasCreated ( piiu->getSock() );
                }
                piiu->processIncoming ();
            }
            piiu = pNext;
	    }
    }
    if ( deadIIU.count() ) {
        {
            epicsAutoMutexRelease autoRelease ( this->mutex );
            while ( tcpiiu *piiu = deadIIU.get() ) {
                // make certain that:
                // 1) this is called from the appropriate thread
                // 2) lock is not held while in call back
                if ( ! this->enablePreemptiveCallback ) {
                    this->notify.fdWasDestroyed ( piiu->getSock() );
                }
                piiu->destroy ();
            }
        }
        assert ( this->pSearchTmr );
        this->pSearchTmr->resetPeriod ( 0.0 );
    }
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
    tsDLIterBD <tcpiiu> piiu = this->iiuList.firstIter ();
    while ( piiu.valid () ) {
        piiu->flushRequest ();
        piiu++;
    }
}

unsigned cac::connectionCount () const
{
    epicsAutoMutex autoMutex ( this->mutex );
    return this->iiuList.count ();
}

void cac::show ( unsigned level ) const
{
    epicsAutoMutex autoMutex2 ( this->mutex );

    ::printf ( "Channel Access Client Context at %p for user %s\n", 
        static_cast <const void *> ( this ), this->pUserName );
    if ( level > 0u ) {
        tsDLIterConstBD < tcpiiu > piiu = this->iiuList.firstIter ();
        while ( piiu.valid() ) {
            piiu->show ( level - 1u );
            piiu++;
	    }

        ::printf ( "\tconnection time out watchdog period %f\n", this->connTMO );
        ::printf ( "\tpreemptive calback is %s\n",
            this->enablePreemptiveCallback ? "enabled" : "disabled" );
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
        ::printf ( "recv process enable count %u\n", 
            this->recvProcessEnableRefCount );
        ::printf ( "\trecv process shutdown command boolean %u\n", 
            this->recvProcessThreadExitRequest );
        ::printf ( "\tthe current read sequence number is %u\n",
                this->readSeq );
    }

    if ( level > 3u ) {
        ::printf ( "Default mutex:\n");
        this->mutex.show ( level - 4u );
        ::printf ( "Receive process activity event:\n" );
        this->recvProcessActivityEvent.show ( level - 3u );
        ::printf ( "Receive process exit event:\n" );
        this->recvProcessThreadExit.show ( level - 3u );
        ::printf ( "IO done event:\n");
        this->ioDone.show ( level - 3u );
        ::printf ( "mutex:\n" );
        this->mutex.show ( level - 3u );
    }
}

/*
 *  cac::beaconNotify
 */
void cac::beaconNotify ( const inetAddrID &addr, const epicsTime &currentTime )
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

int cac::pendIO ( const double &timeout )
{
    // prevent recursion nightmares by disabling calls to 
    // pendIO () from within a CA callback
    if ( this->recvProcessThreadIsCurrentThread () ) {
        return ECA_EVDISALLOW;
    }

    int status = ECA_NORMAL;
    epicsTime beg_time = epicsTime::getCurrent ();
    double remaining;
    if ( timeout == 0.0 ) {
        remaining = 60.0;
    }
    else{
        remaining = timeout;
    }    

    {
        epicsAutoMutex autoMutex ( this->mutex );

        this->flushRequestPrivate ();
   
        while ( this->pndRecvCnt > 0 ) {
            if ( remaining < CAC_SIGNIFICANT_DELAY ) {
                status = ECA_TIMEOUT;
                break;
            }
            this->enableCallbackPreemption ();
            {
                epicsAutoMutexRelease autoRelease ( this->mutex );
                this->ioDone.wait ( remaining );
            }
            this->disableCallbackPreemption ();
            if ( timeout != 0.0 ) {
                double delay = epicsTime::getCurrent () - beg_time;
                remaining = timeout - delay;
            }    
        }

        this->readSeq++;
        this->pndRecvCnt = 0u;
        if ( this->pudpiiu ) {
            this->pudpiiu->connectTimeoutNotify ();
        }
    }

    return status;
}

void cac::blockForEventAndEnableCallbacks ( epicsEvent &event, double timeout )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->enableCallbackPreemption ();
    {
        epicsAutoMutexRelease autoMutexRelease ( this->mutex );
        event.wait ( timeout );
    }
    this->disableCallbackPreemption ();
}

int cac::pendEvent ( const double &timeout )
{
    // prevent recursion nightmares by disabling calls to 
    // pendIO () from within a CA callback
    if ( this->recvProcessThreadIsCurrentThread () ) {
        return ECA_EVDISALLOW;
    }

    {
        epicsAutoMutex autoMutex ( this->mutex );

        this->flushRequestPrivate ();

        this->enableCallbackPreemption ();

        {
            epicsAutoMutexRelease autoMutexRelease ( this->mutex );
            if ( timeout == 0.0 ) {
                while ( true ) {
                    epicsThreadSleep ( 60.0 );
                }
            }
            else if ( timeout >= CAC_SIGNIFICANT_DELAY ) {
                epicsThreadSleep ( timeout );
            }
            while ( this->recvProcessPending ) {
                // give up the processor while 
                // there is recv processing to be done
                epicsThreadSleep ( 0.1 );
            }
        }

        this->disableCallbackPreemption ();
    }

    return ECA_TIMEOUT;
}

// this is to only be used by early protocol revisions
bool cac::connectChannel ( unsigned id )
{
    epicsAutoMutex autoMutex ( this->mutex );
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        pChan->connect ();
        return true;
    }
    else {
        return false;
    }
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

void cac::startRecvProcessThread ()
{
    bool newThread = false;
    {
        epicsAutoMutex epicsMutex ( this->mutex );
        if ( ! this->pRecvProcessThread ) {
            unsigned priorityOfProcess;
            epicsThreadBooleanStatus tbs  = epicsThreadLowestPriorityLevelAbove ( 
                this->getInitializingThreadsPriority (), &priorityOfProcess );
            if ( tbs != epicsThreadBooleanStatusSuccess ) {
                priorityOfProcess = this->getInitializingThreadsPriority ();
            }

            this->pRecvProcessThread = new epicsThread ( *this, "CAC-recv-process",
                epicsThreadGetStackSize ( epicsThreadStackSmall ), priorityOfProcess );
            if ( ! this->pRecvProcessThread ) {
                throw std::bad_alloc ();
            }
            this->pRecvProcessThread->start ();
            newThread = true;
        }
    }
    if ( this->enablePreemptiveCallback && newThread ) {
        this->enableCallbackPreemption ();
    }
}

// this is the recv process thread entry point
void cac::run ()
{
    this->attachToClientCtx ();
    while ( ! this->recvProcessThreadExitRequest ) {
        {
            this->recvProcessPending = true;
            epicsAutoMutex autoMutexPCB ( this->preemptiveCallbackLock );
            epicsAutoMutex autoMutex ( this->mutex );
            this->processRecvBacklog ();
            this->recvProcessPending = false;
        }
        this->recvProcessActivityEvent.wait ();
    }
    this->recvProcessThreadExit.signal ();
}

cacChannel & cac::createChannel ( const char *pName, cacChannelNotify &chan )
{
    cacChannel *pIO;

    pIO = this->services.createChannel ( pName, chan );
    if ( ! pIO ) {
        pIO = cacGlobalServiceList.createChannel ( pName, chan );
        if ( ! pIO ) {
            if ( ! this->pudpiiu || ! this->pSearchTmr ) {
                if ( ! this->setupUDP () ) {
                    throw ECA_INTERNAL;
                }
            }
            epics_auto_ptr < cacChannel > pNetChan 
                ( new nciu ( *this, limboIIU, chan, pName ) );
            if ( pNetChan.get() ) {
                this->startRecvProcessThread ();
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
        if ( ! this->enablePreemptiveCallback ) {
            epicsAutoMutexRelease autoRelease ( this->mutex );
            this->notify.fdWasCreated ( this->pudpiiu->getSock() );
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

// lock must already be applied
void cac::enableCallbackPreemption ()
{
    assert ( this->recvProcessEnableRefCount < UINT_MAX );
    this->recvProcessEnableRefCount++;
    if ( this->recvProcessEnableRefCount == 1u ) {
        this->preemptiveCallbackLock.unlock ();
    }
}

// lock must already be applied
void cac::disableCallbackPreemption ()
{
    if ( this->recvProcessEnableRefCount == 1u ) {
        if ( ! this->preemptiveCallbackLock.tryLock () ) {
            {
                epicsAutoMutexRelease autoMutexRelease ( this->mutex );
                this->preemptiveCallbackLock.lock ();
            }
            // in case some thread enabled it while this->mutex was unlocked
            if ( this->recvProcessEnableRefCount > 1u ) {
                this->preemptiveCallbackLock.unlock ();
            }
        }
    }
    assert ( this->recvProcessEnableRefCount > 0u );
    this->recvProcessEnableRefCount--;
}

void cac::repeaterSubscribeConfirmNotify ()
{
    if ( this->pRepeaterSubscribeTmr ) {
        this->pRepeaterSubscribeTmr->confirmNotify ();
    }
}

bool cac::lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             ca_uint16_t typeCode, arrayElementCount count, 
             unsigned minorVersionNumber, const osiSockAddr &addr,
             const epicsTime & currentTime )
{
    unsigned  retrySeqNumber;

    if ( addr.sa.sa_family != AF_INET ) {
        return false;
    }

    epicsAutoMutex autoMutex ( this->mutex );
    nciu *chan;

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
    tcpiiu *piiu;
    bhe *pBHE = this->beaconTable.lookup ( addr.ia );
    if ( pBHE ) {
        piiu = pBHE->getIIU ();
        if ( piiu ) {
            if ( ! piiu->alive () ) {
                return true;
            }
        }
    }
    else {
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
        piiu = 0;
    }

    if ( ! piiu ) {
        try {
            piiu = new tcpiiu ( *this, this->connTMO, *this->pTimerQueue,
                        addr, minorVersionNumber, *pBHE, this->ipToAEngine );
            if ( ! piiu ) {
                return true;
            }
            this->iiuList.add ( *piiu  );

        }
        catch ( ... ) {
            this->printf ( "CAC: Exception caught during virtual circuit creation\n" );
            return true;
        }
    }

    this->pudpiiu->detachChannel ( *chan );
    chan->searchReplySetUp ( *piiu, sid, typeCode, count );
    piiu->attachChannel ( *chan );

    chan->createChannelRequest ();
    piiu->flushRequest ();

    if ( ! piiu->ca_v42_ok () ) {
        chan->connect ();
    }

    if ( this->pSearchTmr ) {
        this->pSearchTmr->notifySearchResponse ( retrySeqNumber, currentTime );
    }

    return true;
}

void cac::uninstallChannel ( nciu & chan )
{
    epicsAutoMutex autoMutex ( this->mutex );
    nciu *pChan = this->chanTable.remove ( chan );
    assert ( pChan = &chan );
    this->flushIfRequired ( chan );
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
void cac::flushIfRequired ( nciu &chan )
{
    if ( chan.getPIIU()->flushBlockThreshold() ) {
        // the process thread is not permitted to flush as this
        // can result in a push / pull deadlock on the TCP pipe.
        // Instead, the process thread scheduals the flush with the 
        // send thread which runs at a higher priority than the 
        // send thread. The same applies to the UDP thread for
        // locking hierarchy reasons.
        bool blockPermit = true;
        if ( this->recvProcessThreadIsCurrentThread () ) {
            blockPermit = false;
        }
        if ( this->pudpiiu && blockPermit ) {
            if ( this->pudpiiu->isCurrentThread () ) {
                blockPermit = false;
            }
        }
        this->flushRequestPrivate ();
        if ( blockPermit ) {
            // enable / disable of call back preemption must occur here
            // because the tcpiiu might disconnect while waiting and its
            // pointer to this cac might become invalid
            this->enableCallbackPreemption ();
            chan.getPIIU()->blockUntilSendBacklogIsReasonable ( this->mutex );
            this->disableCallbackPreemption ();
        }
    }
    else {
        chan.getPIIU()->flushRequestIfAboveEarlyThreshold ();
    }
}

void cac::writeRequest ( nciu &chan, unsigned type, unsigned nElem, const void *pValue )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->flushIfRequired ( chan );
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
        this->flushIfRequired ( chan );
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
        this->flushIfRequired ( chan );
        chan.getPIIU()->readNotifyRequest ( chan, *pIO, type, nElem );
        return pIO.release()->getId ();
    }
    else {
        throw std::bad_alloc ();
    }
}

// lock must be applied
bool cac::blockForIOCallbackCompletion ( const cacChannel::ioid & id )
{
    while ( this->ioInProgress && this->ioNotifyInProgressId == id ) {
        assert ( this->threadsBlockingOnNotifyCompletion < UINT_MAX );
        this->threadsBlockingOnNotifyCompletion++;
        epicsAutoMutexRelease autoRelease ( this->mutex );
        this->notifyCompletionEvent.wait ( 0.5 );
        assert ( this->threadsBlockingOnNotifyCompletion > 0u );
        this->threadsBlockingOnNotifyCompletion--;
    }
    return this->threadsBlockingOnNotifyCompletion > 0u;
}

void cac::ioCancel ( nciu &chan, const cacChannel::ioid &id )
{   
    bool signalNeeded = false;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        baseNMIU * pmiu = this->ioTable.remove ( id );
        if ( pmiu ) {
            chan.cacPrivateListOfIO::eventq.remove ( *pmiu );
            class netSubscription *pSubscr = pmiu->isSubscription ();
            if ( pSubscr ) {
                chan.getPIIU()->subscriptionCancelRequest ( chan, *pSubscr );
            }
            if ( pRecvProcessThread->isCurrentThread() ) {
                signalNeeded = false;
            }
            else {
                signalNeeded = this->blockForIOCallbackCompletion ( id );
            }
            pmiu->destroy ( *this );
        }
    }
    if ( signalNeeded ) {
        this->notifyCompletionEvent.signal ();
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
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( ! pmiu ) {
        return;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    {
        epicsAutoMutexRelease autoRelease ( this->mutex );
        pmiu->completion ( type, count, pData );
    }
    // threads blocked canceling this IO will wait
    // until we stop processing it
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
}

void cac::ioExceptionNotify ( unsigned id, int status, const char *pContext )
{
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( ! pmiu ) {
        return;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    {
        epicsAutoMutexRelease autoRelease ( this->mutex );
        pmiu->exception ( status, pContext );
    }
    // threads blocked canceling this IO will wait
    // until we stop processing it
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
}

void cac::ioExceptionNotify ( unsigned id, int status, 
                   const char *pContext, unsigned type, arrayElementCount count )
{
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( ! pmiu ) {
        return;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    {
        epicsAutoMutexRelease autoRelease ( this->mutex );
        pmiu->exception ( status, pContext, type, count );
    }
    // threads blocked canceling this IO will wait
    // until we stop processing it
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
}

void cac::ioCompletionNotifyAndDestroy ( unsigned id )
{
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return;
    }
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    {
        epicsAutoMutexRelease autoRelease ( this->mutex );
        pmiu->completion ();
    }
    pmiu->destroy ( *this );
    // threads blocked canceling this IO will wait
    // until we stop processing it
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
}

void cac::ioCompletionNotifyAndDestroy ( unsigned id, 
    unsigned type, arrayElementCount count, const void *pData )
{
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );
    {
        epicsAutoMutexRelease autoRelease ( this->mutex );
        pmiu->completion ( type, count, pData );
    }
    pmiu->destroy ( *this );
    // threads blocked canceling this IO will wait
    // until we stop processing it
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
}

void cac::ioExceptionNotifyAndDestroy ( unsigned id, int status, 
                                       const char *pContext )
{
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );
    {
        epicsAutoMutexRelease autoRelease ( this->mutex );
        pmiu->exception ( status, pContext );
    }
    pmiu->destroy ( *this );
    // threads blocked canceling this IO will wait
    // until we stop processing it
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
}

void cac::ioExceptionNotifyAndDestroy ( unsigned id, int status, 
                        const char *pContext, unsigned type, arrayElementCount count )
{
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );
    {
        epicsAutoMutexRelease autoRelease ( this->mutex );
        pmiu->exception ( status, pContext, type, count );
    }
    pmiu->destroy ( *this );
    // threads blocked canceling this IO will wait
    // until we stop processing it
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
}

// resubscribe for monitors from this channel
void cac::connectAllIO ( nciu &chan )
{
    bool signalNeeded = false;
    {
        tsDLList < baseNMIU > tmpList;
        epicsAutoMutex autoMutex ( this->mutex );
        tsDLIterBD < baseNMIU > pNetIO = 
            chan.cacPrivateListOfIO::eventq.firstIter ();
        while ( pNetIO.valid () ) {
            tsDLIterBD < baseNMIU > next = pNetIO;
            next++;
            class netSubscription *pSubscr = pNetIO->isSubscription ();
            if ( pSubscr ) {
                try {
                    chan.getPIIU()->subscriptionRequest ( chan, *pSubscr );
                }
                catch ( ... ) {
                    this->printf ( "cac: invalid subscription request ignored\n" );
                }
            }
            else {
                // it shouldnt be here at this point - so uninstall it
                this->ioTable.remove ( *pNetIO );
                chan.cacPrivateListOfIO::eventq.remove ( *pNetIO );
                tmpList.add ( *pNetIO );
            }
            pNetIO = next;
        }
        chan.getPIIU()->requestRecvProcessPostponedFlush ();
        while ( baseNMIU *pIO = tmpList.get () ) {
            signalNeeded = this->blockForIOCallbackCompletion ( pIO->getID() );
            pIO->destroy ( *this );
        }
    }
    if ( signalNeeded ) {
        this->notifyCompletionEvent.signal ();
    }
}

// cancel IO operations and monitor subscriptions
void cac::disconnectAllIO ( nciu &chan )
{
    bool signalNeeded = false;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        tsDLList < baseNMIU > tmpList;
        tsDLIterBD < baseNMIU > pNetIO = 
            chan.cacPrivateListOfIO::eventq.firstIter ();
        while ( pNetIO.valid () ) {
            tsDLIterBD < baseNMIU > next = pNetIO;
            next++;
            if ( ! pNetIO->isSubscription () ) {
                // no use after disconnected - so uninstall it
                this->ioTable.remove ( *pNetIO );
                chan.cacPrivateListOfIO::eventq.remove ( *pNetIO );
                tmpList.add ( *pNetIO );
            }
            pNetIO = next;
        }
        while ( baseNMIU *pIO = tmpList.get () ) {
            char buf[128];
            sprintf ( buf, "host = %100s", chan.pHostName() );
            {
                epicsAutoMutexRelease unlocker ( this->mutex );
                pIO->exception ( ECA_DISCONN, buf );
            }
            signalNeeded = this->blockForIOCallbackCompletion ( pIO->getID() );
            pIO->destroy ( *this );
        }
    }
    if ( signalNeeded ) {
        this->notifyCompletionEvent.signal ();
    }
}

void cac::destroyAllIO ( nciu &chan )
{
    bool signalNeeded = false;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        while ( baseNMIU *pIO = chan.cacPrivateListOfIO::eventq.get() ) {
            this->ioTable.remove ( *pIO );
            this->flushIfRequired ( chan );
            class netSubscription *pSubscr = pIO->isSubscription ();
            if ( pSubscr ) {
                chan.getPIIU()->subscriptionCancelRequest ( chan, *pSubscr );
            }
            signalNeeded = this->blockForIOCallbackCompletion ( pIO->getID() );
            pIO->destroy ( *this );
        }
    }
    if ( signalNeeded ) {
        this->notifyCompletionEvent.signal ();
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
            this->flushIfRequired ( chan );
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

bool cac::eventAddExcep ( tcpiiu &iiu, const caHdrLargeArray &hdr, 
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
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( pChan ) {
        unsigned ar = hdr.m_available;
        caAccessRights accessRights ( 
            ( ar & CA_PROTO_ACCESS_RIGHT_READ ) ? true : false, 
            ( ar & CA_PROTO_ACCESS_RIGHT_WRITE ) ? true : false); 
        pChan->accessRightsStateChange ( accessRights );
    }
    return true;
}

bool cac::claimCIURespAction ( tcpiiu &iiu, const caHdrLargeArray &hdr, void *pMsgBdy )
{
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( pChan ) {
        unsigned sidTmp;
        if ( iiu.ca_v44_ok() ) {
            sidTmp = hdr.m_available;
        }
        else {
            sidTmp = pChan->getSID ();
        }
        pChan->connect ( hdr.m_dataType, hdr.m_count, sidTmp, iiu.ca_v41_ok() );
        return true;
    }
    else {
        return true; // ignore claim response to deleted channel
    }
}

bool cac::verifyAndDisconnectChan ( tcpiiu &, const caHdrLargeArray &hdr, void * /* pMsgBdy */ )
{
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( pChan ) {
        assert ( this->pudpiiu && this->pSearchTmr );
        pChan->disconnect ( *this->pudpiiu );
        this->pSearchTmr->resetPeriod ( 0.0 );
    }
    return true;
}

bool cac::badTCPRespAction ( tcpiiu &iiu, const caHdrLargeArray &hdr, void * /* pMsgBdy */ )
{
    char hostName[64];
    iiu.hostName ( hostName, sizeof(hostName) );
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
    
    this->printf ( "CA.Client.Diagnostic..............................................\n" );
    
    this->printf ( "    %s: \"%s\"\n", 
        severity[ CA_EXTRACT_SEVERITY ( ca_status ) ], 
        ca_message ( ca_status ) );

    if  ( pFormat ) {
        this->printf ( "    Context: \"" );
        this->vPrintf ( pFormat, args );
        this->printf ( "\"\n" );
    }
        
    if (pfilenm) {
        this->printf ( "    Source File: %s Line Number: %d\n",
            pfilenm, lineno );    
    } 
    
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

void cac::selfTest ()
{
    this->chanTable.verify ();
    this->ioTable.verify ();
    this->sgTable.verify ();
    this->beaconTable.verify ();
}

