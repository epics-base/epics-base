
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
#include "ioCounter_IL.h"
#include "comQueSend_IL.h"
#include "recvProcessThread_IL.h"

extern "C" void cacRecursionLockExitHandler ()
{
    if ( cacRecursionLock ) {
        epicsThreadPrivateDelete ( cacRecursionLock );
        cacRecursionLock = 0;
    }
}

extern "C" void cacInitRecursionLock ( void * )
{
    cacRecursionLock = epicsThreadPrivateCreate ();
    if ( cacRecursionLock ) {
        atexit ( cacRecursionLockExitHandler );
    }
}

//
// cac::cac ()
//
cac::cac ( bool enablePreemptiveCallbackIn ) :
    ipToAEngine ( "caIPAddrToAsciiEngine" ), 
    chanTable ( 1024 ),
    sgTable ( 128 ),
    beaconTable ( 1024 ),
    fdRegFunc ( 0 ),
    fdRegArg ( 0 ),
    pudpiiu ( 0 ),
    pSearchTmr ( 0 ),
    pRepeaterSubscribeTmr ( 0 ),
    initializingThreadsPriority ( epicsThreadGetPrioritySelf () ),
    enablePreemptiveCallback ( enablePreemptiveCallbackIn )
{
	long status;
    static epicsThreadOnceId once = EPICS_THREAD_ONCE_INIT;
    unsigned abovePriority;

    epicsThreadOnce ( &once, cacInitRecursionLock, 0 );

    if ( cacRecursionLock == 0 ) {
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

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

    this->pTimerQueue = new osiTimerQueue ( osiTimerQueue::mtsCreateManagerThread, abovePriority );
    if ( ! this->pTimerQueue ) {
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

    this->pVPrintfFunc = errlogVprintf;
    this->ca_exception_func = ca_default_exception_handler;
    this->ca_exception_arg = NULL;

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


cac::~cac ()
{
    this->enableCallbackPreemption ();

    //
    // destroy local IO channels
    //
    {
        epicsAutoMutex autoMutex ( this->defaultMutex );
        tsDLIterBD < cacLocalChannelIO > lclChan ( this->localChanList.first () );
        while ( lclChan.valid () ) {
            tsDLIterBD < cacLocalChannelIO > pnext = lclChan.itemAfter ();
            lclChan->destroy ();
            lclChan = pnext;
        }
    }

    //
    // make certain that process thread isnt deleting 
    // tcpiiu objects at the same that this thread is
    //
    if ( this->pRecvProcThread ) {
        this->pRecvProcThread->disable ();
    }

    if ( this->pudpiiu ) {
        // this blocks until the UDP thread exits so that
        // it will not sneak in any new clients
        this->pudpiiu->shutdown ();
    }

    //
    // shutdown all tcp connections and wait for threads to exit
    //
    {
        epicsAutoMutex autoMutex ( this->iiuListMutex );

        tsDLIterBD <tcpiiu> piiu ( this->iiuList.first () );
        while ( piiu.valid () ) {
            tsDLIterBD <tcpiiu> pnext = piiu.itemAfter ();
            {
                epicsAutoMutex autoMutexTmp ( this->defaultMutex );
                piiu->disconnectAllChan ( *this->pudpiiu );
            }
            piiu->disconnect ();
            piiu->suicide ();
            piiu = pnext;
        }

        piiu = this->iiuListLimbo.first ();
        while ( piiu.valid () ) {
            tsDLIterBD <tcpiiu> pnext = piiu.itemAfter ();
            piiu->suicide ();
            piiu = pnext;
        }
    }

    if ( this->pRecvProcThread ) {
        delete this->pRecvProcThread;
    }

    if ( this->pRepeaterSubscribeTmr ) {
        delete this->pRepeaterSubscribeTmr;
    }

    if ( this->pSearchTmr ) {
        delete this->pSearchTmr;
    }

    if ( this->pudpiiu ) {
        if ( ! this->enablePreemptiveCallback ) {
            if ( this->fdRegFunc ) {
                ( *this->fdRegFunc ) 
                    ( this->fdRegArg, this->pudpiiu->getSock (), FALSE );
            }
        }
        //
        // make certain that the UDP thread isnt starting 
        // up  new clients. this adds an additional 
        // requirement that threads 
        //
        {
            epicsAutoMutex autoMutex ( this->defaultMutex );
            this->pudpiiu->disconnectAllChan ( limboIIU );
        }
        delete this->pudpiiu;
    }

    //
    // free user name string
    //
    if ( this->pUserName ) {
        delete [] this->pUserName;
    }
    this->sgTable.traverse ( &CASG::destroy );
    this->beaconTable.traverse ( &bhe::destroy );
    this->chanTable.traverse ( &nciu::destroy );

    osiSockRelease ();

    delete this->pTimerQueue;
}

void cac::processRecvBacklog ()
{
    epicsAutoMutex autoMutex ( this->iiuListMutex );

    tsDLIterBD < tcpiiu > piiu ( this->iiuList.first () );
    while ( piiu.valid () ) {
        tsDLIterBD < tcpiiu > pNext = piiu.itemAfter ();

        if ( ! piiu->alive () ) {
            assert ( this->pudpiiu && this->pSearchTmr );

            bhe *pBHE = piiu->getBHE ();

            if ( ! this->enablePreemptiveCallback ) {
                if ( this->fdRegFunc ) {
                    ( *this->fdRegFunc ) 
                        ( (void *) this->fdRegArg, piiu->getSock (), FALSE );
                }
            }

            if ( piiu->channelCount () ) {
                char hostNameTmp[64];
                piiu->hostName ( hostNameTmp, sizeof ( hostNameTmp ) );
                genLocalExcep ( *this, ECA_DISCONN, hostNameTmp );
            }

            {
                epicsAutoMutex autoMutexTmp ( this->defaultMutex );
                piiu->disconnectAllChan ( *this->pudpiiu );
            }

            piiu->disconnect ();

            this->iiuList.remove ( *piiu );
            this->iiuListLimbo.add ( *piiu );

            if ( pBHE ) {
                this->beaconTable.remove ( *pBHE );
                pBHE->destroy ();
            }

            this->pSearchTmr->resetPeriod ( CA_RECAST_DELAY );
        }
        else {
            piiu->processIncoming ();
        }

        piiu = pNext;
	}
}

/*
 * cac::flush ()
 */
void cac::flush ()
{
    /*
     * set the push pending flag on all virtual circuits
     */
    epicsAutoMutex autoMutex ( this->iiuListMutex );
    tsDLIterBD <tcpiiu> piiu ( this->iiuList.first () );
    while ( piiu.valid () ) {
        piiu->flush ();
        piiu++;
    }
}

unsigned cac::connectionCount () const
{
    return this->iiuList.count ();
}

void cac::show ( unsigned level ) const
{
    ::printf ( "Channel Access Client Context at %p for user %s\n", 
        this, this->pUserName );
    if ( level > 0u ) {
        {
            epicsAutoMutex autoMutex ( this->iiuListMutex );

            tsDLIterConstBD < tcpiiu > piiu ( this->iiuList.first () );
            while ( piiu.valid () ) {
                piiu->show ( level - 1u );
                piiu++;
	        }

            tsDLIterConstBD < cacLocalChannelIO > pChan ( this->localChanList.first () );
            while ( pChan.valid () ) {
                pChan->show ( level - 1u );
                pChan++;
	        }
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
        ::printf ( "\texception function %p, exception arg %p\n",
                this->ca_exception_func, this->ca_exception_arg );
        ::printf ( "\tCA printf function %p\n",
                this->pVPrintfFunc);
        ::printf ( "\tfile descriptor registration function %p, file descriptor registration arg %p\n",
            this->fdRegFunc, this->fdRegArg );
        this->showOutstandingIO ( level - 2u );
    }

    if ( level > 2u ) {
        ::printf ( "Program begin time:\n");
        this->programBeginTime.show ( level - 3u );
        ::printf ( "Channel identifier hash table:\n" );
        this->chanTable.show ( level - 3u );
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
        this->defaultMutex.show ( level - 4u );
        ::printf ( "Virtual circuit list mutex:\n");
        this->iiuListMutex.show ( level - 4u );
    }
}

void cac::signalRecvActivity ()
{
    if ( this->pRecvProcThread ) {
        this->pRecvProcThread->signalActivity ();
    }
}

/*
 * cac::lookupBeaconInetAddr()
 */
bhe * cac::lookupBeaconInetAddr (const inetAddrID &ina)
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    bhe *pBHE;
    pBHE = this->beaconTable.lookup (ina);
    return pBHE;
}

/*
 * cac::createBeaconHashEntry ()
 */
bhe *cac::createBeaconHashEntry (const inetAddrID &ina, const osiTime &initialTimeStamp)
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    bhe *pBHE;

    pBHE = this->beaconTable.lookup ( ina );
    if ( !pBHE ) {
        pBHE = new bhe ( initialTimeStamp, ina );
        if ( pBHE ) {
            if ( this->beaconTable.add ( *pBHE ) < 0 ) {
                pBHE->destroy ();
                pBHE = 0;
            }
        }
    }

    return pBHE;
}

/*
 *  cac::beaconNotify
 */
void cac::beaconNotify ( const inetAddrID &addr )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    bhe *pBHE;
    unsigned port;  
    int netChange;

    if ( ! this->pudpiiu ) {
        return;
    }

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
            this->printf ( "CAC: getsockname () error was \"%s\"\n", SOCKERRSTR (SOCKERRNO) );
            return;
        }
        port = ntohs ( saddr.sin_port );
    }

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

int cac::pend ( double timeout, int early )
{
    int status;
    void *p;

    /*
     * dont allow recursion
     */
    p = epicsThreadPrivateGet ( cacRecursionLock );
    if (p) {
        return ECA_EVDISALLOW;
    }

    epicsThreadPrivateSet ( cacRecursionLock, &cacRecursionLock );

    this->enableCallbackPreemption ();

    status = this->pendPrivate ( timeout, early );

    this->disableCallbackPreemption ();

    epicsThreadPrivateSet ( cacRecursionLock, NULL );

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

    if ( this->currentOutstandingIOCount () == 0u && early ) {
        return ECA_NORMAL;
    }
   
    if ( timeout < 0.0 ) {
        if ( early ) {
            this->cleanUpOutstandingIO ();
            if ( this->pudpiiu ) {
                this->pudpiiu->connectTimeoutNotify ();
            }
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
                    this->cleanUpOutstandingIO ();
                    if ( this->pudpiiu ) {
                        this->pudpiiu->connectTimeoutNotify ();
                    }
                }
                return ECA_TIMEOUT;
            }
        }    
        
        this->waitForCompletionOfIO ( remaining );

        if ( this->currentOutstandingIOCount () == 0 && early ) {
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
    if ( this->currentOutstandingIOCount () == 0u ) {
        return true;
    }
    else{
        return false;
    }
}

void cac::accessRightsNotify ( unsigned id, caar ar )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        pChan->accessRightsStateChange ( ar );
    }
}

void cac::connectChannel ( bool v44Ok, unsigned id, 
          unsigned nativeType, unsigned long nativeCount, unsigned sid )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        unsigned sidTmp;
        if ( v44Ok ) {
            sidTmp = sid;
        }
        else {
            sidTmp = pChan->getSID ();
        }
        pChan->connect ( nativeType, nativeCount, sidTmp );
    }
}

// this is to only be used by early protocol revisions
void cac::connectChannel ( unsigned id )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        pChan->connect ();
    }
}

void cac::channelDestroy ( unsigned id )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    nciu * pChan = this->chanTable.lookup ( id );
    // channel should already have been deleted
    if ( pChan ) {
        epicsPrintf ( "cac: received invalid channel delete verification?\n" );
    }
}

void cac::disconnectChannel ( unsigned id )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        assert ( this->pudpiiu && this->pSearchTmr );
        pChan->disconnect ( *this->pudpiiu );
        this->pSearchTmr->resetPeriod ( CA_RECAST_DELAY );
    }
}

void cac::installCASG (CASG &sg)
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    this->sgTable.add ( sg );
}

void cac::uninstallCASG (CASG &sg)
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    this->sgTable.remove ( sg );
}

CASG * cac::lookupCASG (unsigned id)
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    CASG * psg = this->sgTable.lookup ( id );
    if ( psg ) {
        if ( ! psg->verify () ) {
            psg = 0;
        }
    }
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

    pIO = this->services.createChannelIO ( pName, *this, chan );
    if ( ! pIO ) {
        pIO = cacGlobalServiceList.createChannelIO ( pName, *this, chan );
        if ( ! pIO ) {
            if ( ! this->pudpiiu || ! this->pSearchTmr ) {
                if ( ! this->setupUDP () ) {
                    return false;
                }
            }
            nciu *pNetChan = new nciu ( *this, *this->pudpiiu, chan, pName );
            if ( pNetChan ) {
                if ( ! pNetChan->fullyConstructed () ) {
                    pNetChan->destroy ();
                    return false;
                }
                else {
                    epicsAutoMutex autoMutex ( this->defaultMutex );
                    chan.attachIO ( *pNetChan );
                    this->chanTable.add ( *pNetChan );
                    this->pudpiiu->attachChannel ( *pNetChan );
                    this->pSearchTmr->resetPeriod ( CA_RECAST_DELAY );
                    return true;
                }
            }
            else {
                return false;
            }
        }
    }
    {
        epicsAutoMutex autoMutex ( this->defaultMutex );
        this->localChanList.add ( *pIO );
    }
    return true;
}

void cac::uninstallLocalChannel ( cacLocalChannelIO &localIO )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    this->localChanList.remove ( localIO );
}

bool cac::setupUDP ()
{
    {
        epicsAutoMutex autoMutex ( this->defaultMutex );

        if ( this->pudpiiu ) {
            return true;
        }

        this->pudpiiu = new udpiiu ( *this );
        if ( ! this->pudpiiu ) {
            return false;
        }

        this->pSearchTmr = new searchTimer ( *this->pudpiiu, *this->pTimerQueue );
        if ( ! this->pSearchTmr ) {
            delete this->pudpiiu;
            this->pudpiiu = 0;
            return false;
        }

        this->pRepeaterSubscribeTmr = new repeaterSubscribeTimer ( *this->pudpiiu, *this->pTimerQueue );
        if ( ! this->pRepeaterSubscribeTmr ) {
            delete this->pSearchTmr;
            delete this->pudpiiu;
            this->pudpiiu = 0;
            return false;
        }
    }

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
    epicsAutoMutex autoMutex ( this->defaultMutex );
    this->fdRegFunc = pFunc;
    this->fdRegArg = pArg;
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
    epicsAutoMutex autoMutex ( this->defaultMutex );
    if ( pfunc ) {
        this->ca_exception_func = pfunc;
        this->ca_exception_arg = arg;
    }
    else {
        this->ca_exception_func = ca_default_exception_handler;
        this->ca_exception_arg = NULL;
    }
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

        {
            epicsAutoMutex autoMutex ( this->defaultMutex );
            pExceptionFunc = this->ca_exception_func;
            args.usr = this->ca_exception_arg;
        }

        (*pExceptionFunc) (args);
    }
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
    epicsAutoMutex autoMutex ( this->defaultMutex );
    if ( ca_printf_func ) {
        this->pVPrintfFunc = ca_printf_func;
    }
    else {
        this->pVPrintfFunc = epicsVprintf;
    }
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
    {
        epicsAutoMutex autoMutex ( this->defaultMutex );
        pBHE = this->lookupBeaconInetAddr ( addr.ia );
        if ( ! pBHE ) {
            pBHE = this->createBeaconHashEntry ( addr.ia, osiTime () );
            if ( ! pBHE ) {
                return NULL;
            }
        }
    
        piiu = pBHE->getIIU ();
        if ( piiu ) {
            if ( piiu->alive () ) {
                return piiu;
            }
            else {
                return NULL;
            }
        }
    }

    {
        epicsAutoMutex autoMutex ( this->iiuListMutex );
        piiu = iiuListLimbo.get ();
    }

    if ( ! piiu ) {
        piiu = new tcpiiu ( *this, this->connTMO, *this->pTimerQueue );
        if ( ! piiu ) {
            return NULL;
        }
    }

    if ( piiu->fullyConstructed () ) {
        {
            epicsAutoMutex autoMutex ( this->iiuListMutex );
            this->iiuList.add ( *piiu  );
        }
        if ( ! piiu->initiateConnect ( addr, minorVersion, *pBHE, this->ipToAEngine ) ) {
            epicsAutoMutex autoMutex ( this->iiuListMutex );
            this->iiuList.remove ( *piiu  );
            this->iiuListLimbo.add ( *piiu );
            return NULL;
        }

        {
            epicsAutoMutex autoMutex ( this->defaultMutex );
            if ( ! this->enablePreemptiveCallback && this->fdRegFunc ) {
                ( * this->fdRegFunc ) 
                    ( (void *) this->fdRegArg, piiu->getSock (), TRUE );
            }
        }

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
        epicsAutoMutex autoMutex ( this->defaultMutex );
        nciu *chan;

        /*
         * ignore search replies for deleted channels
         */
        chan = this->chanTable.lookup ( cid );
        if ( ! chan ) {
            return;
        }

        retrySeqNumber = chan->getRetrySeqNo ();

        /*
         * Ignore duplicate search replies
         */
        if ( chan->isAttachedToVirtaulCircuit ( addr ) ) {
            return;
        }

        allocpiiu = this->constructTCPIIU ( addr, minorVersionNumber );
        if ( ! allocpiiu ) {
            return;
        }

        this->pudpiiu->detachChannel ( *chan );
        chan->searchReplySetUp ( *allocpiiu, sid, typeCode, count );
        allocpiiu->attachChannel ( *chan );

        chan->createChannelRequest ();

        // wake up send thread which ultimately sends the claim message
        allocpiiu->flush ();

        if ( ! allocpiiu->ca_v42_ok () ) {
            chan->connect ();
        }
    }

    this->notifySearchResponse ( retrySeqNumber );
    return;
}

void cac::destroyNCIU ( nciu & chan )
{
    {
        epicsAutoMutex autoMutex ( this->defaultMutex );
        nciu *pChan = this->chanTable.remove ( chan );
        assert ( pChan = &chan );
        chan.getPIIU ()->detachChannel ( chan );
    }
    chan.cacDestroy ();
}


