
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
#include "envdefs.h"

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

// TCP protocol jump table
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

//
// cac::cac ()
//
cac::cac ( cacNotify &notifyIn, bool enablePreemptiveCallbackIn ) :
    ipToAEngine ( "caIPAddrToAsciiEngine" ), 
    chanTable ( 1024 ),
    ioTable ( 1024 ),
    sgTable ( 128 ),
    beaconTable ( 1024 ),
    pudpiiu ( 0 ),
    pSearchTmr ( 0 ),
    pRepeaterSubscribeTmr ( 0 ),
    notify ( notifyIn ),
    ioNotifyInProgressId ( 0 ),
    initializingThreadsPriority ( epicsThreadGetPrioritySelf () ),
    threadsBlockingOnNotifyCompletion ( 0u ),
    enablePreemptiveCallback ( enablePreemptiveCallbackIn ),
    ioInProgress ( false )
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

    this->pTimerQueue = & epicsTimerQueueActive::allocate ( false, abovePriority );
    if ( ! this->pTimerQueue ) {
        free ( this->pUserName );
        throwWithLocation ( caErrorCode ( ECA_ALLOCMEM ) );
    }

    //
    // unfortunately, this must be created here in the 
    // constructor, and not on demand (only when it is needed)
    // because the enable reference count must be 
    // maintained whenever this object exists.
    //
    this->pRecvProcThread = new recvProcessThread ( this );
    if ( ! this->pRecvProcThread ) {
        this->pTimerQueue->release ();
        free ( this->pUserName );
        throwWithLocation ( caErrorCode ( ECA_ALLOCMEM ) );
    }
    else if ( this->enablePreemptiveCallback ) {
        // only after this->pRecvProcThread is valid
        this->enableCallbackPreemption ();
    }
}

cac::~cac ()
{
    this->enableCallbackPreemption ();

    //
    // make certain that process thread isnt deleting 
    // tcpiiu objects at the same that this thread is
    //
    if ( this->pRecvProcThread ) {
        this->pRecvProcThread->disable ();
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

    delete this->pRecvProcThread;
    delete this->pRepeaterSubscribeTmr;
    delete this->pSearchTmr;

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

    if ( ! this->enablePreemptiveCallback ) {
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

void cac::processRecvBacklog ()
{
    tsDLList < tcpiiu > deadIIU;
    {
        epicsAutoMutex autoMutex ( this->mutex );

        tsDLIterBD < tcpiiu > piiu = this->iiuList.firstIter ();
        while ( piiu.valid () ) {
            tsDLIterBD < tcpiiu > pNext = piiu;
            pNext++;

            if ( ! piiu->alive () ) {
                assert ( this->pudpiiu && this->pSearchTmr );

                bhe *pBHE = piiu->getBHE ();
                if ( pBHE ) {
                    this->beaconTable.remove ( *pBHE );
                    pBHE->destroy ();
                }

                if ( piiu->channelCount () ) {
                    char hostNameTmp[64];
                    piiu->hostName ( hostNameTmp, sizeof ( hostNameTmp ) );
                    genLocalExcep ( *this, ECA_DISCONN, hostNameTmp );
                }

                piiu->disconnectAllChan ( *this->pudpiiu );

                // make certain that:
                // 1) this is called from the appropriate thread
                // 2) lock is not held while in call back
                if ( ! this->enablePreemptiveCallback ) {
                    epicsAutoMutexRelease autoRelease ( this->mutex );
                    this->notify.fdWasDestroyed ( piiu->getSock() );
                }
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
        this->pSearchTmr->resetPeriod ( CA_RECAST_DELAY );
        while ( tcpiiu *piiu = deadIIU.get() ) {
            piiu->destroy ();
        }
    }
}

//
// set the push pending flag on all virtual circuits
//
void cac::flushRequest ()
{
    epicsAutoMutex autoMutex ( this->mutex );
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
        while ( piiu.valid () ) {
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
        this->ioCounter.show ( level - 2u );
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
        if ( this->pRecvProcThread ) {
            ::printf ( "incoming messages processing thread:\n" );
            this->pRecvProcThread->show ( level - 3u );
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
    }

    if ( level > 3u ) {
        ::printf ( "Default mutex:\n");
        this->mutex.show ( level - 4u );
    }
}

void cac::signalRecvActivity ()
{
    if ( this->pRecvProcThread ) {
        this->pRecvProcThread->signalActivity ();
    }
}

/*
 *  cac::beaconNotify
 */
void cac::beaconNotify ( const inetAddrID &addr )
{
    epicsAutoMutex autoMutex ( this->mutex );
    unsigned port;  
    bhe *pBHE;

    if ( ! this->pudpiiu ) {
        return;
    }

    /*
     * look for it in the hash table
     */
    pBHE = this->beaconTable.lookup ( addr );
    if ( pBHE ) {
        /*
         * return if the beacon period has not changed significantly
         */
        if ( ! pBHE->updatePeriod ( this->programBeginTime ) ) {
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
        pBHE = new bhe ( epicsTime::getCurrent (), addr );
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
    port = this->pudpiiu->getPort ();
    {
        ca_real     delay;

        delay = ( port & CA_RECAST_PORT_MASK );
        delay /= MSEC_PER_SEC;
        delay += CA_RECAST_DELAY;

        if ( this->pudpiiu->channelCount () > 0u  && this->pSearchTmr ) {
            this->pSearchTmr->resetPeriod ( delay );
        }
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
    if ( this->pRecvProcThread->isCurrentThread () ) {
        return ECA_EVDISALLOW;
    }

    this->enableCallbackPreemption ();

    this->flushRequest ();
   
    int status = ECA_NORMAL;
    epicsTime beg_time = epicsTime::getCurrent ();
    double remaining;
    if ( timeout == 0.0 ) {
        remaining = 60.0;
    }
    else{
        remaining = timeout;
    }    
    while ( this->ioCounter.currentCount () > 0 ) {
        if ( remaining < CAC_SIGNIFICANT_SELECT_DELAY ) {
            status = ECA_TIMEOUT;
            break;
        }
        this->ioCounter.waitForCompletion ( remaining );
        if ( timeout != 0.0 ) {
            double delay = epicsTime::getCurrent () - beg_time;
            remaining = timeout - delay;
        }    
    }

    this->ioCounter.cleanUp ();
    {
        epicsAutoMutex autoMutex ( this->mutex );
        if ( this->pudpiiu ) {
            this->pudpiiu->connectTimeoutNotify ();
        }
    }

    this->disableCallbackPreemption ();

    return status;
}

int cac::pendEvent ( const double &timeout )
{
    // prevent recursion nightmares by disabling calls to 
    // pendIO () from within a CA callback
    if ( this->pRecvProcThread->isCurrentThread () ) {
        return ECA_EVDISALLOW;
    }

    this->enableCallbackPreemption ();

    this->flushRequest ();

    if ( timeout == 0.0 ) {
        while ( true ) {
            epicsThreadSleep ( 60.0 );
        }
    }
    else if ( timeout >= CAC_SIGNIFICANT_SELECT_DELAY ) {
        epicsThreadSleep ( timeout );
    }

    this->disableCallbackPreemption ();

    return ECA_TIMEOUT;
}

bool cac::ioComplete () const
{
    if ( this->ioCounter.currentCount () == 0u ) {
        return true;
    }
    else{
        return false;
    }
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
    this->pSearchTmr->resetPeriod ( CA_RECAST_DELAY );
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

void cac::enableCallbackPreemption ()
{
    if ( this->pRecvProcThread ) {
        this->pRecvProcThread->enable ();
    }
}

void cac::disableCallbackPreemption ()
{
    if ( this->pRecvProcThread ) {
        this->pRecvProcThread->disable ();
    }
}

void cac::repeaterSubscribeConfirmNotify ()
{
    if ( this->pRepeaterSubscribeTmr ) {
        this->pRepeaterSubscribeTmr->confirmNotify ();
    }
}

bool cac::lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             unsigned typeCode, unsigned long count, 
             unsigned minorVersionNumber, const osiSockAddr &addr )
{
    unsigned  retrySeqNumber;

    if ( addr.sa.sa_family != AF_INET ) {
        return false;
    }

    {
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
            piiu = new tcpiiu ( *this, this->connTMO, *this->pTimerQueue );
            if ( ! piiu ) {
                return true;
            }
            if ( piiu->fullyConstructed () ) {
                this->iiuList.add ( *piiu  );
                if ( ! piiu->initiateConnect ( addr, minorVersionNumber, 
                            *pBHE, this->ipToAEngine ) ) {
                    this->iiuList.remove ( *piiu  );
                    piiu->destroy ();
                    return true;
                }
            }
            else {
                delete piiu;
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
    }

    if ( this->pSearchTmr ) {
        this->pSearchTmr->notifySearchResponse ( retrySeqNumber );
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

int cac::printf ( const char *pformat, ... )
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
        bool flushPermit = true;
        if ( this->pRecvProcThread ) {
            if ( this->pRecvProcThread->isCurrentThread () ) {
                flushPermit = false;
            }
        }
        if ( this->pudpiiu ) {
            if ( this->pudpiiu->isCurrentThread () ) {
                flushPermit = false;
            }
        }
        if ( flushPermit ) {
            chan.getPIIU()->blockUntilSendBacklogIsReasonable ( this->mutex );
        }
        else {
            this->flushRequest ();
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
                                    const void *pValue, cacWriteNotify &notify )
{
    epicsAutoMutex autoMutex ( this->mutex );
    autoPtrRecycle  < netWriteNotifyIO > pIO ( *this, netWriteNotifyIO::factory ( 
                this->freeListWriteNotifyIO, chan, notify ) );
    if ( pIO.get() ) {
        this->ioTable.add ( *pIO );
        chan.cacPrivateListOfIO::eventq.add ( *pIO );
        this->flushIfRequired ( chan );
        chan.getPIIU()->writeNotifyRequest ( 
            chan, *pIO, type, nElem, pValue );
        return pIO.release()->getId ();
    }
    else {
        throw cacChannel::noMemory ();
    }
}

cacChannel::ioid cac::readNotifyRequest ( nciu &chan, unsigned type, unsigned nElem, cacReadNotify &notify )
{
    epicsAutoMutex autoMutex ( this->mutex );
    autoPtrRecycle  < netReadNotifyIO > pIO ( *this, netReadNotifyIO::factory ( 
                this->freeListReadNotifyIO, chan, notify ) );
    if ( pIO.get() ) {
        this->flushIfRequired ( chan );
        this->ioTable.add ( *pIO );
        chan.cacPrivateListOfIO::eventq.add ( *pIO );
        chan.getPIIU()->readNotifyRequest ( chan, *pIO, type, nElem );
        return pIO.release()->getId ();
    }
    else {
        throw cacChannel::noMemory ();
    }
}

void cac::ioCancel ( nciu &chan, const cacChannel::ioid &id )
{   
    bool signalNeeded;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        baseNMIU * pmiu = this->ioTable.remove ( id );
        if ( pmiu ) {
            chan.cacPrivateListOfIO::eventq.remove ( *pmiu );
            pmiu->destroy ( *this );
        }
        assert ( this->threadsBlockingOnNotifyCompletion < UINT_MAX );
        this->threadsBlockingOnNotifyCompletion++;
        while ( this->ioInProgress && this->ioNotifyInProgressId == id ) {
            epicsAutoMutex autoMutexRelease ( this->mutex );
            this->notifyCompletionEvent.wait ( 0.5 );
        }
        assert ( this->threadsBlockingOnNotifyCompletion > 0u );
        this->threadsBlockingOnNotifyCompletion--;
        signalNeeded = this->threadsBlockingOnNotifyCompletion > 0u;
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

bool cac::ioCompletionNotify ( unsigned id, unsigned type, 
                              unsigned long count, const void *pData )
{
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( ! pmiu ) {
        return false;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    {
        epicsAutoMutexRelease ( this->mutex );
        pmiu->completion ( type, count, pData );
    }
    // threads blocked canceling this IO will wait
    // until we stop processing this IO
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
    return true;
}

bool cac::ioExceptionNotify ( unsigned id, int status, const char *pContext )
{
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( ! pmiu ) {
        return false;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    {
        epicsAutoMutexRelease ( this->mutex );
        pmiu->exception ( status, pContext );
    }
    // threads blocked canceling this IO will wait
    // until we stop processing this IO
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
    return true;
}

bool cac::ioExceptionNotify ( unsigned id, int status, 
                   const char *pContext, unsigned type, unsigned long count )
{
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( ! pmiu ) {
        return false;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    {
        epicsAutoMutexRelease ( this->mutex );
        pmiu->exception ( status, pContext, type, count );
    }
    // threads blocked canceling this IO will wait
    // until we stop processing this IO
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
    return true;
}

bool cac::ioCompletionNotifyAndDestroy ( unsigned id )
{
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return false;
    }
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    {
        epicsAutoMutexRelease ( this->mutex );
        pmiu->completion ();
    }
    pmiu->destroy ( *this );
    // threads blocked canceling this IO will wait
    // until we stop processing this IO
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
    return true;
}

bool cac::ioCompletionNotifyAndDestroy ( unsigned id, 
    unsigned type, unsigned long count, const void *pData )
{
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return false;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );
    {
        epicsAutoMutexRelease ( this->mutex );
        pmiu->completion ( type, count, pData );
    }
    pmiu->destroy ( *this );
    // threads blocked canceling this IO will wait
    // until we stop processing this IO
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
    return true;
}

bool cac::ioExceptionNotifyAndDestroy ( unsigned id, int status, const char *pContext )
{
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return false;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );
    {
        epicsAutoMutexRelease ( this->mutex );
        pmiu->exception ( status, pContext );
    }
    pmiu->destroy ( *this );
    // threads blocked canceling this IO will wait
    // until we stop processing this IO
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
    return true;
}

bool cac::ioExceptionNotifyAndDestroy ( unsigned id, int status, 
                        const char *pContext, unsigned type, unsigned long count )
{
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( ! pmiu ) {
        return false;
    }
    assert ( ! this->ioInProgress );
    this->ioInProgress = true;
    this->ioNotifyInProgressId = id;
    pmiu->channel().cacPrivateListOfIO::eventq.remove ( *pmiu );
    {
        epicsAutoMutexRelease ( this->mutex );
        pmiu->exception ( status, pContext, type, count );
    }
    pmiu->destroy ( *this );
    // threads blocked canceling this IO will wait
    // until we stop processing this IO
    this->ioInProgress = false;
    if ( this->threadsBlockingOnNotifyCompletion ) {
        this->notifyCompletionEvent.signal ();
    }
    return true;
}

// resubscribe for monitors from this channel
void cac::connectAllIO ( nciu &chan )
{
    tsDLList < baseNMIU > tmpList;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        tsDLIterBD < baseNMIU > pNetIO = 
            chan.cacPrivateListOfIO::eventq.firstIter ();
        while ( pNetIO.valid () ) {
            tsDLIterBD < baseNMIU > next = pNetIO;
            next++;
            class netSubscription *pSubscr = pNetIO->isSubscription ();
            if ( pSubscr ) {
                chan.getPIIU()->subscriptionRequest ( chan, *pSubscr );
            }
            else {
                // it shouldnt be here at this point - so uninstall it
                this->ioTable.remove ( *pNetIO );
                chan.cacPrivateListOfIO::eventq.remove ( *pNetIO );
                tmpList.add ( *pNetIO );
            }
            pNetIO = next;
        }
        chan.getPIIU()->flushRequest ();
    }
    while ( baseNMIU *pIO = tmpList.get () ) {
        pIO->exception ( ECA_INTERNAL, "strange IO exists when connecting channel?" );
        pIO->destroy ( *this );
    }
}

// cancel IO operations and monitor subscriptions
void cac::disconnectAllIO ( nciu &chan )
{
    tsDLList < baseNMIU > tmpList;
    {
        epicsAutoMutex autoMutex ( this->mutex );
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
    }
    while ( baseNMIU *pIO = tmpList.get () ) {
        pIO->exception ( ECA_DISCONN, chan.pHostName() );
        pIO->destroy ( *this );
    }
}

void cac::destroyAllIO ( nciu &chan )
{
    epicsAutoMutex autoMutex ( this->mutex );
    while ( baseNMIU *pIO = chan.cacPrivateListOfIO::eventq.get() ) {
        this->ioTable.remove ( *pIO );
        this->flushIfRequired ( chan );
        class netSubscription *pSubscr = pIO->isSubscription ();
        if ( pSubscr ) {
            chan.getPIIU()->subscriptionCancelRequest ( chan, *pSubscr );
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
    unsigned long nElem, unsigned mask, cacStateNotify &notify )
{
    epicsAutoMutex autoMutex ( this->mutex );
    autoPtrRecycle  < netSubscription > pIO ( *this, netSubscription::factory ( 
                this->freeListSubscription, chan, type, nElem, mask, notify ) );
    if ( pIO.get() ) {
        chan.cacPrivateListOfIO::eventq.add ( *pIO );
        this->ioTable.add ( *pIO );
        if ( chan.connected() ) {
            this->flushIfRequired ( chan );
            chan.getPIIU()->subscriptionRequest ( chan, *pIO );
        }
        cacChannel::ioid id = pIO->getId ();
        pIO.release();
        return id;
    }
    else {
        throw cacChannel::noMemory();
    }
}

bool cac::noopAction ( tcpiiu &, const caHdr &, void * /* pMsgBdy */ )
{
    return true;
}
 
bool cac::echoRespAction ( tcpiiu &, const caHdr &, void * /* pMsgBdy */ )
{
    return true;
}

bool cac::writeNotifyRespAction ( tcpiiu &, const caHdr &hdr, void * /* pMsgBdy */ )
{
    int caStatus = hdr.m_cid;
    if ( caStatus == ECA_NORMAL ) {
        return this->ioCompletionNotifyAndDestroy ( hdr.m_available );
    }
    else {
        return this->ioExceptionNotifyAndDestroy ( hdr.m_available, 
                    caStatus, "write notify request rejected" );
    }
}

bool cac::readNotifyRespAction ( tcpiiu &iiu, const caHdr &hdr, void *pMsgBdy )
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
            caStatus = htonl ( ECA_BADTYPE );
        }
#   endif

    if ( caStatus == ECA_NORMAL ) {
        return this->ioCompletionNotifyAndDestroy ( hdr.m_available,
            hdr.m_dataType, hdr.m_count, pMsgBdy );
    }
    else {
        return this->ioExceptionNotifyAndDestroy ( hdr.m_available,
            caStatus, "read failed", hdr.m_dataType, hdr.m_count );
    }
}

bool cac::eventRespAction ( tcpiiu &iiu, const caHdr &hdr, void *pMsgBdy )
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
        return this->ioCompletionNotify ( hdr.m_available,
            hdr.m_dataType, hdr.m_count, pMsgBdy );
    }
    else {
        return this->ioExceptionNotify ( hdr.m_available,
                caStatus, "subscription update failed", 
                hdr.m_dataType, hdr.m_count );
    }
}

bool cac::readRespAction ( tcpiiu &, const caHdr &hdr, void *pMsgBdy )
{
    return this->ioCompletionNotifyAndDestroy ( hdr.m_available,
        hdr.m_dataType, hdr.m_count, pMsgBdy );
}

bool cac::clearChannelRespAction ( tcpiiu &, const caHdr &, void * /* pMsgBdy */ )
{
    return true; // currently a noop
}

bool cac::exceptionRespAction ( tcpiiu &iiu, const caHdr &hdr, void *pMsgBdy )
{
    const caHdr * req = reinterpret_cast < const caHdr * > ( pMsgBdy );
    char context[255];
    char hostName[64];

    const char *pName = reinterpret_cast < const char * > ( req + 1 );

    iiu.hostName ( hostName, sizeof(hostName) );
    sprintf ( context, "detected by: %s for: %s", 
        hostName,  pName);

    switch ( ntohs ( req->m_cmmd ) ) {
    case CA_PROTO_READ_NOTIFY:
        return this->ioExceptionNotifyAndDestroy ( ntohl ( req->m_available ), 
            ntohl ( hdr.m_available ), context, 
            ntohs ( req->m_dataType ), ntohs ( req->m_count ) );
    case CA_PROTO_READ:
        return this->ioExceptionNotifyAndDestroy ( ntohl (req->m_available), 
            ntohl ( hdr.m_available ), context, 
            ntohs ( req->m_dataType ), ntohs ( req->m_count ) );
    case CA_PROTO_WRITE_NOTIFY:
        return this->ioExceptionNotifyAndDestroy ( ntohl (req->m_available), 
            ntohl ( hdr.m_available ), context, 
            ntohs ( req->m_dataType ), ntohs ( req->m_count ) );
    case CA_PROTO_WRITE:
        this->exception ( ntohl ( hdr.m_available ), 
                context, ntohs ( req->m_dataType ), ntohs ( req->m_count ), __FILE__, __LINE__);
        return true;
    case CA_PROTO_EVENT_ADD:
        return this->ioExceptionNotify ( ntohl ( req->m_available ), 
            ntohl ( hdr.m_available ), context, 
            ntohs ( req->m_dataType ), ntohs ( req->m_count ) );
    case CA_PROTO_EVENT_CANCEL:
        return this->ioExceptionNotifyAndDestroy ( ntohl ( req->m_available ), 
            ntohl ( hdr.m_available ), context );
    default:
        this->exception ( ntohl ( hdr.m_available ), 
            context, __FILE__, __LINE__ );
        return true;
    }
}

bool cac::accessRightsRespAction ( tcpiiu &, const caHdr &hdr, void * /* pMsgBdy */ )
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

bool cac::claimCIURespAction ( tcpiiu &iiu, const caHdr &hdr, void * /* pMsgBdy */ )
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
        pChan->connect ( hdr.m_dataType, hdr.m_count, sidTmp );
        return true;
    }
    else {
        return false;
    }
}

bool cac::verifyAndDisconnectChan ( tcpiiu &, const caHdr &hdr, void * /* pMsgBdy */ )
{
    nciu * pChan = this->chanTable.lookup ( hdr.m_cid );
    if ( pChan ) {
        assert ( this->pudpiiu && this->pSearchTmr );
        pChan->disconnect ( *this->pudpiiu );
        this->pSearchTmr->resetPeriod ( CA_RECAST_DELAY );
    }
    return true;
}

bool cac::badTCPRespAction ( tcpiiu &iiu, const caHdr &hdr, void * /* pMsgBdy */ )
{
    char hostName[64];
    iiu.hostName ( hostName, sizeof(hostName) );
    this->printf ( "CAC: Undecipherable TCP message ( bad response type %u ) from %s\n", 
        hdr.m_cmmd, hostName );
    return false;
}

void cac::executeResponse ( tcpiiu &iiu, caHdr &hdr, char *pMshBody )
{
    // execute the response message
    pProtoStubTCP pStub;
    if ( hdr.m_cmmd >= NELEMENTS ( cac::tcpJumpTableCAC ) ) {
        pStub = &cac::badTCPRespAction;
    }
    else {
        pStub = cac::tcpJumpTableCAC [hdr.m_cmmd];
    }
    ( this->*pStub ) ( iiu, hdr, pMshBody );
}


