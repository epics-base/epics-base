
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

#include "osiProcess.h"
#include "osiSigPipeIgnore.h"
#include "iocinf.h"

#include "cac_IL.h"
#include "inetAddrID_IL.h"
#include "bhe_IL.h"
#include "tcpiiu_IL.h"
#include "nciu_IL.h"

static void cacRecursionLockExitHandler ()
{
    if ( cacRecursionLock ) {
        threadPrivateDelete ( cacRecursionLock );
        cacRecursionLock = 0;
    }
}

static void cacInitRecursionLock ( void * )
{
    cacRecursionLock = threadPrivateCreate ();
    if ( cacRecursionLock ) {
        atexit ( cacRecursionLockExitHandler );
    }
}

//
// cac::cac ()
//
cac::cac ( bool enablePreemptiveCallbackIn ) :
    ipToAEngine ( "caIPAddrToAsciiEngine" ), 
    ioTable ( 1024 ),
    chanTable ( 1024 ),
    sgTable ( 128 ),
    beaconTable ( 1024 ),
    fdRegFunc ( 0 ),
    fdRegArg ( 0 ),
    pudpiiu ( 0 ),
    pSearchTmr ( 0 ),
    pRepeaterSubscribeTmr ( 0 ),
    pndrecvcnt ( 0 ),
    enablePreemptiveCallback ( enablePreemptiveCallbackIn )
{
	long status;
    static threadOnceId once = OSITHREAD_ONCE_INIT;
    unsigned abovePriority;

    threadOnce ( &once, cacInitRecursionLock, 0 );

    if ( cacRecursionLock == 0 ) {
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

	if ( ! osiSockAttach () ) {
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
	}

    {
        threadBoolStatus tbs;
        unsigned selfPriority = threadGetPrioritySelf ();

        tbs = threadLowestPriorityLevelAbove ( selfPriority, &abovePriority);
        if ( tbs != tbsSuccess ) {
            abovePriority = selfPriority;
        }
    }

    this->pTimerQueue = new osiTimerQueue ( osiTimerQueue::mtsCreateManagerThread, abovePriority );
    if ( ! this->pTimerQueue ) {
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

    this->pVPrintfFunc = errlogVprintf;
    this->ca_exception_func = ca_default_exception_handler;
    this->ca_exception_arg = NULL;
    this->readSeq = 0u;

    this->ca_blockSem = semBinaryCreate(semEmpty);
    if (!this->ca_blockSem) {
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
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
            semBinaryDestroy (this->ca_blockSem);
            throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
        }
        strncpy ( this->pUserName, tmp, len );
    }

    this->programBeginTime = osiTime::getCurrent ();

    status = envGetDoubleConfigParam ( &EPICS_CA_CONN_TMO, &this->connTMO );
    if ( status ) {
        this->connTMO = CA_CONN_VERIFY_PERIOD;
        ca_printf (
            "EPICS \"%s\" double fetch failed\n",
            EPICS_CA_CONN_TMO.name);
        ca_printf (
            "Defaulting \"%s\" = %f\n",
            EPICS_CA_CONN_TMO.name,
            this->connTMO);
    }

    //
    // unfortunately, this must be created here in the 
    // constructor, and not on demand (only when it is needed)
    // because the enable reference count must be 
    // maintained whenever this object exists.
    //
    this->pRecvProcThread = new recvProcessThread ( this );
    if ( ! this->pRecvProcThread ) {
        throwWithLocation ( caErrorCode ( ECA_ALLOCMEM ) );
    }
    else if ( this->enablePreemptiveCallback ) {
        // only after this->pRecvProcThread is valid
        this->enableCallbackPreemption ();
    }
}

/*
 * cac::~cac ()
 *
 * releases all resources alloc to a channel access client
 */
cac::~cac ()
{
    this->enableCallbackPreemption ();

    //
    // destroy local IO channels
    //
    this->defaultMutex.lock ();
    tsDLIterBD <cacLocalChannelIO> iter ( this->localChanList.first () );
    while ( iter.valid () ) {
        tsDLIterBD <cacLocalChannelIO> pnext = iter.itemAfter ();
        iter->destroy ();
        iter = pnext;
    }
    this->defaultMutex.unlock ();

    //
    // make certain that process thread isnt deleting 
    // tcpiiu objects at the same that this thread is
    //
    recvProcessThread *pTmp = this->pRecvProcThread;
    this->pRecvProcThread = 0;
    delete pTmp;

    //
    // shutdown all tcp connections and wait for threads to exit
    //
    this->iiuListMutex.lock ();
    tsDLIterBD <tcpiiu> piiu ( this->iiuList.first () );
    while ( piiu.valid () ) {
        tsDLIterBD <tcpiiu> pnext = piiu.itemAfter ();
        piiu->suicide ();
        piiu = pnext;
    }
    this->iiuListMutex.unlock ();

    this->defaultMutex.lock ();

    //
    // shutdown udp and wait for threads to exit
    //
    if ( this->pudpiiu ) {
        if ( ! this->enablePreemptiveCallback ) {
            if ( this->fdRegFunc ) {
                ( *this->fdRegFunc ) 
                    ( this->fdRegArg, this->pudpiiu->getSock (), FALSE );
            }
        }
        delete this->pSearchTmr;
        delete this->pRepeaterSubscribeTmr;
        delete this->pudpiiu;
    }

    /*
     * free user name string
     */
    if ( this->pUserName ) {
        delete [] this->pUserName;
    }

    this->sgTable.destroyAllEntries ();
    this->beaconTable.destroyAllEntries ();
    this->chanTable.destroyAllEntries ();
    this->ioTable.destroyAllEntries ();
    semBinaryDestroy ( this->ca_blockSem );

    osiSockRelease ();

    delete this->pTimerQueue;
}

void cac::processRecvBacklog ()
{
    this->iiuListMutex.lock ();

    tsDLIterBD <tcpiiu> piiu ( this->iiuList.first () );
    while ( piiu.valid () ) {
        tsDLIterBD <tcpiiu> pNext = piiu.itemAfter ();
        piiu->processIncomingAndDestroySelfIfDisconnected ();
        piiu = pNext;
	}

    this->iiuListMutex.unlock ();
}

/*
 * cac::flush ()
 */
void cac::flush ()
{
    /*
     * set the push pending flag on all virtual circuits
     */
    this->iiuListMutex.lock ();
    tsDLIterBD <tcpiiu> piiu ( this->iiuList.first () );
    while ( piiu.valid () ) {
        piiu->flush ();
        piiu++;
    }
    this->iiuListMutex.unlock ();
}



/*
 *
 * set pending IO count back to zero and
 * send a sync to each IOC and back. dont
 * count reads until we recv the sync
 *
 */
void cac::cleanUpPendIO ()
{
    this->defaultMutex.lock ();

    this->readSeq++;
    this->pndrecvcnt = 0u;

    this->defaultMutex.unlock ();

    if ( this->pudpiiu ) {
        this->pudpiiu->connectTimeoutNotify ();
    }
}

unsigned cac::connectionCount () const
{
    return this->iiuList.count ();
}

void cac::show (unsigned level) const
{
    if ( this->pudpiiu ) {
        this->pudpiiu->show (level);
    }

    this->iiuListMutex.lock ();

    tsDLIterConstBD <tcpiiu> piiu ( this->iiuList.first () );
    while ( piiu.valid () ) {
        piiu->show (level);
        piiu++;
	}

    this->iiuListMutex.unlock ();
}

void cac::installIIU ( tcpiiu &iiu )
{
    this->iiuListMutex.lock ();
    this->iiuList.add (iiu);
    this->iiuListMutex.unlock ();

    this->defaultMutex.lock ();
    if ( ! this->enablePreemptiveCallback && this->fdRegFunc ) {
        ( * this->fdRegFunc ) 
            ( (void *) this->fdRegArg, iiu.getSock (), TRUE );
    }
    this->defaultMutex.unlock ();
}

void cac::signalRecvActivity ()
{
    if ( this->pRecvProcThread ) {
        this->pRecvProcThread->signalActivity ();
    }
}

void cac::removeIIU ( tcpiiu &iiu )
{
    this->defaultMutex.lock ();
    osiSockAddr addr = iiu.address ();
    if ( addr.sa.sa_family == AF_INET ) {
        bhe *pBHE = this->lookupBeaconInetAddr ( addr.ia );
        if ( pBHE ) {
            pBHE->destroy ();
        }
    }
    else {
        errlogPrintf ( "CA server didnt have inet type address?\n" );
    }
    this->defaultMutex.unlock ();

    this->iiuListMutex.lock ();

    this->iiuList.remove (iiu);

    if ( ! this->enablePreemptiveCallback ) {
        if ( this->fdRegFunc ) {
            (*this->fdRegFunc) 
                ((void *)this->fdRegArg, iiu.getSock (), FALSE);
        }
    }

    this->iiuListMutex.unlock ();
}

/*
 * cac::lookupBeaconInetAddr()
 */
bhe * cac::lookupBeaconInetAddr (const inetAddrID &ina)
{
    bhe *pBHE;
    this->defaultMutex.lock ();
    pBHE = this->beaconTable.lookup (ina);
    this->defaultMutex.unlock ();
    return pBHE;
}

/*
 * cac::createBeaconHashEntry ()
 */
bhe *cac::createBeaconHashEntry (const inetAddrID &ina, const osiTime &initialTimeStamp)
{
    bhe *pBHE;

    this->defaultMutex.lock ();

    pBHE = this->beaconTable.lookup ( ina );
    if ( !pBHE ) {
        pBHE = new bhe (*this, initialTimeStamp, ina);
        if ( pBHE ) {
            if ( this->beaconTable.add (*pBHE) < 0 ) {
                pBHE->destroy ();
                pBHE = 0;
            }
        }
    }

    this->defaultMutex.unlock ();

    return pBHE;
}

/*
 *  cac::beaconNotify
 */
void cac::beaconNotify ( const inetAddrID &addr )
{
    bhe *pBHE;
    unsigned port;  
    int netChange;

    if ( ! this->pudpiiu ) {
        return;
    }

    this->defaultMutex.lock ();

    /*
     * look for it in the hash table
     */
    pBHE = this->lookupBeaconInetAddr ( addr );
    if ( pBHE ) {
        netChange = pBHE->updateBeaconPeriod ( this->programBeginTime );
    }
    else {
        /*
         * This is the first beacon seen from this server.
         * Wait until 2nd beacon is seen before deciding
         * if it is a new server (or just the first
         * time that we have seen a server's beacon
         * shortly after the program started up)
         */
        netChange = FALSE;
        this->createBeaconHashEntry ( addr, osiTime::getCurrent () );
    }

    if ( ! netChange ) {
        this->defaultMutex.unlock ();
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
     * I fetch the local port number and use the low order bits
     * as a pseudo random delay to prevent every one 
     * from replying at once.
     */
    {
        struct sockaddr_in saddr;
        osiSocklen_t saddr_length = sizeof ( saddr );
        int status;

        status = getsockname ( this->pudpiiu->getSock (), (struct sockaddr *) &saddr, &saddr_length );
        if ( status < 0 ) {
            epicsPrintf ( "CAC: getsockname () error was \"%s\"\n", SOCKERRSTR (SOCKERRNO) );
            this->defaultMutex.unlock ();
            return;
        }
        port = ntohs ( saddr.sin_port );
    }

    {
        ca_real     delay;

        delay = ( port & CA_RECAST_PORT_MASK );
        delay /= MSEC_PER_SEC;
        delay += CA_RECAST_DELAY;

        if ( this->pSearchTmr ) {
            this->pSearchTmr->reset ( delay );
        }
    }

    this->defaultMutex.unlock ();

    this->pudpiiu->resetChannelRetryCounts ();

#   if DEBUG
    {
        char buf[64];
        ipAddrToA (pnet_addr, buf, sizeof ( buf ) );
        printf ("new server available: %s\n", buf);
    }
#   endif

}

/*
 * cac::removeBeaconInetAddr ()
 */
void cac::removeBeaconInetAddr (const inetAddrID &ina)
{
    bhe     *pBHE;

    this->defaultMutex.lock ();
    pBHE = this->beaconTable.remove ( ina );
    this->defaultMutex.unlock ();

    assert (pBHE);
}

void cac::decrementOutstandingIO ( unsigned seqNumber )
{
    bool signalNeeded;
    this->defaultMutex.lock ();
    if ( this->readSeq == seqNumber ) {
        if ( this->pndrecvcnt > 0u ) {
            this->pndrecvcnt--;
            if ( this->pndrecvcnt == 0u ) {
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
        signalNeeded = true;
    }
    this->defaultMutex.unlock ();

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

void cac::decrementOutstandingIO ()
{
    bool signalNeeded;
    this->defaultMutex.lock ();
    if ( this->pndrecvcnt > 0u ) {
        this->pndrecvcnt--;
        if ( this->pndrecvcnt == 0u ) {
            signalNeeded = true;
        }
        else {
            signalNeeded = false;
        }
    }
    else {
        signalNeeded = true;
    }
    this->defaultMutex.unlock ();

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

void cac::incrementOutstandingIO ()
{
    this->defaultMutex.lock ();
    if ( this->pndrecvcnt < UINT_MAX ) {
        this->pndrecvcnt++;
    }
    this->defaultMutex.unlock ();
}

unsigned cac::readSequence () const
{
    return this->readSeq;
}

int cac::pend ( double timeout, int early )
{
    int status;
    void *p;

    /*
     * dont allow recursion
     */
    p = threadPrivateGet ( cacRecursionLock );
    if (p) {
        return ECA_EVDISALLOW;
    }

    threadPrivateSet ( cacRecursionLock, &cacRecursionLock );

    this->enableCallbackPreemption ();

    status = this->pendPrivate ( timeout, early );

    this->disableCallbackPreemption ();

    threadPrivateSet ( cacRecursionLock, NULL );

    return status;
}

/*
 * cac::pendPrivate ()
 */
int cac::pendPrivate (double timeout, int early)
{
    osiTime     cur_time;
    osiTime     beg_time;
    double      delay;

    this->flush ();

    if ( this->pndrecvcnt == 0u && early ) {
        return ECA_NORMAL;
    }
   
    if ( timeout < 0.0 ) {
        if (early) {
            this->cleanUpPendIO ();
        }
        return ECA_TIMEOUT;
    }

    beg_time = cur_time = osiTime::getCurrent ();

    delay = 0.0;
    while ( true ) {
        ca_real  remaining;
        
        if ( timeout == 0.0 ) {
            remaining = 60.0;
        }
        else{
            remaining = timeout - delay;

            /*
             * If we are not waiting for any significant delay
             * then force the delay to zero so that we avoid
             * scheduling delays (which can be substantial
             * on some os)
             */
            if ( remaining <= CAC_SIGNIFICANT_SELECT_DELAY ) {
                if ( early ) {
                    this->cleanUpPendIO ();
                }
                return ECA_TIMEOUT;
            }
        }    
        
        this->ioDone.wait ( remaining );

        if ( this->pndrecvcnt == 0 && early ) {
            return ECA_NORMAL;
        }
 
        cur_time = osiTime::getCurrent ();

        if ( timeout != 0.0 ) {
            delay = cur_time - beg_time;
        }
    }
}

bool cac::ioComplete () const
{
    if ( this->pndrecvcnt == 0u ) {
        return true;
    }
    else{
        return false;
    }
}

void cac::ioInstall ( nciu &chan, baseNMIU &io )
{
    this->defaultMutex.lock ();
    this->ioTable.add ( io );
    chan.cacPrivate::eventq.add ( io );
    this->defaultMutex.unlock ();
}

void cac::ioDestroy ( unsigned id )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( pmiu ) {
        pmiu->chan.cacPrivate::eventq.remove ( *pmiu );
    }
    this->defaultMutex.unlock ();
    // care is taken to not destroy with the cac lock
    // applied because we could potentially hold the 
    // cac lock while sending and deadlock with the 
    // recv thread, but we must uninstall the IO 
    // before accessing it with the lock released
    if ( pmiu ) {
        pmiu->destroy ();
    }
}

void cac::ioCompletionNotify ( unsigned id )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( pmiu ) {
        pmiu->completionNotify ();
    }
    this->defaultMutex.unlock ();
}

void cac::ioCompletionNotify ( unsigned id, unsigned type, 
                              unsigned long count, const void *pData )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( pmiu ) {
        pmiu->completionNotify ( type, count, pData );
    }
    this->defaultMutex.unlock ();
}

void cac::ioExceptionNotify ( unsigned id, int status, const char *pContext )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( pmiu ) {
        pmiu->exceptionNotify ( status, pContext );
    }
    this->defaultMutex.unlock ();
}

void cac::ioExceptionNotify ( unsigned id, int status, 
                   const char *pContext, unsigned type, unsigned long count )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( pmiu ) {
        pmiu->exceptionNotify ( status, pContext, type, count );
    }
    this->defaultMutex.unlock ();
}

void cac::ioCompletionNotifyAndDestroy ( unsigned id )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( pmiu ) {
        pmiu->chan.cacPrivate::eventq.remove ( *pmiu );
    }
    this->defaultMutex.unlock ();
    // care is taken to not destroy with the cac lock
    // applied because we could potentially hold the 
    // cac lock while sending and deadlock with the 
    // recv thread, but we must uninstall the IO 
    // before accessing it with the lock released
    if ( pmiu ) {
        pmiu->completionNotify ();
        pmiu->destroy (); 
    }
}

void cac::ioCompletionNotifyAndDestroy ( unsigned id, 
                        unsigned type, unsigned long count, const void *pData )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( pmiu ) {
        pmiu->chan.cacPrivate::eventq.remove ( *pmiu );
    }
    this->defaultMutex.unlock ();
    // care is taken to not destroy with the cac lock
    // applied because we could potentially hold the 
    // cac lock while sending and deadlock with the 
    // recv thread, but we must uninstall the IO 
    // before accessing it with the lock released
    if ( pmiu ) {
        pmiu->completionNotify ( type, count, pData );
        pmiu->destroy ();
    }
}

void cac::ioExceptionNotifyAndDestroy ( unsigned id, int status, const char *pContext )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( pmiu ) {
        pmiu->chan.cacPrivate::eventq.remove ( *pmiu );
    }
    this->defaultMutex.unlock ();
    // care is taken to not destroy with the cac lock
    // applied because we could potentially hold the 
    // cac lock while sending and deadlock with the 
    // recv thread, but we must uninstall the IO 
    // before accessing it with the lock released
    if ( pmiu ) {
        pmiu->exceptionNotify ( status, pContext );
        pmiu->destroy ();
    }
}

void cac::ioExceptionNotifyAndDestroy ( unsigned id, int status, 
                        const char *pContext, unsigned type, unsigned long count )
{
    this->defaultMutex.lock ();
    baseNMIU * pmiu = this->ioTable.remove ( id );
    if ( pmiu ) {
        pmiu->chan.cacPrivate::eventq.remove ( *pmiu );
    }
    this->defaultMutex.unlock ();
    // care is taken to not destroy with the cac lock
    // applied because we could potentially hold the 
    // cac lock while sending and deadlock with the 
    // recv thread, but we must uninstall the IO 
    // before accessing it with the lock released
    if ( pmiu ) {
        pmiu->exceptionNotify ( status, pContext, type, count );
        pmiu->destroy (); 
    }
}

void cac::registerChannel (nciu &chan)
{
    this->defaultMutex.lock ();
    this->chanTable.add ( chan );
    this->defaultMutex.unlock ();
}

void cac::unregisterChannel ( nciu &chan )
{
    this->defaultMutex.lock ();
    this->chanTable.remove ( chan );
    this->defaultMutex.unlock ();
}

void cac::accessRightsNotify ( unsigned id, caar ar )
{
    this->defaultMutex.lock ();
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        pChan->accessRightsStateChange ( ar );
    }
    this->defaultMutex.unlock ();
}

void cac::connectChannel ( unsigned id, class tcpiiu &iiu, 
          unsigned nativeType, unsigned long nativeCount, unsigned sid )
{
    this->defaultMutex.lock ();
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        unsigned sidTmp;
        if ( iiu.ca_v44_ok () ) {
            sidTmp = sid;
        }
        else {
            sidTmp = pChan->getSID ();
        }
        pChan->connect ( iiu, nativeType, nativeCount, sidTmp );
    }
    this->defaultMutex.unlock ();
}

void cac::channelDestroy ( unsigned id )
{
    this->defaultMutex.lock ();
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        pChan->destroy ();
    }
    this->defaultMutex.unlock ();
}

void cac::disconnectChannel ( unsigned id )
{
    this->defaultMutex.lock ();
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        pChan->disconnect ();
    }
    this->defaultMutex.unlock ();
}

void cac::installCASG (CASG &sg)
{
    this->defaultMutex.lock ();
    this->sgTable.add ( sg );
    this->defaultMutex.unlock ();
}

void cac::uninstallCASG (CASG &sg)
{
    this->defaultMutex.lock ();
    this->sgTable.remove ( sg );
    this->defaultMutex.unlock ();
}

CASG * cac::lookupCASG (unsigned id)
{
    this->defaultMutex.lock ();
    CASG * psg = this->sgTable.lookup ( id );
    if ( psg ) {
        if ( ! psg->verify () ) {
            psg = 0;
        }
    }
    this->defaultMutex.unlock ();
    return psg;
}

void cac::exceptionNotify (int status, const char *pContext,
    const char *pFileName, unsigned lineNo)
{
    ca_signal_with_file_and_lineno ( status, pContext, pFileName, lineNo );
}

void cac::exceptionNotify (int status, const char *pContext,
    unsigned type, unsigned long count, 
    const char *pFileName, unsigned lineNo)
{
    ca_signal_formated ( status, pFileName, lineNo, "%s type=%d count=%ld\n", 
        pContext, type, count );
}

void cac::registerService ( cacServiceIO &service )
{
    this->services.registerService ( service );
}

bool cac::createChannelIO (const char *pName, cacChannel &chan)
{
    cacLocalChannelIO *pIO;

    pIO = this->services.createChannelIO ( pName, chan );
    if ( ! pIO ) {
        pIO = cacGlobalServiceList.createChannelIO ( pName, chan );
        if ( ! pIO ) {
            if ( ! this->pudpiiu ) {
                if ( ! this->setupUDP () ) {
                    return false;
                }
            }
            nciu *pNetChan = new nciu ( *this, chan, pName );
            if ( pNetChan ) {
                if ( ! pNetChan->fullyConstructed () ) {
                    pNetChan->destroy ();
                    return false;
                }
                else {
                    return true;
                }
            }
            else {
                return false;
            }
        }
    }
    this->defaultMutex.lock ();
    this->localChanList.add ( *pIO );
    this->defaultMutex.unlock ();
    return true;
}

bool cac::setupUDP ()
{
    this->defaultMutex.lock ();

    if ( this->pudpiiu ) {
        this->defaultMutex.unlock ();
        return true;
    }

    this->pudpiiu = new udpiiu ( *this );
    if ( ! this->pudpiiu ) {
        this->defaultMutex.unlock ();
        return false;
    }

    this->pSearchTmr = new searchTimer ( *this->pudpiiu, *this->pTimerQueue );
    if ( ! this->pSearchTmr ) {
        delete this->pudpiiu;
        this->pudpiiu = 0;
        this->defaultMutex.unlock ();
        return false;
    }

    this->pRepeaterSubscribeTmr = new repeaterSubscribeTimer ( *this->pudpiiu, *this->pTimerQueue );
    if ( ! this->pRepeaterSubscribeTmr ) {
        delete this->pSearchTmr;
        delete this->pudpiiu;
        this->pudpiiu = 0;
        this->defaultMutex.unlock ();
        return false;
    }

    this->defaultMutex.unlock ();

    if ( ! this->enablePreemptiveCallback ) {
        if ( this->fdRegFunc ) {
            ( *this->fdRegFunc )
                ( this->fdRegArg, this->pudpiiu->getSock (), TRUE );
        }
    }

    return true;
}

void cac::registerForFileDescriptorCallBack ( CAFDHANDLER *pFunc, void *pArg )
{
    this->defaultMutex.lock ();
    this->fdRegFunc = pFunc;
    this->fdRegArg = pArg;
    this->defaultMutex.unlock ();
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

void cac::changeExceptionEvent ( caExceptionHandler *pfunc, void *arg )
{
    this->defaultMutex.lock ();
    if ( pfunc ) {
        this->ca_exception_func = pfunc;
        this->ca_exception_arg = arg;
    }
    else {
        this->ca_exception_func = ca_default_exception_handler;
        this->ca_exception_arg = NULL;
    }
    this->defaultMutex.unlock ();
}

//
// cac::genLocalExcepWFL ()
// (generate local exception with file and line number)
//
void cac::genLocalExcepWFL (long stat, const char *ctx, const char *pFile, unsigned lineNo)
{
    struct exception_handler_args args;
    caExceptionHandler *pExceptionFunc;

    /*
     * NOOP if they disable exceptions
     */
    if ( this->ca_exception_func ) {
        args.chid = NULL;
        args.type = -1;
        args.count = 0u;
        args.addr = NULL;
        args.stat = stat;
        args.op = CA_OP_OTHER;
        args.ctx = ctx;
        args.pFile = pFile;
        args.lineNo = lineNo;

        this->defaultMutex.lock ();
        pExceptionFunc = this->ca_exception_func;
        args.usr = this->ca_exception_arg;
        this->defaultMutex.unlock ();

        (*pExceptionFunc) (args);
    }
}

void cac::installDisconnectedChannel ( nciu &chan )
{

    assert ( this->pudpiiu && this->pSearchTmr );

    chan.attachChanToIIU ( *this->pudpiiu );
    chan.resetRetryCount ();
    this->pSearchTmr->reset ( CA_RECAST_DELAY );
}

void cac::notifySearchResponse ( unsigned short retrySeqNo )
{
    if ( this->pSearchTmr ) {
        this->pSearchTmr->notifySearchResponse ( retrySeqNo );
    }
}

void cac::repeaterSubscribeConfirmNotify ()
{
    if ( this->pRepeaterSubscribeTmr ) {
        this->pRepeaterSubscribeTmr->confirmNotify ();
    }
}

void cac::replaceErrLogHandler ( caPrintfFunc *ca_printf_func )
{
    this->defaultMutex.lock ();
    if ( ca_printf_func ) {
        this->pVPrintfFunc = ca_printf_func;
    }
    else {
        this->pVPrintfFunc = epicsVprintf;
    }
    this->defaultMutex.unlock ();
}

/*
 * constructTCPIIU ()
 */
tcpiiu * cac::constructTCPIIU ( const osiSockAddr &addr, unsigned minorVersion )
{
    bhe *pBHE;
    tcpiiu *piiu;

    if ( addr.sa.sa_family != AF_INET ) {
        return 0u;
    }

    /*
     * look for an existing virtual circuit
     */
    this->defaultMutex.lock ();
    pBHE = this->lookupBeaconInetAddr ( addr.ia );
    if ( ! pBHE ) {
        pBHE = this->createBeaconHashEntry ( addr.ia, osiTime () );
        if ( ! pBHE ) {
            this->defaultMutex.unlock ();
            return NULL;
        }
    }
    
    piiu = pBHE->getIIU ();
    if ( piiu ) {
        if ( piiu->alive () ) {
            this->defaultMutex.unlock ();
            return piiu;
        }
        else {
            this->defaultMutex.unlock ();
            return NULL;
        }
    }
    this->defaultMutex.unlock ();

    piiu = new tcpiiu ( *this, addr, minorVersion, 
        *pBHE, this->connTMO, *this->pTimerQueue,
        this->ipToAEngine );
    if ( ! piiu ) {
        return NULL;
    }

    if ( piiu->fullyConstructed () ) {
        return piiu;
    }
    else {
        delete piiu;
        return NULL;
    }
}

void cac::lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             unsigned typeCode, unsigned long count, 
             unsigned minorVersionNumber, const osiSockAddr &addr )
{
    unsigned  retrySeqNumber;
    tcpiiu *allocpiiu;

    {
        this->defaultMutex.lock ();
        nciu *chan;

        /*
         * ignore search replies for deleted channels
         */
        chan = this->chanTable.lookup ( cid );
        if ( ! chan ) {
            this->defaultMutex.unlock ();
            return;
        }

        retrySeqNumber = chan->getRetrySeqNo ();

        /*
         * Ignore duplicate search replies
         */
        if ( chan->connectionInProgress ( addr ) ) {
            this->defaultMutex.unlock ();
            return;
        }

        allocpiiu = this->constructTCPIIU ( addr, minorVersionNumber );
        if ( ! allocpiiu ) {
            this->defaultMutex.unlock ();
            return;
        }

        /*
         * remove it from the broadcast niiu
         */
        chan->searchReplySetUp ( sid, typeCode, count );
        allocpiiu->installChannelPendingClaim ( *chan );

        this->defaultMutex.unlock ();
    }

    this->notifySearchResponse ( retrySeqNumber );
    return;
}

bool cac::currentThreadIsRecvProcessThread ()
{
    if ( this->pRecvProcThread ) {
        return this->pRecvProcThread->isCurrentThread ();
    }
    else {
        return false;
    }
}
