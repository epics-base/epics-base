
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
#include "inetAddrID_IL.h"
#include "bhe_IL.h"

extern "C" void cacRecursionLockExitHandler ()
{
    if ( cacRecursionLock ) {
        threadPrivateDelete ( cacRecursionLock );
        cacRecursionLock = 0;
    }
}

static void cacInitRecursionLock ( void * dummy )
{
    cacRecursionLock = threadPrivateCreate ();
    if ( cacRecursionLock ) {
        atexit ( cacRecursionLockExitHandler );
    }
}

//
// cac::cac ()
//
cac::cac () :
    beaconTable (1024),
    endOfBCastList (0),
    ioTable (1024),
    chanTable (1024),
    sgTable (128),
    pndrecvcnt (0)
{
	long status;
    static threadOnceId once = OSITHREAD_ONCE_INIT;

    threadOnce ( &once, cacInitRecursionLock, 0);

    if ( cacInitRecursionLock == 0 ) {
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

	if ( ! osiSockAttach () ) {
        throwWithLocation ( caErrorCode (ECA_INTERNAL) );
	}

	ellInit (&this->ca_taskVarList);
	ellInit (&this->putCvrtBuf);
    ellInit (&this->fdInfoFreeList);
    ellInit (&this->fdInfoList);
    this->ca_printf_func = errlogVprintf;
    this->pudpiiu = NULL;
    this->ca_exception_func = ca_default_exception_handler;
    this->ca_exception_arg = NULL;
    this->ca_fd_register_func = NULL;
    this->ca_fd_register_arg = NULL;
    this->ca_number_iiu_in_fc = 0u;
    this->readSeq = 0u;

    this->ca_client_lock = semMutexCreate();
    if (!this->ca_client_lock) {
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }
    this->ca_io_done_sem = semBinaryCreate(semEmpty);
    if (!this->ca_io_done_sem) {
        semMutexDestroy (this->ca_client_lock);
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

    this->ca_blockSem = semBinaryCreate(semEmpty);
    if (!this->ca_blockSem) {
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
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
        len = strlen (tmp) + 1;
        this->ca_pUserName = (char *) malloc ( len );
        if ( ! this->ca_pUserName ) {
            semMutexDestroy (this->ca_client_lock);
            semBinaryDestroy (this->ca_io_done_sem);
            semBinaryDestroy (this->ca_blockSem);
            throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
        }
        strncpy (this->ca_pUserName, tmp, len);
    }

    /* record the host name */
    this->ca_pHostName = localHostName();
    if (!this->ca_pHostName) {
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
        semBinaryDestroy (this->ca_blockSem);
        free (this->ca_pUserName);
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }

    this->programBeginTime = osiTime::getCurrent ();

    status = envGetDoubleConfigParam (&EPICS_CA_CONN_TMO, &this->ca_connectTMO);
    if (status) {
        this->ca_connectTMO = CA_CONN_VERIFY_PERIOD;
        ca_printf (
            "EPICS \"%s\" float fetch failed\n",
            EPICS_CA_CONN_TMO.name);
        ca_printf (
            "Setting \"%s\" = %f\n",
            EPICS_CA_CONN_TMO.name,
            this->ca_connectTMO);
    }

    this->ca_server_port = 
        envGetInetPortConfigParam (&EPICS_CA_SERVER_PORT, CA_SERVER_PORT);

    this->pProcThread = new processThread (this);
    if ( ! this->pProcThread ) {
        semMutexDestroy (this->ca_client_lock);
        semBinaryDestroy (this->ca_io_done_sem);
        semBinaryDestroy (this->ca_blockSem);
        free (this->ca_pUserName);
        free (this->ca_pHostName);
        throwWithLocation ( caErrorCode (ECA_ALLOCMEM) );
    }
}

/*
 * cac::~cac ()
 *
 * releases all resources alloc to a channel access client
 */
cac::~cac ()
{
    //
    // destroy local IO channels
    //
    this->defaultMutex.lock ();
    tsDLIterBD <cacLocalChannelIO> iter ( this->localChanList.first () );
    while ( iter != tsDLIterBD <cacLocalChannelIO> ::eol () ) {
        tsDLIterBD <cacLocalChannelIO> pnext = iter.itemAfter ();
        iter->destroy ();
        iter = pnext;
    }
    this->defaultMutex.unlock ();

    //
    // make certain that process thread isnt deleting 
    // tcpiiu objects at the same that this thread is
    //
    delete this->pProcThread;

    //
    // shutdown all tcp connections and wait for threads to exit
    //
    this->iiuListMutex.lock ();
    tsDLIterBD <tcpiiu> piiu ( this->iiuListIdle.first () );
    while ( piiu != piiu.eol () ) {
        tsDLIterBD <tcpiiu> pnext = piiu.itemAfter ();
        piiu->suicide ();
        piiu = pnext;
    }
    piiu = this->iiuListRecvPending.first ();
    while ( piiu != piiu.eol () ) {
        tsDLIterBD <tcpiiu> pnext = piiu.itemAfter ();
        piiu->suicide ();
        piiu = pnext;
    }
    this->iiuListMutex.unlock ();

    //
    // shutdown udp and wait for threads to exit
    //
    if (this->pudpiiu) {
        delete this->pudpiiu;
    }

    LOCK (this);

    /* remove put convert block free list */
    ellFree (&this->putCvrtBuf);

    /* reclaim sync group resources */
    this->sgTable.destroyAllEntries ();

    /* free select context lists */
    ellFree (&this->fdInfoFreeList);
    ellFree (&this->fdInfoList);

    /*
     * free user name string
     */
    if (this->ca_pUserName) {
        free (this->ca_pUserName);
    }

    /*
     * free host name string
     */
    if (this->ca_pHostName) {
        free (this->ca_pHostName);
    }

    this->beaconTable.destroyAllEntries ();

    semBinaryDestroy (this->ca_io_done_sem);
    semBinaryDestroy (this->ca_blockSem);

    semMutexDestroy (this->ca_client_lock);

    osiSockRelease ();
}

void cac::safeDestroyNMIU (unsigned id)
{
    LOCK (this);

    baseNMIU *pIOBlock = this->ioTable.lookup (id);
    if (pIOBlock) {
        pIOBlock->destroy ();
    }

    UNLOCK (this);
}

void cac::processRecvBacklog ()
{
    tcpiiu *piiu;

    while ( 1 ) {
        int status;
        unsigned bytesToProcess;

        this->iiuListMutex.lock ();
        piiu = this->iiuListRecvPending.get ();
        if ( ! piiu ) {
            this->iiuListMutex.unlock ();
            break;
        }

        piiu->recvPending = false;
        this->iiuListIdle.add (*piiu);
        this->iiuListMutex.unlock ();

        if ( piiu->state == iiu_disconnected ) {
            delete piiu;
            continue;
        }

        char *pProto = (char *) cacRingBufferReadReserveNoBlock 
                            (&piiu->recv, &bytesToProcess);
        while ( pProto ) {
            status = piiu->post_msg (pProto, bytesToProcess);
            if ( status == ECA_NORMAL ) {
                cacRingBufferReadCommit (&piiu->recv, bytesToProcess);
                cacRingBufferReadFlush (&piiu->recv);
            }
            else {
                delete piiu;
            }
            pProto = (char *) cacRingBufferReadReserveNoBlock 
                            (&piiu->recv, &bytesToProcess);
        }
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
    this->iiuListMutex.lock ();
    tsDLIterBD<tcpiiu> piiu ( this->iiuListIdle.first () );
    while ( piiu != tsDLIterBD<tcpiiu>::eol () ) {
        piiu->flush ();
        piiu++;
    }
    piiu = this->iiuListRecvPending.first ();
    while ( piiu != tsDLIterBD<tcpiiu>::eol () ) {
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
    nciu *pchan;

    LOCK (this);

    this->readSeq++;
    this->pndrecvcnt = 0u;

    tsDLIter <nciu> iter ( this->pudpiiu->chidList );
    while ( ( pchan = iter () ) ) {
        pchan->connectTimeoutNotify ();
    }

    UNLOCK (this);
}

unsigned cac::connectionCount () const
{
    unsigned count;

    this->iiuListMutex.lock ();
    count = this->iiuListIdle.count () + this->iiuListRecvPending.count ();
    this->iiuListMutex.unlock ();
    return count;
}

void cac::show (unsigned level) const
{
	LOCK (this);
    if (this->pudpiiu) {
        this->pudpiiu->show (level);
    }

    this->iiuListMutex.lock ();

    tsDLIterConstBD <tcpiiu> piiu ( this->iiuListIdle.first () );
    while ( piiu != piiu.eol () ) {
        piiu->show (level);
        piiu++;
	}

    piiu = this->iiuListRecvPending.first ();
    while ( piiu != piiu.eol () ) {
        piiu->show (level);
        piiu++;
	}

    this->iiuListMutex.unlock ();

    UNLOCK (this);
}

void cac::installIIU (tcpiiu &iiu)
{
    this->iiuListMutex.lock ();
    iiu.recvPending = false;
    this->iiuListIdle.add (iiu);
    this->iiuListMutex.unlock ();

    if (this->ca_fd_register_func) {
        ( * this->ca_fd_register_func ) ( (void *) this->ca_fd_register_arg, iiu.sock, TRUE);
    }
}

void cac::signalRecvActivityIIU (tcpiiu &iiu)
{
    bool change;

    this->iiuListMutex.lock ();

    if ( iiu.recvPending ) {
        change = false;
    }
    else {
        this->iiuListIdle.remove (iiu);
        this->iiuListRecvPending.add (iiu);
        iiu.recvPending = true;
        change = true;
    }

    this->iiuListMutex.unlock ();

    //
    // wakeup after unlock improves performance
    //
    if (change) {
        this->recvActivity.signal ();
    }
}

void cac::removeIIU (tcpiiu &iiu)
{
    this->iiuListMutex.lock ();

    if (iiu.recvPending) {
        this->iiuListRecvPending.remove (iiu);
    }
    else {
        this->iiuListIdle.remove (iiu);
    }

    this->iiuListMutex.unlock ();
}

/*
 * cac::lookupBeaconInetAddr()
 *
 * LOCK must be applied
 */
bhe * cac::lookupBeaconInetAddr (const inetAddrID &ina)
{
    bhe *pBHE;
    LOCK (this);
    pBHE = this->beaconTable.lookup (ina);
    UNLOCK (this);
    return pBHE;
}

/*
 * cac::createBeaconHashEntry ()
 */
bhe *cac::createBeaconHashEntry (const inetAddrID &ina, const osiTime &initialTimeStamp)
{
    bhe *pBHE;

    LOCK (this);

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

    UNLOCK (this);

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

    LOCK ( this );
    /*
     * look for it in the hash table
     */
    pBHE = this->lookupBeaconInetAddr ( addr );
    if (pBHE) {

        netChange = pBHE->updateBeaconPeriod (this->programBeginTime);

        /*
         * update state of health for active virtual circuits 
         * (only if we are not suspicious about past beacon changes
         * until the next echo reply)
         */
        tcpiiu *piiu = pBHE->getIIU ();
        if ( piiu ) {
            if ( ! piiu->beaconAnomaly ) {
                piiu->rescheduleRecvTimer (); // reset connection activity watchdog
            }
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
        netChange = FALSE;
        this->createBeaconHashEntry (addr, osiTime::getCurrent () );
    }

    if ( ! netChange ) {
        UNLOCK (this);
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
        struct sockaddr_in  saddr;
        int                 saddr_length = sizeof(saddr);
        int                 status;

        status = getsockname ( this->pudpiiu->sock, (struct sockaddr *) &saddr,
                        &saddr_length);
        assert ( status >= 0 );
        port = ntohs ( saddr.sin_port );
    }

    {
        ca_real     delay;

        delay = (port&CA_RECAST_PORT_MASK);
        delay /= MSEC_PER_SEC;
        delay += CA_RECAST_DELAY;

        this->pudpiiu->searchTmr.reset ( delay );
    }

    /*
     * set retry count of all disconnected channels
     * to zero
     */
    tsDLIterBD <nciu> iter ( this->pudpiiu->chidList.first () );
    while ( iter != tsDLIterBD<nciu>::eol () ) {
        iter->retry = 0u;
        iter++;
    }

    UNLOCK ( this );

#   if DEBUG
    {
        char buf[64];
        ipAddrToA (pnet_addr, buf, sizeof (buf) );
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

    LOCK (this);
    pBHE = this->beaconTable.remove ( ina );
    UNLOCK (this);

    assert (pBHE);
}

void cac::decrementOutstandingIO (unsigned seqNumber)
{
    LOCK (this);
    if ( this->readSeq == seqNumber ) {
        if ( this->pndrecvcnt > 0u ) {
            this->pndrecvcnt--;
        }
    }
    UNLOCK (this);
    if ( this->pndrecvcnt == 0u ) {
        semBinaryGive (this->ca_io_done_sem);
    }
}

void cac::decrementOutstandingIO ()
{
    LOCK (this);
    if ( this->pndrecvcnt > 0u ) {
        this->pndrecvcnt--;
    }
    UNLOCK (this);
    if ( this->pndrecvcnt == 0u ) {
        semBinaryGive (this->ca_io_done_sem);
    }
}

void cac::incrementOutstandingIO ()
{
    LOCK (this);
    if ( this->pndrecvcnt < UINT_MAX ) {
        this->pndrecvcnt++;
    }
    UNLOCK (this);
}

unsigned cac::readSequence () const
{
    return this->readSeq;
}

int cac::pend (double timeout, int early)
{
    int status;
    void *p;

    /*
     * dont allow recursion
     */
    p = threadPrivateGet (cacRecursionLock);
    if (p) {
        return ECA_EVDISALLOW;
    }

    threadPrivateSet (cacRecursionLock, &cacRecursionLock);

    status = this->pendPrivate (timeout, early);

    threadPrivateSet (cacRecursionLock, NULL);

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
            remaining = min ( 60.0, remaining );

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
        
        /* recv occurs in another thread */
        semBinaryTakeTimeout ( this->ca_io_done_sem, remaining );

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

void cac::installIO ( baseNMIU &io )
{
    LOCK (this);
    this->ioTable.add ( io );
    UNLOCK (this);
}

void cac::uninstallIO ( baseNMIU &io )
{
    LOCK (this);
    this->ioTable.remove ( io );
    UNLOCK (this);
}

baseNMIU * cac::lookupIO (unsigned id)
{
    LOCK (this);
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    UNLOCK (this);
    return pmiu;
}

void cac::installChannel (nciu &chan)
{
    LOCK (this);
    this->chanTable.add ( chan );
    UNLOCK (this);
}

void cac::uninstallChannel (nciu &chan)
{
    LOCK (this);
    this->chanTable.remove ( chan );
    UNLOCK (this);
}

nciu * cac::lookupChan (unsigned id)
{
    LOCK (this);
    nciu * pchan = this->chanTable.lookup ( id );
    UNLOCK (this);
    return pchan;
}

void cac::installCASG (CASG &sg)
{
    LOCK (this);
    this->sgTable.add ( sg );
    UNLOCK (this);
}

void cac::uninstallCASG (CASG &sg)
{
    LOCK (this);
    this->sgTable.remove ( sg );
    UNLOCK (this);
}

CASG * cac::lookupCASG (unsigned id)
{
    LOCK (this);
    CASG * psg = this->sgTable.lookup ( id );
    if ( psg ) {
        if ( ! psg->verify () ) {
            psg = 0;
        }
    }
    UNLOCK (this);
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
                this->pudpiiu = new udpiiu ( this );
                if ( ! this->pudpiiu ) {
                    return false;
                }
            }
            nciu *pNetChan = new nciu ( this, chan, pName );
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

void cac::lock () const
{
    semMutexMustTake ( this->ca_client_lock );
}

void cac::unlock () const
{
    semMutexGive ( this->ca_client_lock );
}
