/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/


/* * $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#ifdef _MSC_VER
#   pragma warning(disable:4355)
#endif

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#define epicsExportSharedSymbols
#include "localHostName.h"
#include "iocinf.h"
#include "virtualCircuit.h"
#include "inetAddrID.h"
#include "cac.h"
#include "netiiu.h"
#include "hostNameCache.h"
#include "net_convert.h"
#include "bhe.h"

const unsigned mSecPerSec = 1000u;
const unsigned uSecPerSec = 1000u * mSecPerSec;

tcpSendThread::tcpSendThread ( class tcpiiu & iiuIn,
    const char * pName, unsigned stackSize, unsigned priority ) :
        iiu ( iiuIn ), thread ( *this, pName, stackSize, priority )
{
}

tcpSendThread::~tcpSendThread ()
{
}

void tcpSendThread::start ()
{
    this->thread.start ();
}

void tcpSendThread::exitWait ()
{
    this->thread.exitWait ();
}

void tcpSendThread::run ()
{
    try {
        epicsSocketEnableInterruptedSystemCall ();

        while ( true ) {
            bool flowControlLaborNeeded;
            bool echoLaborNeeded;

            this->iiu.sendThreadFlushEvent.wait ();

            if ( this->iiu.state != tcpiiu::iiucs_connected ) {
                break;
            }

            {
                epicsGuard < cacMutex > guard ( this->iiu.cacRef.mutexRef() );
                flowControlLaborNeeded = 
                    this->iiu.busyStateDetected != this->iiu.flowControlActive;
                echoLaborNeeded = this->iiu.echoRequestPending;
                this->iiu.echoRequestPending = false;
 
                if ( flowControlLaborNeeded ) {
                    if ( this->iiu.flowControlActive ) {
                        this->iiu.disableFlowControlRequest ( guard );
                        this->iiu.flowControlActive = false;
                        debugPrintf ( ( "fc off\n" ) );
                    }
                    else {
                        this->iiu.enableFlowControlRequest ( guard );
                        this->iiu.flowControlActive = true;
                        debugPrintf ( ( "fc on\n" ) );
                    }
                }

                if ( echoLaborNeeded ) {
                    if ( CA_V43 ( this->iiu.minorProtocolVersion ) ) {
                        this->iiu.echoRequest ( guard );
                    }
                    else {
                        this->iiu.versionMessage ( guard, this->iiu.priority() );
                    }
                }
            }

            if ( ! this->iiu.flush () ) {
                break;
            }
        }
        if ( this->iiu.state == tcpiiu::iiucs_clean_shutdown ) {
            this->iiu.flush ();
        }
    }
    catch ( ... ) {
        this->iiu.printf (
            "cac: tcp send thread received an unexpected exception - disconnecting\n");
    }

    this->iiu.sendDog.cancel ();

    this->iiu.cacRef.initiateAbortShutdown ( this->iiu );

    // wakeup user threads blocking for send backlog to be reduced
    // and wait for them to stop using this IIU
    this->iiu.flushBlockEvent.signal ();
    while ( this->iiu.blockingForFlush ) {
        epicsThreadSleep ( 0.1 );
    }

    this->iiu.recvThread.exitWait ();

    this->thread.exitWaitRelease ();

    this->iiu.cacRef.uninstallIIU ( this->iiu );

    delete & this->iiu;
}

unsigned tcpiiu::sendBytes ( const void *pBuf, 
                            unsigned nBytesInBuf ) epicsThrows (())
{
    int status;
    unsigned nBytes = 0u;

    if ( this->state != iiucs_connected && 
            this->state != iiucs_clean_shutdown ) {
        return 0u;
    }

    assert ( nBytesInBuf <= INT_MAX );

    this->sendDog.start ();

    while ( true ) {
        status = ::send ( this->sock, 
            static_cast < const char * > (pBuf), (int) nBytesInBuf, 0 );
        if ( status > 0 ) {
            nBytes = static_cast <unsigned> ( status );
            break;
        }
        else {
            int localError = SOCKERRNO;

            // winsock indicates disconnect by returniing zero here
            if ( status == 0 ) {
                this->cacRef.disconnectNotify ( *this );
                nBytes = 0u;
                break;
            }

            if ( localError == SOCK_SHUTDOWN ) {
                this->cacRef.disconnectNotify ( *this );
                nBytes = 0u;
                break;
            }

            if ( localError == SOCK_EINTR ) {
                continue;
            }

            if ( 
                    localError != SOCK_EPIPE && 
                    localError != SOCK_ECONNRESET &&
                    localError != SOCK_ETIMEDOUT && 
                    localError != SOCK_ECONNABORTED ) {
                this->cacRef.printf ( "CAC: unexpected TCP send error: %s\n", 
                    SOCKERRSTR ( localError ) );
            }

            this->cacRef.disconnectNotify ( *this );
            nBytes = 0u;
            break;
        }
    }

    this->sendDog.cancel ();

    return nBytes;
}

unsigned tcpiiu::recvBytes ( void * pBuf, unsigned nBytesInBuf ) epicsThrows (())
{
    if ( this->state != iiucs_connected && 
            this->state != iiucs_clean_shutdown ) {
        return 0u;
    }

    assert ( nBytesInBuf <= INT_MAX );

    int status = ::recv ( this->sock, static_cast <char *> ( pBuf ), 
        static_cast <int> ( nBytesInBuf ), 0 );

    // if the circuit was aborted then supress warning message about
    // bad file descriptor
    if ( this->state != iiucs_connected && 
            this->state != iiucs_clean_shutdown ) {
        return 0u;
    } 

    if ( status <= 0 ) {
        int localErrno = SOCKERRNO;

        if ( status == 0 ) {
            this->cacRef.disconnectNotify ( *this );
            return 0u;
        }

        if ( localErrno == SOCK_SHUTDOWN ) {
            this->cacRef.disconnectNotify ( *this );
            return 0u;
        }

        if ( localErrno == SOCK_EINTR ) {
            return 0u;
        }
        
        if ( localErrno == SOCK_ECONNABORTED ) {
            this->cacRef.disconnectNotify ( *this );
            return 0u;
        }

        if ( localErrno == SOCK_ECONNRESET ) {
            this->cacRef.disconnectNotify ( *this );
            return 0u;
        }

        {
            char name[64];
            this->hostName ( name, sizeof ( name ) );
            this->printf ( "Unexpected problem with circuit to CA server \"%s\" was \"%s\" - disconnecting\n", 
                name, SOCKERRSTR ( localErrno ) );
        }

        this->cacRef.initiateAbortShutdown ( *this );

        return 0u;
    }
    
    assert ( static_cast <unsigned> ( status ) <= nBytesInBuf );
    return static_cast <unsigned> ( status );
}

tcpRecvThread::tcpRecvThread ( class tcpiiu & iiuIn, class callbackMutex & cbMutexIn,
        const char * pName, unsigned int stackSize, unsigned int priority  ) :
    thread ( *this, pName, stackSize, priority ),
        iiu ( iiuIn ), cbMutex ( cbMutexIn ) {}

tcpRecvThread::~tcpRecvThread ()
{
}

void tcpRecvThread::start ()
{
    this->thread.start ();
}

void tcpRecvThread::exitWait ()
{
    this->thread.exitWait ();
}

void tcpRecvThread::run ()
{
    try {
        epicsSocketEnableInterruptedSystemCall ();

        this->iiu.cacRef.attachToClientCtx ();

        epicsThreadPrivateSet ( caClientCallbackThreadId, &this->iiu );

        this->iiu.connect ();

        this->iiu.sendThread.start ();

        if ( this->iiu.state != tcpiiu::iiucs_connected ) {
            this->iiu.cacRef.disconnectNotify ( this->iiu );
            return;
        }

        comBuf * pComBuf = new ( this->iiu.comBufMemMgr ) comBuf;
        while ( this->iiu.state == tcpiiu::iiucs_connected ||
                this->iiu.state == tcpiiu::iiucs_clean_shutdown ) {

            //
            // We leave the bytes pending and fetch them after
            // callbacks are enabled when running in the old preemptive 
            // call back disabled mode so that asynchronous wakeup via
            // file manager call backs works correctly. This does not 
            // appear to impact performance.
            //
            unsigned nBytesIn;
            if ( this->iiu.cacRef.preemptiveCallbakIsEnabled() ) {
                nBytesIn = pComBuf->fillFromWire ( this->iiu );
                if ( nBytesIn == 0u ) {
                    continue;
                }
            }
            else {
                this->iiu.blockUntilBytesArePendingInOS();
                nBytesIn = 0u;
            }
 
            if ( this->iiu.state != tcpiiu::iiucs_connected &&
                this->iiu.state != tcpiiu::iiucs_clean_shutdown ) {
                break;
            }

            // reschedule connection activity watchdog
            // but dont hold the lock for fear of deadlocking 
            // because cancel is blocking for the completion
            // of the recvDog expire which takes the lock
            // - it take also the callback lock
            this->iiu.recvDog.messageArrivalNotify (); 

            // only one recv thread at a time may call callbacks
            // - pendEvent() blocks until threads waiting for
            // this lock get a chance to run
            epicsGuard < callbackMutex > guard ( this->cbMutex );

            if ( ! this->iiu.cacRef.preemptiveCallbakIsEnabled() ) {
                nBytesIn = pComBuf->fillFromWire ( this->iiu );
                if ( nBytesIn == 0u ) {
                    // outer loop checks to see if state is connected
                    // ( properly set by fillFromWire() )
                    continue;
                }
            }

            if ( this->iiu.state != tcpiiu::iiucs_connected &&
                this->iiu.state != tcpiiu::iiucs_clean_shutdown ) {
                break;
            }

            // force the receive watchdog to be reset every 5 frames
            unsigned contiguousFrameCount = 0;
            while ( true ) {
                if ( nBytesIn == pComBuf->capacityBytes () ) {
                    if ( this->iiu.contigRecvMsgCount >= 
                        contiguousMsgCountWhichTriggersFlowControl ) {
                        this->iiu.busyStateDetected = true;
                    }
                    else { 
                        this->iiu.contigRecvMsgCount++;
                    }
                }
                else {
                    this->iiu.contigRecvMsgCount = 0u;
                    this->iiu.busyStateDetected = false;
                }         
                this->iiu.unacknowledgedSendBytes = 0u;

                this->iiu.recvQue.pushLastComBufReceived ( *pComBuf );
                pComBuf = new ( this->iiu.comBufMemMgr ) comBuf;

                // execute receive labor
                bool protocolOK = this->iiu.processIncoming ( guard );
                if ( ! protocolOK ) {
                    this->iiu.cacRef.initiateAbortShutdown ( this->iiu );
                    break;
                }

                if ( ! this->iiu.bytesArePendingInOS () ) {
                    break;
                }

                if ( ++contiguousFrameCount >= 5 ) {
                    break;
                }

                nBytesIn = pComBuf->fillFromWire ( this->iiu );
                if ( nBytesIn == 0u ) {
                    // outer loop checks to see if state is connected
                    // ( properly set by fillFromWire() )
                    break;
                }
            }
        }

        if ( pComBuf ) {
            pComBuf->~comBuf ();
            this->iiu.comBufMemMgr.release ( pComBuf );
        }
    }
    catch ( ... ) {
        errlogPrintf ( "cac tcp receive thread terminating due to a c++ exception\n" );
    }

    // Although this is redundant in certain situations it is
    // required because the receive thread must hang around 
    // until it receives its blocking socket call interrupt 
    // signal.
    this->iiu.cacRef.initiateAbortShutdown ( this->iiu );
}

//
// tcpiiu::tcpiiu ()
//
tcpiiu::tcpiiu ( cac & cac, callbackMutex & cbMutex, double connectionTimeout, 
        epicsTimerQueue & timerQueue, const osiSockAddr & addrIn, 
        comBufMemoryManager & comBufMemMgrIn,
        unsigned minorVersion, ipAddrToAsciiEngine & engineIn, 
        const cacChannel::priLev & priorityIn ) :
    caServerID ( addrIn.ia, priorityIn ),
    hostNameCacheInstance ( addrIn, engineIn ),
    recvThread ( *this, cbMutex, "CAC-TCP-recv", 
        epicsThreadGetStackSize ( epicsThreadStackBig ),
        cac::highestPriorityLevelBelow ( cac.getInitializingThreadsPriority() ) ),
    sendThread ( *this, "CAC-TCP-send",
        epicsThreadGetStackSize ( epicsThreadStackMedium ),
        cac::lowestPriorityLevelAbove (
            cac::lowestPriorityLevelAbove (
                cac.getInitializingThreadsPriority() ) ) ),
    recvDog ( cac, *this, connectionTimeout, timerQueue ),
    sendDog ( cac, *this, connectionTimeout, timerQueue ),
    sendQue ( *this, comBufMemMgrIn ),
    recvQue ( comBufMemMgrIn ),
    curDataMax ( MAX_TCP ),
    curDataBytes ( 0ul ),
    comBufMemMgr ( comBufMemMgrIn ),
    cacRef ( cac ),
    pCurData ( cac.allocateSmallBufferTCP () ),
    minorProtocolVersion ( minorVersion ),
    state ( iiucs_connecting ),
    sock ( INVALID_SOCKET ),
    contigRecvMsgCount ( 0u ),
    blockingForFlush ( 0u ),
    socketLibrarySendBufferSize ( 0x1000 ),
    unacknowledgedSendBytes ( 0u ),
    busyStateDetected ( false ),
    flowControlActive ( false ),
    echoRequestPending ( false ),
    oldMsgHeaderAvailable ( false ),
    msgHeaderAvailable ( false ),
    earlyFlush ( false ),
    recvProcessPostponedFlush ( false )
{
    this->sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( this->sock == INVALID_SOCKET ) {
        this->printf ( "CAC: unable to create virtual circuit because \"%s\"\n",
            SOCKERRSTR ( SOCKERRNO ) );
        cac.releaseSmallBufferTCP ( this->pCurData );
        throw std::bad_alloc ();
    }

    int flag = true;
    int status = setsockopt ( this->sock, IPPROTO_TCP, TCP_NODELAY,
                (char *) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        this->printf ("CAC: problems setting socket option TCP_NODELAY = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
    }

    flag = true;
    status = setsockopt ( this->sock , SOL_SOCKET, SO_KEEPALIVE,
                ( char * ) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        this->printf ( "CAC: problems setting socket option SO_KEEPALIVE = \"%s\"\n",
            SOCKERRSTR ( SOCKERRNO ) );
    }

    // load message queue with messages informing server 
    // of version, user, and host name of client
    {
        epicsGuard < cacMutex > guard ( this->cacRef.mutexRef() );
        this->versionMessage ( guard, this->priority() );
        this->userNameSetRequest ( guard );
        this->hostNameSetRequest ( guard );
    }

#   if 0
    {
        int i;

        /*
         * some concern that vxWorks will run out of mBuf's
         * if this change is made joh 11-10-98
         */        
        i = MAX_MSG_SIZE;
        status = setsockopt ( this->sock, SOL_SOCKET, SO_SNDBUF,
                ( char * ) &i, sizeof ( i ) );
        if (status < 0) {
            this->printf ("CAC: problems setting socket option SO_SNDBUF = \"%s\"\n",
                SOCKERRSTR ( SOCKERRNO ) );
        }
        i = MAX_MSG_SIZE;
        status = setsockopt ( this->sock, SOL_SOCKET, SO_RCVBUF,
                ( char * ) &i, sizeof ( i ) );
        if ( status < 0 ) {
            this->printf ("CAC: problems setting socket option SO_RCVBUF = \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
        }
    }
#   endif

    {
        int nBytes;
        osiSocklen_t sizeOfParameter = static_cast < int > ( sizeof ( nBytes ) );
        status = getsockopt ( this->sock, SOL_SOCKET, SO_SNDBUF,
                ( char * ) &nBytes, &sizeOfParameter );
        if ( status < 0 || nBytes < 0 || 
                sizeOfParameter != static_cast < int > ( sizeof ( nBytes ) ) ) {
            this->printf ("CAC: problems getting socket option SO_SNDBUF = \"%s\"\n",
                SOCKERRSTR ( SOCKERRNO ) );
        }
        else {
            this->socketLibrarySendBufferSize = static_cast < unsigned > ( nBytes );
        }
    }

#   if 0
    //
    // windows has a really strange implementation of thess options
    // and we can avoid the need for this by using pthread_kill on unix
    //
    {
        struct timeval timeout;
        double pollInterval = connectionTimeout / 8.0;
        timeout.tv_sec = static_cast < long > ( pollInterval );
        timeout.tv_usec = static_cast < long > 
            ( ( pollInterval - timeout.tv_sec ) * uSecPerSec );
        // intentionally ignore status as we dont expect that all systems
        // will accept this request
        setsockopt ( this->sock, SOL_SOCKET, SO_SNDTIMEO,
                ( char * ) & timeout, sizeof ( timeout ) );
        // intentionally ignore status as we dont expect that all systems
        // will accept this request
        setsockopt ( this->sock, SOL_SOCKET, SO_RCVTIMEO,
                ( char * ) & timeout, sizeof ( timeout ) );
    }
#   endif

    memset ( (void *) &this->curMsg, '\0', sizeof ( this->curMsg ) );
}

// this must always be called by the udp thread when it holds 
// the callback lock.
void tcpiiu::start ( epicsGuard < callbackMutex > & cbGuard )
{
    this->recvThread.start ();
    this->cacRef.notifyNewFD ( cbGuard, this->sock );
}

/*
 * tcpiiu::connect ()
 */
void tcpiiu::connect ()
{
    /* 
     * attempt to connect to a CA server
     */
    this->sendDog.start ();

    while ( true ) {
        osiSockAddr tmp = this->address ();
        int status = ::connect ( this->sock, 
                        &tmp.sa, sizeof ( tmp.sa ) );
        if ( status == 0 ) {

            this->sendDog.cancel ();

            epicsGuard < cacMutex > autoMutex ( this->cacRef.mutexRef() );

            if ( this->state == iiucs_connecting ) {
                // put the iiu into the connected state
                this->state = iiucs_connected;

                // start connection activity watchdog
                this->recvDog.connectNotify (); 
            }

            return;
        }

        int errnoCpy = SOCKERRNO;

        if ( errnoCpy == SOCK_EINTR ) {
            if ( this->state != iiucs_connecting ) {
                this->sendDog.cancel ();
                return;
            }
            continue;
        }
        else if ( errnoCpy == SOCK_SHUTDOWN ) {
            this->sendDog.cancel ();
            return;
        }
        else {  
            this->sendDog.cancel ();
            this->printf ( "Unable to connect because %d=\"%s\"\n", 
                errnoCpy, SOCKERRSTR ( errnoCpy ) );
            this->cacRef.disconnectNotify ( *this );
            return;
        }
    }
}

void tcpiiu::initiateCleanShutdown ( epicsGuard < cacMutex > & )
{
    if ( this->state == iiucs_connected || this->state == iiucs_connecting ) {
        this->state = iiucs_clean_shutdown;
    }
    this->sendThreadFlushEvent.signal ();
}

void tcpiiu::disconnectNotify ( epicsGuard < cacMutex > & )
{
    if ( this->state == iiucs_connected || this->state == iiucs_connecting ) {
        this->state = iiucs_disconnected;
    }
    this->sendThreadFlushEvent.signal ();
}

void tcpiiu::initiateAbortShutdown ( epicsGuard < callbackMutex > & cbGuard, 
                                    epicsGuard <cacMutex > & guard )
{
    iiu_conn_state oldState = this->state;
    if ( oldState != iiucs_abort_shutdown ) {
        this->state = iiucs_abort_shutdown;
        {
            epicsGuardRelease < cacMutex > guardRelease ( guard );
            this->cacRef.notifyDestroyFD ( cbGuard, this->sock );
        }

        // force abortive shutdown sequence 
        // (discard outstanding sends and receives)
        struct linger tmpLinger;
        tmpLinger.l_onoff = true;
        tmpLinger.l_linger = 0u;
        int status = setsockopt ( this->sock, SOL_SOCKET, SO_LINGER, 
            reinterpret_cast <char *> ( &tmpLinger ), sizeof (tmpLinger) );
        if ( status != 0 ) {
            errlogPrintf ( "CAC TCP socket linger set error was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
        }

        //
        // on HPUX close() and shutdown() are not enough so we must also
        // throw signals to interrupt the threads that may be in the 
        // send() and recv() system calls.
        //
        this->recvThread.interruptSocketRecv ();
        this->sendThread.interruptSocketSend ();

        // linux threads in recv() dont wakeup unless we also
        // call shutdown ( close() by itself is not enough )
        if ( oldState == iiucs_connected ) {
            status = ::shutdown ( this->sock, SD_BOTH );
            if ( status ) {
                errlogPrintf ("CAC TCP socket shutdown error was %s\n", 
                    SOCKERRSTR (SOCKERRNO) );
            }
        }

        //
        // on winsock and probably vxWorks shutdown() does not
        // unblock a thread in recv() so we use close() and introduce
        // some complexity because we must unregister the fd early
        //
        status = socket_close ( this->sock );
        if ( status ) {
            errlogPrintf ("CAC TCP socket close error was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
        }

        // 
        // wake up the send thread if it isnt blocking in send()
        //
        this->sendThreadFlushEvent.signal ();
    }
}

//
// tcpiiu::~tcpiiu ()
//
tcpiiu::~tcpiiu ()
{
    this->sendThread.exitWait ();
    this->recvThread.exitWait ();

    if ( this->state != this->iiucs_abort_shutdown ) {
        int status = socket_close ( this->sock );
        if ( status ) {
            errlogPrintf ("CAC TCP socket close error was %s\n", 
                SOCKERRSTR ( SOCKERRNO ) );
        }
    }

    // free message body cache
    if ( this->pCurData ) {
        if ( this->curDataMax == MAX_TCP ) {
            this->cacRef.releaseSmallBufferTCP ( this->pCurData );
        }
        else {
            this->cacRef.releaseLargeBufferTCP ( this->pCurData );
        }
    }
}

void tcpiiu::show ( unsigned level ) const
{
    epicsGuard < cacMutex > locker ( this->cacRef.mutexRef() );
    char buf[256];
    this->hostNameCacheInstance.hostName ( buf, sizeof ( buf ) );
    ::printf ( "Virtual circuit to \"%s\" at version V%u.%u state %u\n", 
        buf, CA_MAJOR_PROTOCOL_REVISION,
        this->minorProtocolVersion, this->state );
    if ( level > 1u ) {
        ::printf ( "\tcurrent data cache pointer = %p current data cache size = %lu\n",
            static_cast < void * > ( this->pCurData ), this->curDataMax );
        ::printf ( "\tcontiguous receive message count=%u, busy detect bool=%u, flow control bool=%u\n", 
            this->contigRecvMsgCount, this->busyStateDetected, this->flowControlActive );
    }
    if ( level > 2u ) {
        ::printf ( "\tvirtual circuit socket identifier %d\n", this->sock );
        ::printf ( "\tsend thread flush signal:\n" );
        this->sendThreadFlushEvent.show ( level-2u );
        ::printf ( "\tsend thread:\n" );
        this->sendThread.show ( level-2u );
        ::printf ( "\trecv thread:\n" );
        this->recvThread.show ( level-2u );
        ::printf ("\techo pending bool = %u\n", this->echoRequestPending );
        ::printf ( "IO identifier hash table:\n" );
        tsDLIterConst < nciu > pChan = this->channelList.firstIter ();
	    while ( pChan.valid () ) {
            pChan->show ( level - 2u );
            pChan++;
        }
    }
}

bool tcpiiu::setEchoRequestPending () // X aCC 361
{
    {
        epicsGuard < cacMutex > locker ( this->cacRef.mutexRef() );
        this->echoRequestPending = true;
    }
    this->flushRequest ();
    if ( CA_V43 ( this->minorProtocolVersion ) ) {
        // we send an echo
        return true;
    }
    else {
        // we send a NOOP
        return false;
    }
}

//
// tcpiiu::processIncoming()
//
bool tcpiiu::processIncoming ( epicsGuard < callbackMutex > & guard )
{
    while ( true ) {

        //
        // fetch a complete message header
        //
        unsigned nBytes = this->recvQue.occupiedBytes ();

        if ( ! this->msgHeaderAvailable ) {
            if ( ! this->oldMsgHeaderAvailable ) {
                if ( nBytes < sizeof ( caHdr ) ) {
                    this->flushIfRecvProcessRequested ();
                    return true;
                } 
                this->curMsg.m_cmmd = this->recvQue.popUInt16 ();
                this->curMsg.m_postsize = this->recvQue.popUInt16 ();
                this->curMsg.m_dataType = this->recvQue.popUInt16 ();
                this->curMsg.m_count = this->recvQue.popUInt16 ();
                this->curMsg.m_cid = this->recvQue.popUInt32 ();
                this->curMsg.m_available = this->recvQue.popUInt32 ();
                this->oldMsgHeaderAvailable = true;
            }
            if ( this->curMsg.m_postsize == 0xffff ) {
                static const unsigned annexSize = 
                    sizeof ( this->curMsg.m_postsize ) + sizeof ( this->curMsg.m_count );
                if ( this->recvQue.occupiedBytes () < annexSize ) {
                    this->flushIfRecvProcessRequested ();
                    return true;
                }
                this->curMsg.m_postsize = this->recvQue.popUInt32 ();
                this->curMsg.m_count = this->recvQue.popUInt32 ();
            }
            this->msgHeaderAvailable = true;
            debugPrintf (
                ( "%s Cmd=%3u Type=%3u Count=%8u Size=%8u",
                this->pHostName (),
                this->curMsg.m_cmmd,
                this->curMsg.m_dataType,
                this->curMsg.m_count,
                this->curMsg.m_postsize) );
            debugPrintf (
                ( " Avail=%8u Cid=%8u\n",
                this->curMsg.m_available,
                this->curMsg.m_cid) );
        }

        //
        // make sure we have a large enough message body cache
        //
        if ( this->curMsg.m_postsize > this->curDataMax ) {
            if ( this->curDataMax == MAX_TCP && 
                    this->cacRef.largeBufferSizeTCP() >= this->curMsg.m_postsize ) {
                char * pBuf = this->cacRef.allocateLargeBufferTCP ();
                if ( pBuf ) {
                    this->cacRef.releaseSmallBufferTCP ( this->pCurData );
                    this->pCurData = pBuf;
                    this->curDataMax = this->cacRef.largeBufferSizeTCP ();
                }
                else {
                    this->printf ("CAC: not enough memory for message body cache (ignoring response message)\n");
                }
            }
        }

        if ( this->curMsg.m_postsize <= this->curDataMax ) {
            if ( this->curMsg.m_postsize > 0u ) {
                this->curDataBytes += this->recvQue.copyOutBytes ( 
                            &this->pCurData[this->curDataBytes], 
                            this->curMsg.m_postsize - this->curDataBytes );
                if ( this->curDataBytes < this->curMsg.m_postsize ) {
                    this->flushIfRecvProcessRequested ();
                    return true;
                }
            }
            bool msgOK = this->cacRef.executeResponse ( guard, *this, 
                                this->curMsg, this->pCurData );
            if ( ! msgOK ) {
                return false;
            }
        }
        else {
            static bool once = false;
            if ( ! once ) {
                this->printf (
    "CAC: response with payload size=%u > EPICS_CA_MAX_ARRAY_BYTES ignored\n",
                    this->curMsg.m_postsize );
                once = true;
            }
            this->curDataBytes += this->recvQue.removeBytes ( 
                    this->curMsg.m_postsize - this->curDataBytes );
            if ( this->curDataBytes < this->curMsg.m_postsize  ) {
                this->flushIfRecvProcessRequested ();
                return true;
            }
        }
 
        this->oldMsgHeaderAvailable = false;
        this->msgHeaderAvailable = false;
        this->curDataBytes = 0u;
    }
#   if defined ( __HP_aCC ) && _HP_aCC <= 033300
        return false; // to make hpux compiler happy...
#   endif
}

/*
 * tcpiiu::hostNameSetRequest ()
 */
void tcpiiu::hostNameSetRequest ( epicsGuard < cacMutex > & )
{
    if ( ! CA_V41 ( this->minorProtocolVersion ) ) {
        return;
    }

    epicsSingleton < localHostName >::reference 
            ref ( localHostNameAtLoadTime.getReference () );
    const char * pName = ref->pointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( postSize + 16u ) ) {
        this->flushRequest ();
    }

    this->sendQue.beginMsg ();
    this->sendQue.pushUInt16 ( CA_PROTO_HOST_NAME ); // cmd
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( postSize ) ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( cacNillBytes, postSize - size );
    this->sendQue.commitMsg ();
}

/*
 * tcpiiu::userNameSetRequest ()
 */
void tcpiiu::userNameSetRequest ( epicsGuard < cacMutex > & )
{
    if ( ! CA_V41 ( this->minorProtocolVersion ) ) {
        return;
    }

    const char *pName = this->cacRef.userNamePointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( postSize + 16u ) ) {
        this->flushRequest ();
    }

    this->sendQue.beginMsg ();
    this->sendQue.pushUInt16 ( CA_PROTO_CLIENT_NAME ); // cmd
    this->sendQue.pushUInt16 ( static_cast <epicsUInt16> ( postSize ) ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( cacNillBytes, postSize - size );
    this->sendQue.commitMsg ();
}

void tcpiiu::disableFlowControlRequest ( epicsGuard < cacMutex > & )
{
    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ();
    }
    this->sendQue.beginMsg ();
    this->sendQue.pushUInt16 ( CA_PROTO_EVENTS_ON ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
    this->sendQue.commitMsg ();
}

void tcpiiu::enableFlowControlRequest ( epicsGuard < cacMutex > & )
{
    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ();
    }
    this->sendQue.beginMsg ();
    this->sendQue.pushUInt16 ( CA_PROTO_EVENTS_OFF ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
    this->sendQue.commitMsg ();
}

void tcpiiu::versionMessage ( epicsGuard < cacMutex > &,
                             const cacChannel::priLev & priority )
{
    assert ( priority <= 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ();
    }

    this->sendQue.beginMsg ();
    this->sendQue.pushUInt16 ( CA_PROTO_VERSION ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // old postsize field
    this->sendQue.pushUInt16 ( static_cast <ca_uint16_t> ( priority ) ); // old dataType field
    this->sendQue.pushUInt16 ( CA_MINOR_PROTOCOL_REVISION ); // old count field
    this->sendQue.pushUInt32 ( 0u ); // ( old cid field )
    this->sendQue.pushUInt32 ( 0u ); // ( old available field )
    this->sendQue.commitMsg ();
}

void tcpiiu::echoRequest ( epicsGuard < cacMutex > & )
{
    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ();
    }
    this->sendQue.beginMsg ();
    this->sendQue.pushUInt16 ( CA_PROTO_ECHO ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
    this->sendQue.commitMsg ();
}

void tcpiiu::writeRequest ( epicsGuard < cacMutex > &,
                nciu &chan, unsigned type, unsigned nElem, const void *pValue )
{
    if ( ! chan.connected () ) {
        throw cacChannel::notConnected();
    }
    this->sendQue.insertRequestWithPayLoad ( CA_PROTO_WRITE,  
        type, nElem, chan.getSID(), chan.getCID(), pValue,
        CA_V49 ( this->minorProtocolVersion ) );
}


void tcpiiu::writeNotifyRequest ( epicsGuard < cacMutex > &,
                                 nciu &chan, netWriteNotifyIO &io, unsigned type,  
                                unsigned nElem, const void *pValue )
{
    if ( ! chan.connected () ) {
        throw cacChannel::notConnected();
    }
    if ( ! this->ca_v41_ok () ) {
        throw cacChannel::unsupportedByService();
    }
    this->sendQue.insertRequestWithPayLoad ( CA_PROTO_WRITE_NOTIFY,  
        type, nElem, chan.getSID(), io.getId(), pValue,
        CA_V49 ( this->minorProtocolVersion ) );
}

void tcpiiu::readNotifyRequest ( epicsGuard < cacMutex > &, 
                               nciu & chan, netReadNotifyIO & io, 
                               unsigned dataType, unsigned nElem )
{
    if ( ! chan.connected () ) {
        throw cacChannel::notConnected ();
    }
    if ( ! dbr_type_is_valid ( dataType ) ) {
        throw cacChannel::badType();
    }
    if ( nElem > chan.nativeElementCount() ) {
        throw cacChannel::outOfBounds ();
    }
    unsigned maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = this->cacRef.largeBufferSizeTCP ();
    }
    else {
        maxBytes = MAX_TCP;
    }
    unsigned maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_READ_NOTIFY, 0u, 
        static_cast < ca_uint16_t > ( dataType ), 
        nElem, chan.getSID(), io.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );
    this->sendQue.commitMsg ();
}

void tcpiiu::createChannelRequest ( nciu & chan )
{
    const char *pName;
    unsigned nameLength;
    ca_uint32_t identity;
    if ( this->ca_v44_ok () ) {
        identity = chan.getCID ();
        pName = chan.pName ();
        nameLength = chan.nameLen ();
    }
    else {
        identity = chan.getSID ();
        pName = 0;
        nameLength = 0u;
    }

    unsigned postCnt = CA_MESSAGE_ALIGN ( nameLength );

    if ( postCnt >= 0xffff ) {
        throw cacChannel::unsupportedByService();
    }

    this->sendQue.beginMsg ();
    this->sendQue.pushUInt16 ( CA_PROTO_CLAIM_CIU ); // cmd
    this->sendQue.pushUInt16 ( static_cast < epicsUInt16 > ( postCnt ) ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( identity ); // cid
    //
    // The available field is used (abused)
    // here to communicate the minor version number
    // starting with CA 4.1.
    //
    this->sendQue.pushUInt32 ( CA_MINOR_PROTOCOL_REVISION ); // available 
    if ( nameLength ) {
        this->sendQue.pushString ( pName, nameLength );
    }
    if ( postCnt > nameLength ) {
        this->sendQue.pushString ( cacNillBytes, postCnt - nameLength );
    }
    this->sendQue.commitMsg ();
}

void tcpiiu::clearChannelRequest ( epicsGuard < cacMutex > &,
                                  ca_uint32_t sid, ca_uint32_t cid )
{
    this->sendQue.beginMsg ();
    this->sendQue.pushUInt16 ( CA_PROTO_CLEAR_CHANNEL ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( sid ); // cid
    this->sendQue.pushUInt32 ( cid ); // available 
    this->sendQue.commitMsg ();
}

//
// this routine return void because if this internally fails the best response
// is to try again the next time that we reconnect
//
void tcpiiu::subscriptionRequest ( epicsGuard < cacMutex > &,
                                  nciu &chan, netSubscription & subscr )
{
    if ( ! chan.connected() ) {
        return;
    }
    unsigned mask = subscr.getMask();
    if ( mask > 0xffff ) {
        mask &= 0xffff;
        this->cacRef.printf ( "CAC: subscriptionRequest() truncated unusual event select mask\n" );
    }
    arrayElementCount nElem = subscr.getCount ();
    unsigned maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = this->cacRef.largeBufferSizeTCP ();
    }
    else {
        maxBytes = MAX_TCP;
    }
    unsigned dataType = subscr.getType ();
    unsigned maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENT_ADD, 16u, 
        static_cast < ca_uint16_t > ( dataType ), 
        nElem, chan.getSID(), subscr.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );

    // extension
    this->sendQue.pushFloat32 ( 0.0 ); // m_lval
    this->sendQue.pushFloat32 ( 0.0 ); // m_hval
    this->sendQue.pushFloat32 ( 0.0 ); // m_toval
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( mask ) ); // m_mask
    this->sendQue.pushUInt16 ( 0u ); // m_pad
    this->sendQue.commitMsg ();
}

void tcpiiu::subscriptionCancelRequest ( epicsGuard < cacMutex > &,
                             nciu & chan, netSubscription & subscr )
{
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENT_CANCEL, 0u, 
        static_cast < ca_uint16_t > ( subscr.getType() ), 
        static_cast < ca_uint16_t > ( subscr.getCount() ), 
        chan.getSID(), subscr.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );
    this->sendQue.commitMsg ();
}

bool tcpiiu::flush ()
{
    if ( this->sendQue.occupiedBytes() == 0 ) {
        return true;
    }

    bool success = true;
    unsigned bytesToBeSent = 0u;
    while ( true ) {
        comBuf * pBuf;

        {
            epicsGuard < cacMutex > autoMutex ( this->cacRef.mutexRef() );
            // set it here with this odd order because we must have 
            // the lock and we miust have already sent the bytes
            if ( bytesToBeSent ) {
                this->unacknowledgedSendBytes += bytesToBeSent;
            }
            pBuf = this->sendQue.popNextComBufToSend ();
            if ( ! pBuf ) {
                break;
            }
            bytesToBeSent = pBuf->occupiedBytes ();
        }

        success = pBuf->flushToWire ( *this );
        pBuf->~comBuf ();
        this->comBufMemMgr.release ( pBuf );

        if ( ! success ) {
            epicsGuard < cacMutex > autoMutex ( this->cacRef.mutexRef() );
            while ( ( pBuf = this->sendQue.popNextComBufToSend () ) ) {
                pBuf->~comBuf ();
                this->comBufMemMgr.release ( pBuf );
            }
            break;
        }

        //
        // we avoid calling this with the lock applied because
        // it restarts the recv wd timer, this might block
        // until a recv wd timer expire callback completes, and 
        // this callback takes the lock
        //
        if ( this->unacknowledgedSendBytes > 
            this->socketLibrarySendBufferSize ) {
            this->recvDog.sendBacklogProgressNotify ();
        }
    }
    if ( this->blockingForFlush ) {
        this->flushBlockEvent.signal ();
    }
    this->earlyFlush = false;
    return success;
}

// ~tcpiiu() will not return while this->blockingForFlush is greater than zero
void tcpiiu::blockUntilSendBacklogIsReasonable ( 
          cacNotify & notify, epicsGuard < cacMutex > & primaryLocker )
{
    assert ( this->blockingForFlush < UINT_MAX );
    this->blockingForFlush++;
    while ( this->sendQue.flushBlockThreshold(0u) && this->state == iiucs_connected ) {
        epicsGuardRelease < cacMutex > autoRelease ( primaryLocker );
        notify.blockForEventAndEnableCallbacks ( this->flushBlockEvent, 30.0 );
    }
    assert ( this->blockingForFlush > 0u );
    this->blockingForFlush--;
    if ( this->blockingForFlush == 0 ) {
        this->flushBlockEvent.signal ();
    }
}

void tcpiiu::flushRequestIfAboveEarlyThreshold ( epicsGuard < cacMutex > & )
{
    if ( ! this->earlyFlush && this->sendQue.flushEarlyThreshold(0u) ) {
        this->earlyFlush = true;
        this->sendThreadFlushEvent.signal ();
    }
}

bool tcpiiu::flushBlockThreshold ( epicsGuard < cacMutex > & ) const
{
    return this->sendQue.flushBlockThreshold ( 0u );
}

osiSockAddr tcpiiu::getNetworkAddress () const
{
    return this->address();
}

// not inline because its virtual
bool tcpiiu::ca_v42_ok () const
{
    return CA_V42 ( this->minorProtocolVersion );
}

void tcpiiu::requestRecvProcessPostponedFlush ()
{
    this->recvProcessPostponedFlush = true;
}

void tcpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{   
    this->hostNameCacheInstance.hostName ( pBuf, bufLength );
}

// deprecated - please dont use - this is _not_ thread safe
const char * tcpiiu::pHostName () const
{
    static char nameBuf [128];
    this->hostName ( nameBuf, sizeof ( nameBuf ) );
    return nameBuf; // ouch !!
}

void tcpiiu::removeAllChannels ( epicsGuard < callbackMutex > & cbGuard, 
                                epicsGuard < cacMutex > & guard,
                                cacDisconnectChannelPrivate & dcp )
{
    while ( nciu *pChan = this->channelList.first() ) {
        // if the claim reply has not returned then we will issue
        // the clear channel request to the server when the claim reply
        // arrives and there is no matching nciu in the client
        if ( pChan->connected() ) {
            this->clearChannelRequest ( guard, pChan->getSID(), pChan->getCID() );
        }
        dcp.disconnectChannel ( cbGuard, guard, *pChan );
    }
}

void tcpiiu::installChannel ( epicsGuard < cacMutex > &, nciu & chan, unsigned sidIn, 
    ca_uint16_t typeIn, arrayElementCount countIn )
{
    this->channelList.add ( chan );
    chan.searchReplySetUp ( *this, sidIn, typeIn, countIn );
    chan.createChannelRequest ( *this );
    this->flushRequest ();
}

void tcpiiu::uninstallChan 
        ( epicsGuard < cacMutex > & guard, nciu & chan )
{
    this->channelList.remove ( chan );
    if ( channelList.count() == 0 ) {
        this->initiateCleanShutdown ( guard );
    }
}

int tcpiiu::printf ( const char *pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->cacRef.vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

// this is called virtually
void tcpiiu::flushRequest ()
{
    this->sendThreadFlushEvent.signal ();
}

void tcpiiu::blockUntilBytesArePendingInOS ()
{
#if 1
    FD_SET readBits;
    FD_ZERO ( & readBits );
    while ( this->state == tcpiiu::iiucs_connected ) {
        FD_SET ( this->sock, & readBits );
        struct timeval tmo;
        tmo.tv_sec = 1;
        tmo.tv_usec = 0;
        int status = select ( this->sock + 1, & readBits, NULL, NULL, & tmo );
        if ( status > 0 ) {
            if ( FD_ISSET ( this->sock, & readBits ) ) {	
                return;
            }
        }
        else if ( status < 0 ) {
            if ( SOCKERRNO != SOCK_EINTR ) {
                return;
            }
        }
    }
#else
    char buf;
    while ( this->state == tcpiiu::iiucs_connected ) {
        int status = ::recv ( this->sock, 
            & buf, 1, MSG_PEEK );
        if ( status > 0 ) {
            break;
        }
        else if ( status == 0 ) {
            this->cacRef.disconnectNotify ( *this );
            return;
        }
        else {
            int localErrno = SOCKERRNO;

            if ( localErrno == SOCK_SHUTDOWN ) {
                this->cacRef.disconnectNotify ( *this );
                return;
            }

            if ( localErrno == SOCK_EINTR ) {
                continue;
            }
            
            if ( localErrno == SOCK_ECONNABORTED ) {
                this->cacRef.disconnectNotify ( *this );
                return;
            }

            if ( localErrno == SOCK_ECONNRESET ) {
                this->cacRef.disconnectNotify ( *this );
                return;
            }

            {
                char name[64];
                this->hostName ( name, sizeof ( name ) );
                this->printf ( "Unexpected problem with circuit to CA server \"%s\" was \"%s\" - disconnecting\n", 
                    name, SOCKERRSTR ( localErrno ) );
            }

            this->cacRef.initiateAbortShutdown ( *this );
            return;
        }
    }
#endif
}

bool tcpiiu::bytesArePendingInOS () const
{
#if 1
    FD_SET readBits;
    FD_ZERO ( & readBits );
    FD_SET ( this->sock, & readBits );
    struct timeval tmo;
    tmo.tv_sec = 0;
    tmo.tv_usec = 0;
    int status = select ( this->sock + 1, & readBits, NULL, NULL, & tmo );
    if ( status > 0 ) {
        if ( FD_ISSET ( this->sock, & readBits ) ) {	
            return true;
        }
    }
    return false;
#else
    osiSockIoctl_t bytesPending;
    int status = socket_ioctl ( this->sock, // X aCC 392
                            FIONREAD, & bytesPending );
    if ( status ) {
        return false;
    }
    if ( bytesPending > 0 ) {
        return true;
    }
    return false;
#endif
}



