
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
#include "comQueSend_IL.h"
#include "recvProcessThread_IL.h"
#include "netiiu_IL.h"

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

    this->programBeginTime = epicsTime::getCurrent ();

    status = envGetDoubleConfigParam ( &EPICS_CA_CONN_TMO, &this->connTMO );
    if ( status ) {
        this->connTMO = CA_CONN_VERIFY_PERIOD;
        ca_printf ( "EPICS \"%s\" double fetch failed\n", EPICS_CA_CONN_TMO.name);
        ca_printf ( "Defaulting \"%s\" = %f\n", EPICS_CA_CONN_TMO.name, this->connTMO);
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

        tsDLIterBD <tcpiiu> piiu = this->iiuList.firstIter ();
        while ( piiu.valid () ) {
            tsDLIterBD <tcpiiu> pnext = piiu;
            pnext++;
            {
                epicsAutoMutex autoMutexTmp ( this->defaultMutex );
                piiu->disconnectAllChan ( limboIIU );
            }
            piiu->disconnect ();
            piiu->suicide ();
            piiu = pnext;
        }

        piiu = this->iiuListLimbo.firstIter ();
        while ( piiu.valid () ) {
            tsDLIterBD <tcpiiu> pnext = piiu;
            pnext++;
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

    this->beaconTable.traverse ( &bhe::destroy );

    osiSockRelease ();

    this->pTimerQueue->release ();
}

void cac::processRecvBacklog ()
{
    epicsAutoMutex autoMutex ( this->iiuListMutex );

    tsDLIterBD < tcpiiu > piiu = this->iiuList.firstIter ();
    while ( piiu.valid () ) {
        tsDLIterBD < tcpiiu > pNext = piiu;
        pNext++;

        if ( ! piiu->alive () ) {
            assert ( this->pudpiiu && this->pSearchTmr );

            bhe *pBHE = piiu->getBHE ();
            if ( pBHE ) {
                {
                    epicsAutoMutex autoMutexTmp ( this->defaultMutex );
                    this->beaconTable.remove ( *pBHE );
                }
                pBHE->destroy ();
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
    tsDLIterBD <tcpiiu> piiu = this->iiuList.firstIter ();
    while ( piiu.valid () ) {
        piiu->flush ();
        piiu++;
    }
}

unsigned cac::connectionCount () const
{
    epicsAutoMutex autoMutex ( this->iiuListMutex );
    return this->iiuList.count ();
}

void cac::show ( unsigned level ) const
{
    epicsAutoMutex autoMutex1 ( this->iiuListMutex );
    epicsAutoMutex autoMutex2 ( this->defaultMutex );

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
 *  cac::beaconNotify
 */
void cac::beaconNotify ( const inetAddrID &addr )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
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

    this->flush ();
   
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
    if ( this->pudpiiu ) {
        this->pudpiiu->connectTimeoutNotify ();
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

    this->flush ();

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

void cac::accessRightsNotify ( unsigned id, const caar &ar )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        pChan->accessRightsStateChange ( ar );
    }
}

bool cac::connectChannel ( bool v44Ok, unsigned id, 
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
        return true;
    }
    else {
        return false;
    }
}

// this is to only be used by early protocol revisions
bool cac::connectChannel ( unsigned id )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    nciu * pChan = this->chanTable.lookup ( id );
    if ( pChan ) {
        pChan->connect ();
        return true;
    }
    else {
        return false;
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

void cac::installCASG ( CASG &sg )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    this->sgTable.add ( sg );
}

void cac::uninstallCASG ( CASG &sg )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    this->sgTable.remove ( sg );
}

CASG * cac::lookupCASG ( unsigned id )
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

void cac::exceptionNotify ( int status, const char *pContext,
    const char *pFileName, unsigned lineNo )
{
    ca_signal_with_file_and_lineno ( status, pContext, pFileName, lineNo );
}

void cac::exceptionNotify ( int status, const char *pContext,
    unsigned type, unsigned long count, 
    const char *pFileName, unsigned lineNo )
{
    ca_signal_formated ( status, pFileName, lineNo, "%s type=%d count=%ld\n", 
        pContext, type, count );
}

void cac::registerService ( cacServiceIO &service )
{
    this->services.registerService ( service );
}

cacChannelIO * cac::createChannelIO ( const char *pName, cacChannelNotify &chan )
{
    cacChannelIO *pIO;

    pIO = this->services.createChannelIO ( pName, chan );
    if ( ! pIO ) {
        pIO = cacGlobalServiceList.createChannelIO ( pName, chan );
        if ( ! pIO ) {
            if ( ! this->pudpiiu || ! this->pSearchTmr ) {
                if ( ! this->setupUDP () ) {
                    return 0;
                }
            }
            nciu *pNetChan = new nciu ( *this, limboIIU, chan, pName );
            if ( pNetChan ) {
                if ( ! pNetChan->fullyConstructed () ) {
                    delete static_cast < cacChannelIO * > ( pNetChan );
                    return 0;
                }
                else {
                    return pNetChan;
                }
            }
            else {
                return 0;
            }
        }
    }
    return pIO;
}

void cac::installNetworkChannel ( nciu & chan, netiiu * & piiu )
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    this->chanTable.add ( chan );
    this->pudpiiu->attachChannel ( chan );
    piiu = this->pudpiiu;
    this->pSearchTmr->resetPeriod ( CA_RECAST_DELAY );
}

bool cac::setupUDP ()
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

bool cac::lookupChannelAndTransferToTCP ( unsigned cid, unsigned sid, 
             unsigned typeCode, unsigned long count, 
             unsigned minorVersionNumber, const osiSockAddr &addr )
{
    unsigned  retrySeqNumber;

    if ( addr.sa.sa_family != AF_INET ) {
        return false;
    }

    {
        // lock order is significant here
        epicsAutoMutex autoMutex1 ( this->iiuListMutex );
        epicsAutoMutex autoMutex2 ( this->defaultMutex );
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
        if ( chan->isAttachedToVirtaulCircuit ( addr ) ) {
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
            piiu = iiuListLimbo.get ();
            if ( ! piiu ) {
                piiu = new tcpiiu ( *this, this->connTMO, *this->pTimerQueue );
                if ( ! piiu ) {
                    return true;
                }
            }
            if ( piiu->fullyConstructed () ) {
                this->iiuList.add ( *piiu  );
                if ( ! piiu->initiateConnect ( addr, minorVersionNumber, 
                            *pBHE, this->ipToAEngine ) ) {
                    this->iiuList.remove ( *piiu  );
                    this->iiuListLimbo.add ( *piiu );
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

        // wake up send thread which ultimately sends the claim message
        piiu->flush ();

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
    epicsAutoMutex autoMutex ( this->defaultMutex );
    nciu *pChan = this->chanTable.remove ( chan );
    assert ( pChan = &chan );
    chan.getPIIU ()->detachChannel ( chan );
}

void cac::getFDRegCallback ( CAFDHANDLER *&fdRegFuncOut, void *&fdRegArgOut ) const
{
    epicsAutoMutex autoMutex ( this->defaultMutex );
    if ( this->enablePreemptiveCallback ) {
        fdRegFuncOut = 0;
        fdRegArgOut = 0;
    }
    else {
        fdRegFuncOut = this->fdRegFunc;
        fdRegArgOut = this->fdRegArg;
    }
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



