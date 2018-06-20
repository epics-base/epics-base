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

#ifdef _MSC_VER
#   pragma warning(disable:4355)
#endif

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include <stdexcept>
#include <string>

#include <stdlib.h>

#include "errlog.h"

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
#include "epicsSignal.h"
#include "caerr.h"
#include "udpiiu.h"

using namespace std;

tcpSendThread::tcpSendThread (
        class tcpiiu & iiuIn, const char * pName, 
        unsigned stackSize, unsigned priority ) :
    thread ( *this, pName, stackSize, priority ), iiu ( iiuIn )
{
}

tcpSendThread::~tcpSendThread ()
{
}

void tcpSendThread::start ()
{
    this->thread.start ();
}

void tcpSendThread::show ( unsigned /* level */ ) const
{
}

void tcpSendThread::exitWait ()
{
    this->thread.exitWait ();
}

void tcpSendThread::run ()
{
    try {
        epicsGuard < epicsMutex > guard ( this->iiu.mutex );

        bool laborPending = false;

        while ( true ) {

            // dont wait if there is still labor to be done below
            if ( ! laborPending ) {
                epicsGuardRelease < epicsMutex > unguard ( guard );
                this->iiu.sendThreadFlushEvent.wait ();
            }

            if ( this->iiu.state != tcpiiu::iiucs_connected ) {
                break;
            }

            laborPending = false;
            bool flowControlLaborNeeded = 
                this->iiu.busyStateDetected != this->iiu.flowControlActive;
            bool echoLaborNeeded = this->iiu.echoRequestPending;
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
                this->iiu.echoRequest ( guard );
            }

            while ( nciu * pChan = this->iiu.createReqPend.get () ) {
                this->iiu.createChannelRequest ( *pChan, guard );

                if ( CA_V42 ( this->iiu.minorProtocolVersion ) ) {
                    this->iiu.createRespPend.add ( *pChan );
                    pChan->channelNode::listMember = 
                        channelNode::cs_createRespPend;
                }
                else {
                    // This wakes up the resp thread so that it can call
                    // the connect callback. This isnt maximally efficent
                    // but it has the excellent side effect of not requiring
                    // that the UDP thread take the callback lock. There are
                    // almost no V42 servers left at this point.
                    this->iiu.v42ConnCallbackPend.add ( *pChan );
                    pChan->channelNode::listMember = 
                        channelNode::cs_v42ConnCallbackPend;
                    this->iiu.echoRequestPending = true;
                    laborPending = true;
                }
                
                if ( this->iiu.sendQue.flushBlockThreshold () ) {
                    laborPending = true;
                    break;
                }
            }

            while ( nciu * pChan = this->iiu.subscripReqPend.get () ) {
                // this installs any subscriptions as needed
                pChan->resubscribe ( guard );
                this->iiu.connectedList.add ( *pChan );
                pChan->channelNode::listMember = 
                    channelNode::cs_connected;
                if ( this->iiu.sendQue.flushBlockThreshold () ) {
                    laborPending = true;
                    break;
                }
            }

            while ( nciu * pChan = this->iiu.subscripUpdateReqPend.get () ) {
                // this updates any subscriptions as needed
                pChan->sendSubscriptionUpdateRequests ( guard );
                this->iiu.connectedList.add ( *pChan );
                pChan->channelNode::listMember = 
                    channelNode::cs_connected;
                if ( this->iiu.sendQue.flushBlockThreshold () ) {
                    laborPending = true;
                    break;
                }
            }

            if ( ! this->iiu.sendThreadFlush ( guard ) ) {
                break;
            }
        }
        if ( this->iiu.state == tcpiiu::iiucs_clean_shutdown ) {
            this->iiu.sendThreadFlush ( guard );
            // this should cause the server to disconnect from 
            // the client
            int status = ::shutdown ( this->iiu.sock, SHUT_WR );
            if ( status ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf ("CAC TCP clean socket shutdown error was %s\n", 
                    sockErrBuf );
            }
        }
    }
    catch ( ... ) {
        errlogPrintf (
            "cac: tcp send thread received an unexpected exception "
            "- disconnecting\n");
        // this should cause the server to disconnect from 
        // the client
        int status = ::shutdown ( this->iiu.sock, SHUT_WR );
        if ( status ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ("CAC TCP clean socket shutdown error was %s\n", 
                sockErrBuf );
        }
    }

    this->iiu.sendDog.cancel ();
    this->iiu.recvDog.shutdown ();

    while ( ! this->iiu.recvThread.exitWait ( 30.0 ) ) {
        // it is possible to get stuck here if the user calls 
        // ca_context_destroy() when a circuit isnt known to
        // be unresponsive, but is. That situation is probably
        // rare, and the IP kernel might have a timeout for
        // such situations, nevertheless we will attempt to deal 
        // with it here after waiting a reasonable amount of time
        // for a clean shutdown to finish.
        epicsGuard < epicsMutex > guard ( this->iiu.mutex );
        this->iiu.initiateAbortShutdown ( guard );
    }

    // user threads blocking for send backlog to be reduced
    // will abort their attempt to get space if 
    // the state of the tcpiiu changes from connected to a
    // disconnecting state. Nevertheless, we need to wait
    // for them to finish prior to destroying the IIU.
    {
        epicsGuard < epicsMutex > guard ( this->iiu.mutex );
        while ( this->iiu.blockingForFlush ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            epicsThreadSleep ( 0.1 );
        }
    }
    this->iiu.cacRef.destroyIIU ( this->iiu );
}

unsigned tcpiiu::sendBytes ( const void *pBuf, 
    unsigned nBytesInBuf, const epicsTime & currentTime )
{
    unsigned nBytes = 0u;
    assert ( nBytesInBuf <= INT_MAX );

    this->sendDog.start ( currentTime );

    while ( true ) {
        int status = ::send ( this->sock, 
            static_cast < const char * > (pBuf), (int) nBytesInBuf, 0 );
        if ( status > 0 ) {
            nBytes = static_cast <unsigned> ( status );
            // printf("SEND: %u\n", nBytes );
            break;
        }
        else {
            epicsGuard < epicsMutex > guard ( this->mutex );
            if ( this->state != iiucs_connected &&
                this->state != iiucs_clean_shutdown ) {
                break;
            }
            // winsock indicates disconnect by returning zero here
            if ( status == 0 ) {
                this->disconnectNotify ( guard );
                break;
            }

            int localError = SOCKERRNO;

            if ( localError == SOCK_EINTR ) {
                continue;
            }

            if ( localError == SOCK_ENOBUFS ) {
                errlogPrintf ( 
                    "CAC: system low on network buffers "
                    "- send retry in 15 seconds\n" );
                {
                    epicsGuardRelease < epicsMutex > unguard ( guard );
                    epicsThreadSleep ( 15.0 );
                }
                continue;
            }

            if ( 
                    localError != SOCK_EPIPE && 
                    localError != SOCK_ECONNRESET &&
                    localError != SOCK_ETIMEDOUT && 
                    localError != SOCK_ECONNABORTED &&
                    localError != SOCK_SHUTDOWN ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf ( "CAC: unexpected TCP send error: %s\n", 
                    sockErrBuf );
            }

            this->disconnectNotify ( guard );
            break;
        }
    }

    this->sendDog.cancel ();

    return nBytes;
}

void tcpiiu::recvBytes ( 
        void * pBuf, unsigned nBytesInBuf, statusWireIO & stat )
{
    assert ( nBytesInBuf <= INT_MAX );

    while ( true ) {
        int status = ::recv ( this->sock, static_cast <char *> ( pBuf ), 
            static_cast <int> ( nBytesInBuf ), 0 );

        if ( status > 0 ) {
            stat.bytesCopied = static_cast <unsigned> ( status );
            assert ( stat.bytesCopied <= nBytesInBuf );
            stat.circuitState = swioConnected;
            return;
        }
        else {
            epicsGuard < epicsMutex > guard ( this->mutex );

            if ( status == 0 ) {
                this->disconnectNotify ( guard );
                stat.bytesCopied = 0u;
                stat.circuitState = swioPeerHangup;
                return;
            }

            // if the circuit was locally aborted then supress 
            // warning messages about bad file descriptor etc
            if ( this->state != iiucs_connected && 
                    this->state != iiucs_clean_shutdown ) {
                stat.bytesCopied = 0u;
                stat.circuitState = swioLocalAbort;
                return;
            } 

            int localErrno = SOCKERRNO;

            if ( localErrno == SOCK_SHUTDOWN ) {
                stat.bytesCopied = 0u;
                stat.circuitState = swioPeerHangup;
                return;
            }

            if ( localErrno == SOCK_EINTR ) {
                continue;
            }

            if ( localErrno == SOCK_ENOBUFS ) {
                errlogPrintf ( 
                    "CAC: system low on network buffers "
                    "- receive retry in 15 seconds\n" );
                {
                    epicsGuardRelease < epicsMutex > unguard ( guard );
                    epicsThreadSleep ( 15.0 );
                }
                continue;
            }

            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );

            // the replacable printf handler isnt called here
            // because it reqires a callback lock which probably
            // isnt appropriate here
            char name[64];
            this->hostNameCacheInstance.getName ( 
                name, sizeof ( name ) );
            errlogPrintf (
                "Unexpected problem with CA circuit to"
                " server \"%s\" was \"%s\" - disconnecting\n", 
                        name, sockErrBuf );
            
            stat.bytesCopied = 0u;
            stat.circuitState = swioPeerAbort;
            return;
        }
    }
}

tcpRecvThread::tcpRecvThread ( 
    class tcpiiu & iiuIn, class epicsMutex & cbMutexIn,
    cacContextNotify & ctxNotifyIn, const char * pName, 
    unsigned int stackSize, unsigned int priority  ) :
    thread ( *this, pName, stackSize, priority ),
        iiu ( iiuIn ), cbMutex ( cbMutexIn ), 
        ctxNotify ( ctxNotifyIn ) {}

tcpRecvThread::~tcpRecvThread ()
{
}

void tcpRecvThread::start ()
{
    this->thread.start ();
}

void tcpRecvThread::show ( unsigned /* level */ ) const
{
}

bool tcpRecvThread::exitWait ( double delay )
{
    return this->thread.exitWait ( delay );
}

void tcpRecvThread::exitWait ()
{
    this->thread.exitWait ();
}

bool tcpRecvThread::validFillStatus ( 
    epicsGuard < epicsMutex > & guard, const statusWireIO & stat )
{
    if ( this->iiu.state != tcpiiu::iiucs_connected &&
        this->iiu.state != tcpiiu::iiucs_clean_shutdown ) {
        return false;
    }
    if ( stat.circuitState == swioConnected ) {
        return true;
    }
    if ( stat.circuitState == swioPeerHangup ||
        stat.circuitState == swioPeerAbort ) {
        this->iiu.disconnectNotify ( guard );
    }
    else if ( stat.circuitState == swioLinkFailure ) {
        this->iiu.initiateAbortShutdown ( guard );
    }
    else if ( stat.circuitState == swioLocalAbort ) {
        // state change already occurred
    }
    else {
        errlogMessage ( "cac: invalid fill status - disconnecting" );
        this->iiu.disconnectNotify ( guard );
    }
    return false;
}

void tcpRecvThread::run ()
{
    try {
        {
            bool connectSuccess = false;
            {
                epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                this->connect ( guard );
                connectSuccess = this->iiu.state == tcpiiu::iiucs_connected;
            }
            if ( ! connectSuccess ) {
                this->iiu.recvDog.shutdown ();
                this->iiu.cacRef.destroyIIU ( this->iiu );
                return;
            }
            if ( this->iiu.isNameService () ) {
                this->iiu.pSearchDest->setCircuit ( &this->iiu );
                this->iiu.pSearchDest->enable ();
            }
        }

        this->iiu.sendThread.start ();
        epicsThreadPrivateSet ( caClientCallbackThreadId, &this->iiu );
        this->iiu.cacRef.attachToClientCtx ();

        comBuf * pComBuf = 0;
        while ( true ) {

            //
            // We leave the bytes pending and fetch them after
            // callbacks are enabled when running in the old preemptive 
            // call back disabled mode so that asynchronous wakeup via
            // file manager call backs works correctly. This does not 
            // appear to impact performance.
            //
            if ( ! pComBuf ) {
                pComBuf = new ( this->iiu.comBufMemMgr ) comBuf;
            }

            statusWireIO stat;
            pComBuf->fillFromWire ( this->iiu, stat );

            epicsTime currentTime = epicsTime::getCurrent ();

            {
                epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                
                if ( ! this->validFillStatus ( guard, stat ) ) {
                    break;
                }
                if ( stat.bytesCopied == 0u ) {
                    continue;
                }

                this->iiu.recvQue.pushLastComBufReceived ( *pComBuf );
                pComBuf = 0;

                this->iiu._receiveThreadIsBusy = true;
            }

            bool sendWakeupNeeded = false;
            {
                // only one recv thread at a time may call callbacks
                // - pendEvent() blocks until threads waiting for
                // this lock get a chance to run
                callbackManager mgr ( this->ctxNotify, this->cbMutex );

                epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                
                // route legacy V42 channel connect through the recv thread -
                // the only thread that should be taking the callback lock
                while ( nciu * pChan = this->iiu.v42ConnCallbackPend.first () ) {
                    this->iiu.connectNotify ( guard, *pChan );
                    pChan->connect ( mgr.cbGuard, guard );
                }

                this->iiu.unacknowledgedSendBytes = 0u;

                bool protocolOK = false;
                {
                    epicsGuardRelease < epicsMutex > unguard ( guard );
                    // execute receive labor
                    protocolOK = this->iiu.processIncoming ( currentTime, mgr );
                }

                if ( ! protocolOK ) {
                    this->iiu.initiateAbortShutdown ( guard );
                    break;
                }
                this->iiu._receiveThreadIsBusy = false;
                // reschedule connection activity watchdog
                this->iiu.recvDog.messageArrivalNotify ( guard ); 
                //
                // if this thread has connected channels with subscriptions
                // that need to be sent then wakeup the send thread
                if ( this->iiu.subscripReqPend.count() ) {
                    sendWakeupNeeded = true;
                }
            }
            
            //
            // we dont feel comfortable calling this with a lock applied
            // (it might block for longer than we like)
            //
            // we would prefer to improve efficency by trying, first, a 
            // recv with the new MSG_DONTWAIT flag set, but there isnt 
            // universal support
            //
            bool bytesArePending = this->iiu.bytesArePendingInOS ();
            {
                epicsGuard < epicsMutex > guard ( this->iiu.mutex );
                if ( bytesArePending ) {
                    if ( ! this->iiu.busyStateDetected ) {
                        this->iiu.contigRecvMsgCount++;
                        if ( this->iiu.contigRecvMsgCount >= 
                            this->iiu.cacRef.maxContiguousFrames ( guard ) ) {
                            this->iiu.busyStateDetected = true;
                            sendWakeupNeeded = true;
                        }
                    }
                }
                else {
                    // if no bytes are pending then we must immediately
                    // switch off flow control w/o waiting for more
                    // data to arrive
                    this->iiu.contigRecvMsgCount = 0u;
                    if ( this->iiu.busyStateDetected ) {
                        sendWakeupNeeded = true;
                        this->iiu.busyStateDetected = false;
                    }
                }
            }

            if ( sendWakeupNeeded ) {
                this->iiu.sendThreadFlushEvent.signal ();
            }
        }

        if ( pComBuf ) {
            pComBuf->~comBuf ();
            this->iiu.comBufMemMgr.release ( pComBuf );
        }
    }
    catch ( std::bad_alloc & ) {
        errlogPrintf ( 
            "CA client library tcp receive thread "
            "terminating due to no space in pool "
            "C++ exception\n" );
        epicsGuard < epicsMutex > guard ( this->iiu.mutex );
        this->iiu.initiateCleanShutdown ( guard );
    }
    catch ( std::exception & except ) {
        errlogPrintf ( 
            "CA client library tcp receive thread "
            "terminating due to C++ exception \"%s\"\n", 
            except.what () );
        epicsGuard < epicsMutex > guard ( this->iiu.mutex );
        this->iiu.initiateCleanShutdown ( guard );
    }
    catch ( ... ) {
        errlogPrintf ( 
            "CA client library tcp receive thread "
            "terminating due to a non-standard C++ exception\n" );
        epicsGuard < epicsMutex > guard ( this->iiu.mutex );
        this->iiu.initiateCleanShutdown ( guard );
    }
}

/*
 * tcpRecvThread::connect ()
 */
void tcpRecvThread::connect (
    epicsGuard < epicsMutex > & guard )
{
    // attempt to connect to a CA server
    while ( true ) {
        int status;
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            osiSockAddr tmp = this->iiu.address ();
            status = ::connect ( this->iiu.sock, 
                            & tmp.sa, sizeof ( tmp.sa ) );
        }

        if ( this->iiu.state != tcpiiu::iiucs_connecting ) {
            break;
        }
        if ( status >= 0 ) {
            // put the iiu into the connected state
            this->iiu.state = tcpiiu::iiucs_connected;
            this->iiu.recvDog.connectNotify ( guard ); 
            break;
        }
        else {
            int errnoCpy = SOCKERRNO;

            if ( errnoCpy == SOCK_EINTR ) {
                continue;
            }
            else if ( errnoCpy == SOCK_SHUTDOWN ) {
                if ( ! this->iiu.isNameService () ) {
                    break;
                }
            }
            else {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( 
                    sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf ( "CAC: Unable to connect because \"%s\"\n",
                    sockErrBuf );
                if ( ! this->iiu.isNameService () ) {
                    this->iiu.disconnectNotify ( guard );
                    break;
                }
            }
            {
                double sleepTime = this->iiu.cacRef.connectionTimeout ( guard );
                epicsGuardRelease < epicsMutex > unguard ( guard );
                epicsThreadSleep ( sleepTime );
            }
            continue;
        }
    }
    return;
}

//
// tcpiiu::tcpiiu ()
//
tcpiiu::tcpiiu ( 
        cac & cac, epicsMutex & mutexIn, epicsMutex & cbMutexIn, 
        cacContextNotify & ctxNotifyIn, double connectionTimeout, 
        epicsTimerQueue & timerQueue, const osiSockAddr & addrIn, 
        comBufMemoryManager & comBufMemMgrIn,
        unsigned minorVersion, ipAddrToAsciiEngine & engineIn, 
        const cacChannel::priLev & priorityIn,
        SearchDestTCP * pSearchDestIn ) :
    caServerID ( addrIn.ia, priorityIn ),
    hostNameCacheInstance ( addrIn, engineIn ),
    recvThread ( *this, cbMutexIn, ctxNotifyIn, "CAC-TCP-recv", 
        epicsThreadGetStackSize ( epicsThreadStackBig ),
        cac::highestPriorityLevelBelow ( cac.getInitializingThreadsPriority() ) ),
    sendThread ( *this, "CAC-TCP-send",
        epicsThreadGetStackSize ( epicsThreadStackMedium ),
        cac::lowestPriorityLevelAbove (
            cac.getInitializingThreadsPriority() ) ),
    recvDog ( cbMutexIn, ctxNotifyIn, mutexIn, 
        *this, connectionTimeout, timerQueue ),
    sendDog ( cbMutexIn, ctxNotifyIn, mutexIn,
        *this, connectionTimeout, timerQueue ),
    sendQue ( *this, comBufMemMgrIn ),
    recvQue ( comBufMemMgrIn ),
    curDataMax ( MAX_TCP ),
    curDataBytes ( 0ul ),
    comBufMemMgr ( comBufMemMgrIn ),
    cacRef ( cac ),
    pCurData ( (char*) freeListMalloc(this->cacRef.tcpSmallRecvBufFreeList) ),
    pSearchDest ( pSearchDestIn ),
    mutex ( mutexIn ),
    cbMutex ( cbMutexIn ),
    minorProtocolVersion ( minorVersion ),
    state ( iiucs_connecting ),
    sock ( INVALID_SOCKET ),
    contigRecvMsgCount ( 0u ),
    blockingForFlush ( 0u ),
    socketLibrarySendBufferSize ( 0x1000 ),
    unacknowledgedSendBytes ( 0u ),
    channelCountTot ( 0u ),
    _receiveThreadIsBusy ( false ),
    busyStateDetected ( false ),
    flowControlActive ( false ),
    echoRequestPending ( false ),
    oldMsgHeaderAvailable ( false ),
    msgHeaderAvailable ( false ),
    earlyFlush ( false ),
    recvProcessPostponedFlush ( false ),
    discardingPendingData ( false ),
    socketHasBeenClosed ( false ),
    unresponsiveCircuit ( false )
{
    if(!pCurData)
        throw std::bad_alloc();

    this->sock = epicsSocketCreate ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( this->sock == INVALID_SOCKET ) {
        freeListFree(this->cacRef.tcpSmallRecvBufFreeList, this->pCurData);
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        std :: string reason = 
            "CAC: TCP circuit creation failure because \"";
        reason += sockErrBuf;
        reason += "\"";
        throw runtime_error ( reason );
    }

    int flag = true;
    int status = setsockopt ( this->sock, IPPROTO_TCP, TCP_NODELAY,
                (char *) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "CAC: problems setting socket option TCP_NODELAY = \"%s\"\n",
            sockErrBuf );
    }

    flag = true;
    status = setsockopt ( this->sock , SOL_SOCKET, SO_KEEPALIVE,
                ( char * ) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( 
            sockErrBuf, sizeof ( sockErrBuf ) );
        errlogPrintf ( "CAC: problems setting socket option SO_KEEPALIVE = \"%s\"\n",
            sockErrBuf );
    }

    // load message queue with messages informing server 
    // of version, user, and host name of client
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
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
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAC: problems setting socket option SO_SNDBUF = \"%s\"\n",
                sockErrBuf );
        }
        i = MAX_MSG_SIZE;
        status = setsockopt ( this->sock, SOL_SOCKET, SO_RCVBUF,
                ( char * ) &i, sizeof ( i ) );
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAC: problems setting socket option SO_RCVBUF = \"%s\"\n",
                sockErrBuf );
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
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ("CAC: problems getting socket option SO_SNDBUF = \"%s\"\n",
                sockErrBuf );
        }
        else {
            this->socketLibrarySendBufferSize = static_cast < unsigned > ( nBytes );
        }
    }

    if ( isNameService() ) {
        pSearchDest->setCircuit ( this );
    }

    memset ( (void *) &this->curMsg, '\0', sizeof ( this->curMsg ) );
}

// this must always be called by the udp thread when it holds 
// the callback lock.
void tcpiiu::start ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->recvThread.start ();
}

void tcpiiu::initiateCleanShutdown ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->state == iiucs_connected ) {
        if ( this->unresponsiveCircuit ) {
            this->initiateAbortShutdown ( guard );
        }
        else {
            this->state = iiucs_clean_shutdown;
            this->sendThreadFlushEvent.signal ();
            this->flushBlockEvent.signal ();
        }
    }
    else if ( this->state == iiucs_clean_shutdown ) {
        if ( this->unresponsiveCircuit ) {
            this->initiateAbortShutdown ( guard );
        }
    }
    else if ( this->state == iiucs_connecting ) {
        this->initiateAbortShutdown ( guard );
    }
}

void tcpiiu::disconnectNotify ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->state = iiucs_disconnected;
    this->sendThreadFlushEvent.signal ();
    this->flushBlockEvent.signal ();
}

void tcpiiu::responsiveCircuitNotify ( 
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->unresponsiveCircuit ) {
        this->unresponsiveCircuit = false;
        while ( nciu * pChan = this->unrespCircuit.get() ) {
            this->subscripUpdateReqPend.add ( *pChan );
            pChan->channelNode::listMember = 
                channelNode::cs_subscripUpdateReqPend;
            pChan->connect ( cbGuard, guard );
        }
        this->sendThreadFlushEvent.signal ();
    }
}

void tcpiiu::sendTimeoutNotify ( 
    callbackManager & mgr,
    epicsGuard < epicsMutex > & guard )
{
    mgr.cbGuard.assertIdenticalMutex ( this-> cbMutex );
    guard.assertIdenticalMutex ( this->mutex );
    this->unresponsiveCircuitNotify ( mgr.cbGuard, guard );
    // setup circuit probe sequence
    this->recvDog.sendTimeoutNotify ( mgr.cbGuard, guard );
}

void tcpiiu::receiveTimeoutNotify ( 
    callbackManager & mgr,
    epicsGuard < epicsMutex > & guard )
{
    mgr.cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );
    this->unresponsiveCircuitNotify ( mgr.cbGuard, guard );
}

void tcpiiu::unresponsiveCircuitNotify ( 
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & guard )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! this->unresponsiveCircuit ) {
        this->unresponsiveCircuit = true;
        this->echoRequestPending = true;
        this->sendThreadFlushEvent.signal ();
        this->flushBlockEvent.signal ();

        // must not hold lock when canceling timer
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            {
                epicsGuardRelease < epicsMutex > cbUnguard ( cbGuard );
                this->recvDog.cancel ();
                this->sendDog.cancel ();
            }
        }

        if ( this->connectedList.count() ) {
            char hostNameTmp[128];
            this->getHostName ( guard, hostNameTmp, sizeof ( hostNameTmp ) );
            genLocalExcep ( cbGuard, guard, this->cacRef, 
                ECA_UNRESPTMO, hostNameTmp );
            while ( nciu * pChan = this->connectedList.get () ) {
                // The cac lock is released herein so there is concern that
                // the list could be changed while we are traversing it.
                // However, this occurs only if a circuit disconnects,
                // a user deletes a channel, or a server disconnects a
                // channel. The callback lock must be taken in all of
                // these situations so this code is protected.
                this->unrespCircuit.add ( *pChan );
                pChan->channelNode::listMember = 
                    channelNode::cs_unrespCircuit;
                pChan->unresponsiveCircuitNotify ( cbGuard, guard );
            }
        }
    }
}

void tcpiiu::initiateAbortShutdown ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! this->discardingPendingData ) {
        // force abortive shutdown sequence 
        // (discard outstanding sends and receives)
        struct linger tmpLinger;
        tmpLinger.l_onoff = true;
        tmpLinger.l_linger = 0u;
        int status = setsockopt ( this->sock, SOL_SOCKET, SO_LINGER, 
            reinterpret_cast <char *> ( &tmpLinger ), sizeof (tmpLinger) );
        if ( status != 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( 
                sockErrBuf, sizeof ( sockErrBuf ) );
            errlogPrintf ( "CAC TCP socket linger set error was %s\n", 
                sockErrBuf );
        }
        this->discardingPendingData = true;
    }

    iiu_conn_state oldState = this->state;
    if ( oldState != iiucs_abort_shutdown && oldState != iiucs_disconnected ) {
        this->state = iiucs_abort_shutdown;

        epicsSocketSystemCallInterruptMechanismQueryInfo info  =
            epicsSocketSystemCallInterruptMechanismQuery ();
        switch ( info ) {
        case esscimqi_socketCloseRequired:
            //
            // on winsock and probably vxWorks shutdown() does not
            // unblock a thread in recv() so we use close() and introduce
            // some complexity because we must unregister the fd early
            //
            if ( ! this->socketHasBeenClosed ) {
                epicsSocketDestroy ( this->sock );
                this->socketHasBeenClosed = true;
            }
            break;
        case esscimqi_socketBothShutdownRequired:
            {
                int status = ::shutdown ( this->sock, SHUT_RDWR );
                if ( status ) {
                    char sockErrBuf[64];
                    epicsSocketConvertErrnoToString ( 
                        sockErrBuf, sizeof ( sockErrBuf ) );
                    errlogPrintf ("CAC TCP socket shutdown error was %s\n", 
                        sockErrBuf );
                }
            }
            break;
        case esscimqi_socketSigAlarmRequired:
            this->recvThread.interruptSocketRecv ();
            this->sendThread.interruptSocketSend ();
            break;
        default:
            break;
        };

        // 
        // wake up the send thread if it isnt blocking in send()
        //
        this->sendThreadFlushEvent.signal ();
        this->flushBlockEvent.signal ();
    }
}

//
// tcpiiu::~tcpiiu ()
//
tcpiiu :: ~tcpiiu ()
{
    if ( this->pSearchDest ) {
        this->pSearchDest->disable ();
    }

    this->sendThread.exitWait ();
    this->recvThread.exitWait ();
    this->sendDog.cancel ();
    this->recvDog.shutdown ();

    if ( ! this->socketHasBeenClosed ) {
        epicsSocketDestroy ( this->sock );
    }

    // free message body cache
    if ( this->pCurData ) {
        if ( this->curDataMax <= MAX_TCP ) {
            freeListFree(this->cacRef.tcpSmallRecvBufFreeList, this->pCurData);
        }
        else if ( this->cacRef.tcpLargeRecvBufFreeList ) {
            freeListFree(this->cacRef.tcpLargeRecvBufFreeList, this->pCurData);
        }
        else {
            free ( this->pCurData );
        }
    }
}

void tcpiiu::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > locker ( this->mutex );
    char buf[256];
    this->hostNameCacheInstance.getName ( buf, sizeof ( buf ) );
    ::printf ( "Virtual circuit to \"%s\" at version V%u.%u state %u\n", 
        buf, CA_MAJOR_PROTOCOL_REVISION,
        this->minorProtocolVersion, this->state );
    if ( level > 1u ) {
        ::printf ( "\tcurrent data cache pointer = %p current data cache size = %lu\n",
            static_cast < void * > ( this->pCurData ), this->curDataMax );
        ::printf ( "\tcontiguous receive message count=%u, busy detect bool=%u, flow control bool=%u\n", 
            this->contigRecvMsgCount, this->busyStateDetected, this->flowControlActive );
        ::printf ( "\receive thread is busy=%u\n", 
            this->_receiveThreadIsBusy );
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

        if ( this->createReqPend.count () ) {
            ::printf ( "Create request pending channels\n" );
            tsDLIterConst < nciu > pChan = this->createReqPend.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->createRespPend.count () ) {
            ::printf ( "Create response pending channels\n" );
            tsDLIterConst < nciu > pChan = this->createRespPend.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->v42ConnCallbackPend.count () ) {
            ::printf ( "V42 Conn Callback pending channels\n" );
            tsDLIterConst < nciu > pChan = this->v42ConnCallbackPend.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->subscripReqPend.count () ) {
            ::printf ( "Subscription request pending channels\n" );
            tsDLIterConst < nciu > pChan = this->subscripReqPend.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->connectedList.count () ) {
            ::printf ( "Connected channels\n" );
            tsDLIterConst < nciu > pChan = this->connectedList.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
        if ( this->unrespCircuit.count () ) {
            ::printf ( "Unresponsive circuit channels\n" );
            tsDLIterConst < nciu > pChan = this->unrespCircuit.firstIter ();
	        while ( pChan.valid () ) {
                pChan->show ( level - 2u );
                pChan++;
            }
        }
    }
}

bool tcpiiu::setEchoRequestPending ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    this->echoRequestPending = true;
    this->sendThreadFlushEvent.signal ();
    if ( CA_V43 ( this->minorProtocolVersion ) ) {
        // we send an echo
        return true;
    }
    else {
        // we send a NOOP
        return false;
    }
}

void tcpiiu::flushIfRecvProcessRequested (
    epicsGuard < epicsMutex > & guard )
{
    if ( this->recvProcessPostponedFlush ) {
        this->flushRequest ( guard );
        this->recvProcessPostponedFlush = false;
    }
}

bool tcpiiu::processIncoming ( 
    const epicsTime & currentTime, 
    callbackManager & mgr )
{
    mgr.cbGuard.assertIdenticalMutex ( this->cbMutex );

    while ( true ) {

        //
        // fetch a complete message header
        //
        if ( ! this->msgHeaderAvailable ) {
            if ( ! this->oldMsgHeaderAvailable ) {
                this->oldMsgHeaderAvailable = 
                    this->recvQue.popOldMsgHeader ( this->curMsg );
                if ( ! this->oldMsgHeaderAvailable ) {
                    epicsGuard < epicsMutex > guard ( this->mutex );
                    this->flushIfRecvProcessRequested ( guard );
                    return true;
                }
            }
            if ( this->curMsg.m_postsize == 0xffff ) {
                static const unsigned annexSize = 
                    sizeof ( this->curMsg.m_postsize ) + 
                    sizeof ( this->curMsg.m_count );
                if ( this->recvQue.occupiedBytes () < annexSize ) {
                    epicsGuard < epicsMutex > guard ( this->mutex );
                    this->flushIfRecvProcessRequested ( guard );
                    return true;
                }
                this->curMsg.m_postsize = this->recvQue.popUInt32 ();
                this->curMsg.m_count = this->recvQue.popUInt32 ();
            }
            this->msgHeaderAvailable = true;
#           ifdef DEBUG
                epicsGuard < epicsMutex > guard ( this->mutex );
                debugPrintf (
                    ( "%s Cmd=%3u Type=%3u Count=%8u Size=%8u",
                      this->pHostName ( guard ),
                      this->curMsg.m_cmmd,
                      this->curMsg.m_dataType,
                      this->curMsg.m_count,
                      this->curMsg.m_postsize) );
                debugPrintf (
                    ( " Avail=%8u Cid=%8u\n",
                      this->curMsg.m_available,
                      this->curMsg.m_cid) );
#           endif
        }

        // check for 8 byte aligned protocol
        if ( this->curMsg.m_postsize & 0x7 ) {
            this->printFormated ( mgr.cbGuard,
                "CAC: server sent missaligned payload 0x%x\n", 
                this->curMsg.m_postsize );
            return false;
        }

        //
        // make sure we have a large enough message body cache
        //
        if ( this->curMsg.m_postsize > this->curDataMax ) {
            assert (this->curMsg.m_postsize > MAX_TCP);

            char * newbuf = NULL;
            arrayElementCount newsize;

            if ( !this->cacRef.tcpLargeRecvBufFreeList ) {
                // round size up to multiple of 4K
                newsize = ((this->curMsg.m_postsize-1)|0xfff)+1;

                if ( this->curDataMax <= MAX_TCP ) {
                    // small -> large
                    newbuf = (char*)malloc(newsize);

                } else {
                    // expand large to larger
                    newbuf = (char*)realloc(this->pCurData, newsize);
                }

            } else if ( this->curMsg.m_postsize <= this->cacRef.maxRecvBytesTCP ) {
                newbuf = (char*) freeListMalloc(this->cacRef.tcpLargeRecvBufFreeList);
                newsize = this->cacRef.maxRecvBytesTCP;

            }

            if ( newbuf) {
                if (this->curDataMax <= MAX_TCP) {
                    freeListFree(this->cacRef.tcpSmallRecvBufFreeList, this->pCurData );

                } else if (this->cacRef.tcpLargeRecvBufFreeList) {
                    freeListFree(this->cacRef.tcpLargeRecvBufFreeList, this->pCurData );

                } else {
                    // called realloc()
                }
                this->pCurData = newbuf;
                this->curDataMax = newsize;

            } else {
                this->printFormated ( mgr.cbGuard,
                    "CAC: not enough memory for message body cache (ignoring response message)\n");
            }
        }

        if ( this->curMsg.m_postsize <= this->curDataMax ) {
            if ( this->curMsg.m_postsize > 0u ) {
                this->curDataBytes += this->recvQue.copyOutBytes ( 
                            &this->pCurData[this->curDataBytes], 
                            this->curMsg.m_postsize - this->curDataBytes );
                if ( this->curDataBytes < this->curMsg.m_postsize ) {
                    epicsGuard < epicsMutex > guard ( this->mutex );
                    this->flushIfRecvProcessRequested ( guard );
                    return true;
                }
            }
            bool msgOK = this->cacRef.executeResponse ( mgr, *this, 
                                currentTime, this->curMsg, this->pCurData );
            if ( ! msgOK ) {
                return false;
            }
        }
        else {
            static bool once = false;
            if ( ! once ) {
                this->printFormated ( mgr.cbGuard,
    "CAC: response with payload size=%u > EPICS_CA_MAX_ARRAY_BYTES ignored\n",
                    this->curMsg.m_postsize );
                once = true;
            }
            this->curDataBytes += this->recvQue.removeBytes ( 
                    this->curMsg.m_postsize - this->curDataBytes );
            if ( this->curDataBytes < this->curMsg.m_postsize  ) {
                epicsGuard < epicsMutex > guard ( this->mutex );
                this->flushIfRecvProcessRequested ( guard );
                return true;
            }
        }
 
        this->oldMsgHeaderAvailable = false;
        this->msgHeaderAvailable = false;
        this->curDataBytes = 0u;
    }
}

void tcpiiu::hostNameSetRequest ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! CA_V41 ( this->minorProtocolVersion ) ) {
        return;
    }
    
    const char * pName = this->cacRef.pLocalHostName ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( postSize + 16u ) ) {
        this->flushRequest ( guard );
    }

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_HOST_NAME, postSize, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( cacNillBytes, postSize - size );
    minder.commit ();
}

/*
 * tcpiiu::userNameSetRequest ()
 */
void tcpiiu::userNameSetRequest ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! CA_V41 ( this->minorProtocolVersion ) ) {
        return;
    }

    const char *pName = this->cacRef.userNamePointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( postSize + 16u ) ) {
        this->flushRequest ( guard );
    }

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_CLIENT_NAME, postSize, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( cacNillBytes, postSize - size );
    minder.commit ();
}

void tcpiiu::disableFlowControlRequest ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ( guard );
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENTS_ON, 0u, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::enableFlowControlRequest ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ( guard );
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENTS_OFF, 0u, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::versionMessage ( epicsGuard < epicsMutex > & guard,
                             const cacChannel::priLev & priority )
{
    guard.assertIdenticalMutex ( this->mutex );

    assert ( priority <= 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ( guard );
    }

    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_VERSION, 0u, 
        static_cast < ca_uint16_t > ( priority ), 
        CA_MINOR_PROTOCOL_REVISION, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::echoRequest ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    
    epicsUInt16 command = CA_PROTO_ECHO;
    if ( ! CA_V43 ( this->minorProtocolVersion ) ) {
        // we fake an echo to early server using a read sync
        command = CA_PROTO_READ_SYNC;
    }

    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ( guard );
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        command, 0u, 
        0u, 0u, 0u, 0u, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::writeRequest ( epicsGuard < epicsMutex > & guard,
    nciu &chan, unsigned type, arrayElementCount nElem, const void *pValue )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( INVALID_DB_REQ ( type ) ) {
        throw cacChannel::badType ();
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestWithPayLoad ( CA_PROTO_WRITE,  
        type, nElem, chan.getSID(guard), chan.getCID(guard), pValue,
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}


void tcpiiu::writeNotifyRequest ( epicsGuard < epicsMutex > & guard,
                                 nciu &chan, netWriteNotifyIO &io, unsigned type,  
                                arrayElementCount nElem, const void *pValue )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( ! this->ca_v41_ok ( guard ) ) {
        throw cacChannel::unsupportedByService();
    }
    if ( INVALID_DB_REQ ( type ) ) {
        throw cacChannel::badType ();
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestWithPayLoad ( CA_PROTO_WRITE_NOTIFY,  
        type, nElem, chan.getSID(guard), io.getId(), pValue,
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::readNotifyRequest ( epicsGuard < epicsMutex > & guard,
                               nciu & chan, netReadNotifyIO & io, 
                               unsigned dataType, arrayElementCount nElem )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( INVALID_DB_REQ ( dataType ) ) {
        throw cacChannel::badType ();
    }
    arrayElementCount maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = 0xfffffff0;
    }
    else {
        maxBytes = MAX_TCP;
    }
    arrayElementCount maxElem = 
        ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    if (nElem == 0 && !CA_V413(this->minorProtocolVersion))
       nElem = chan.getcount();
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_READ_NOTIFY, 0u, 
        static_cast < ca_uint16_t > ( dataType ), 
        static_cast < ca_uint32_t > ( nElem ), 
        chan.getSID(guard), io.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::createChannelRequest ( 
    nciu & chan, epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->state != iiucs_connected && 
        this->state != iiucs_connecting ) {
        return;
    }

    const char *pName;
    unsigned nameLength;
    ca_uint32_t identity;
    if ( this->ca_v44_ok ( guard ) ) {
        identity = chan.getCID ( guard );
        pName = chan.pName ( guard );
        nameLength = chan.nameLen ( guard );
    }
    else {
        identity = chan.getSID ( guard );
        pName = 0;
        nameLength = 0u;
    }

    unsigned postCnt = CA_MESSAGE_ALIGN ( nameLength );

    if ( postCnt >= 0xffff ) {
        throw cacChannel::unsupportedByService();
    }

    comQueSendMsgMinder minder ( this->sendQue, guard );
    //
    // The available field is used (abused)
    // here to communicate the minor version number
    // starting with CA 4.1.
    //
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_CREATE_CHAN, postCnt, 
        0u, 0u, identity, CA_MINOR_PROTOCOL_REVISION, 
        CA_V49 ( this->minorProtocolVersion ) );
    if ( nameLength ) {
        this->sendQue.pushString ( pName, nameLength );
    }
    if ( postCnt > nameLength ) {
        this->sendQue.pushString ( cacNillBytes, postCnt - nameLength );
    }
    minder.commit ();
}

void tcpiiu::clearChannelRequest ( epicsGuard < epicsMutex > & guard,
                                  ca_uint32_t sid, ca_uint32_t cid )
{
    guard.assertIdenticalMutex ( this->mutex );
    // there are situations where the circuit is disconnected, but
    // the channel does not know this yet
    if ( this->state != iiucs_connected ) {
        return;
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_CLEAR_CHANNEL, 0u, 
        0u, 0u, sid, cid, 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

//
// this routine return void because if this internally fails the best response
// is to try again the next time that we reconnect
//
void tcpiiu::subscriptionRequest ( 
    epicsGuard < epicsMutex > & guard,
    nciu & chan, netSubscription & subscr )
{
    guard.assertIdenticalMutex ( this->mutex );
    // there are situations where the circuit is disconnected, but
    // the channel does not know this yet
    if ( this->state != iiucs_connected && 
        this->state != iiucs_connecting ) {
        return;
    }
    unsigned mask = subscr.getMask(guard);
    if ( mask > 0xffff ) {
        throw cacChannel::badEventSelection ();
    }
    arrayElementCount nElem = subscr.getCount (
        guard, CA_V413(this->minorProtocolVersion) );
    arrayElementCount maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = 0xfffffff0;
    }
    else {
        maxBytes = MAX_TCP;
    }
    unsigned dataType = subscr.getType ( guard );
    // data type bounds checked when sunscription created
    arrayElementCount maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    // nElement bounds checked above
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENT_ADD, 16u, 
        static_cast < ca_uint16_t > ( dataType ), 
        static_cast < ca_uint32_t > ( nElem ), 
        chan.getSID(guard), subscr.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );

    // extension
    this->sendQue.pushFloat32 ( 0.0f ); // m_lval
    this->sendQue.pushFloat32 ( 0.0f ); // m_hval
    this->sendQue.pushFloat32 ( 0.0f ); // m_toval
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( mask ) ); // m_mask
    this->sendQue.pushUInt16 ( 0u ); // m_pad
    minder.commit ();
}

//
// this routine return void because if this internally fails the best response
// is to try again the next time that we reconnect
//
void tcpiiu::subscriptionUpdateRequest ( 
    epicsGuard < epicsMutex > & guard,
    nciu & chan, netSubscription & subscr )
{
    guard.assertIdenticalMutex ( this->mutex );
    // there are situations where the circuit is disconnected, but
    // the channel does not know this yet
    if ( this->state != iiucs_connected ) {
        return;
    }
    arrayElementCount nElem = subscr.getCount (
        guard, CA_V413(this->minorProtocolVersion) );
    arrayElementCount maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = 0xfffffff0;
    }
    else {
        maxBytes = MAX_TCP;
    }
    unsigned dataType = subscr.getType ( guard );
    // data type bounds checked when subscription constructed
    arrayElementCount maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    // nElem boounds checked above
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_READ_NOTIFY, 0u, 
        static_cast < ca_uint16_t > ( dataType ), 
        static_cast < ca_uint32_t > ( nElem ), 
        chan.getSID (guard), subscr.getId (), 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

void tcpiiu::subscriptionCancelRequest ( epicsGuard < epicsMutex > & guard,
                             nciu & chan, netSubscription & subscr )
{
    guard.assertIdenticalMutex ( this->mutex );
    // there are situations where the circuit is disconnected, but
    // the channel does not know this yet
    if ( this->state != iiucs_connected ) {
        return;
    }
    comQueSendMsgMinder minder ( this->sendQue, guard );
    this->sendQue.insertRequestHeader ( 
        CA_PROTO_EVENT_CANCEL, 0u, 
        static_cast < ca_uint16_t > ( subscr.getType ( guard ) ), 
        static_cast < ca_uint16_t > ( subscr.getCount (
            guard, CA_V413(this->minorProtocolVersion) ) ),
        chan.getSID(guard), subscr.getId(), 
        CA_V49 ( this->minorProtocolVersion ) );
    minder.commit ();
}

bool tcpiiu::sendThreadFlush ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );

    if ( this->sendQue.occupiedBytes() > 0 ) {
        while ( comBuf * pBuf = this->sendQue.popNextComBufToSend () ) {
            epicsTime current = epicsTime::getCurrent ();

            unsigned bytesToBeSent = pBuf->occupiedBytes ();
            bool success = false;
            {
                // no lock while blocking to send
                epicsGuardRelease < epicsMutex > unguard ( guard );
                success = pBuf->flushToWire ( *this, current );
                pBuf->~comBuf ();
                this->comBufMemMgr.release ( pBuf );
            }

            if ( ! success ) {
                while ( ( pBuf = this->sendQue.popNextComBufToSend () ) ) {
                    pBuf->~comBuf ();
                    this->comBufMemMgr.release ( pBuf );
                }
                return false;
            }

            // set it here with this odd order because we must have 
            // the lock and we must have already sent the bytes
            this->unacknowledgedSendBytes += bytesToBeSent;
            if ( this->unacknowledgedSendBytes > 
                this->socketLibrarySendBufferSize ) {
                this->recvDog.sendBacklogProgressNotify ( guard );
            }
        }
    }

    this->earlyFlush = false;
    if ( this->blockingForFlush ) {
        this->flushBlockEvent.signal ();
    }

    return true;
}

void tcpiiu :: flush ( epicsGuard < epicsMutex > & guard )
{
    this->flushRequest ( guard );
    // the process thread is not permitted to flush as this
    // can result in a push / pull deadlock on the TCP pipe.
    // Instead, the process thread scheduals the flush with the 
    // send thread which runs at a higher priority than the 
    // receive thread. The same applies to the UDP thread for
    // locking hierarchy reasons.
    if ( ! epicsThreadPrivateGet ( caClientCallbackThreadId ) ) {
        // enable / disable of call back preemption must occur here
        // because the tcpiiu might disconnect while waiting and its
        // pointer to this cac might become invalid            
        assert ( this->blockingForFlush < UINT_MAX );
        this->blockingForFlush++;
        while ( this->sendQue.flushBlockThreshold() ) {

            bool userRequestsCanBeAccepted =
                this->state == iiucs_connected ||
                ( ! this->ca_v42_ok ( guard ) && 
                    this->state == iiucs_connecting );
            // fail the users request if we have a disconnected
            // or unresponsive circuit
            if ( ! userRequestsCanBeAccepted || 
                    this->unresponsiveCircuit ) {
                this->decrementBlockingForFlushCount ( guard );
                throw cacChannel::notConnected ();
            }

            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->flushBlockEvent.wait ( 30.0 );
        }
        this->decrementBlockingForFlushCount ( guard );
    }
}

unsigned tcpiiu::requestMessageBytesPending ( 
        epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
#if 0
    if ( ! this->earlyFlush && this->sendQue.flushEarlyThreshold(0u) ) {
        this->earlyFlush = true;
        this->sendThreadFlushEvent.signal ();
    }
#endif
    return sendQue.occupiedBytes ();
}

void tcpiiu::decrementBlockingForFlushCount ( 
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    assert ( this->blockingForFlush > 0u );
    this->blockingForFlush--;
    if ( this->blockingForFlush > 0 ) {
        this->flushBlockEvent.signal ();
    }
}

osiSockAddr tcpiiu::getNetworkAddress (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->address();
}

// not inline because its virtual
bool tcpiiu::ca_v42_ok (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->mutex );
    return CA_V42 ( this->minorProtocolVersion );
}

void tcpiiu::requestRecvProcessPostponedFlush (
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->recvProcessPostponedFlush = true;
}

unsigned tcpiiu::getHostName ( 
    epicsGuard < epicsMutex > & guard,
    char * pBuf, unsigned bufLength ) const throw ()
{   
    guard.assertIdenticalMutex ( this->mutex );
    return this->hostNameCacheInstance.getName ( pBuf, bufLength );
}

const char * tcpiiu::pHostName (
    epicsGuard < epicsMutex > & guard ) const throw ()
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->hostNameCacheInstance.pointer ();
}

void tcpiiu::disconnectAllChannels ( 
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & guard,
    class udpiiu & discIIU )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );

    while ( nciu * pChan = this->createReqPend.get () ) {
        discIIU.installDisconnectedChannel ( guard, *pChan );
    }

    while ( nciu * pChan = this->createRespPend.get () ) {
        // we dont yet know the server's id so we cant
        // send a channel delete request and will instead 
        // trust that the server can do the proper cleanup
        // when the circuit disconnects
        discIIU.installDisconnectedChannel ( guard, *pChan );
    }
    
    while ( nciu * pChan = this->v42ConnCallbackPend.get () ) {
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        discIIU.installDisconnectedChannel ( guard, *pChan );
    }

    while ( nciu * pChan = this->subscripReqPend.get () ) {
        pChan->disconnectAllIO ( cbGuard, guard );
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        discIIU.installDisconnectedChannel ( guard, *pChan );
        pChan->unresponsiveCircuitNotify ( cbGuard, guard );
    }

    while ( nciu * pChan = this->connectedList.get () ) {
        pChan->disconnectAllIO ( cbGuard, guard );
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        discIIU.installDisconnectedChannel ( guard, *pChan );
        pChan->unresponsiveCircuitNotify ( cbGuard, guard );
    }

    while ( nciu * pChan = this->unrespCircuit.get () ) {
        // if we know that the circuit is unresponsive
        // then we dont send a channel delete request and 
        // will instead trust that the server can do the 
        // proper cleanup when the circuit disconnects
        pChan->disconnectAllIO ( cbGuard, guard );
        discIIU.installDisconnectedChannel ( guard, *pChan );
    }
    
    while ( nciu * pChan = this->subscripUpdateReqPend.get () ) {
        pChan->disconnectAllIO ( cbGuard, guard );
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        discIIU.installDisconnectedChannel ( guard, *pChan );
        pChan->unresponsiveCircuitNotify ( cbGuard, guard );
    }

    this->channelCountTot = 0u;
    this->initiateCleanShutdown ( guard );
}

void tcpiiu::unlinkAllChannels ( 
    epicsGuard < epicsMutex > & cbGuard, 
    epicsGuard < epicsMutex > & guard )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );
    guard.assertIdenticalMutex ( this->mutex );

    while ( nciu * pChan = this->createReqPend.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }

    while ( nciu * pChan = this->createRespPend.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        // we dont yet know the server's id so we cant
        // send a channel delete request and will instead 
        // trust that the server can do the proper cleanup
        // when the circuit disconnects
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }
    
    while ( nciu * pChan = this->v42ConnCallbackPend.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }

    while ( nciu * pChan = this->subscripReqPend.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        pChan->disconnectAllIO ( cbGuard, guard );
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }

    while ( nciu * pChan = this->connectedList.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        pChan->disconnectAllIO ( cbGuard, guard );
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }

    while ( nciu * pChan = this->unrespCircuit.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        pChan->disconnectAllIO ( cbGuard, guard );
        // if we know that the circuit is unresponsive
        // then we dont send a channel delete request and 
        // will instead trust that the server can do the 
        // proper cleanup when the circuit disconnects
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }
    
     while ( nciu * pChan = this->subscripUpdateReqPend.get () ) {
        pChan->channelNode::listMember = 
            channelNode::cs_none;
        pChan->disconnectAllIO ( cbGuard, guard );
        this->clearChannelRequest ( guard, 
            pChan->getSID(guard), pChan->getCID(guard) );
        pChan->serviceShutdownNotify ( cbGuard, guard );
    }

    this->channelCountTot = 0u;
    this->initiateCleanShutdown ( guard );
}

void tcpiiu::installChannel ( 
    epicsGuard < epicsMutex > & guard, 
    nciu & chan, unsigned sidIn, 
    ca_uint16_t typeIn, arrayElementCount countIn )
{
    guard.assertIdenticalMutex ( this->mutex );

    this->createReqPend.add ( chan );
    this->channelCountTot++;
    chan.channelNode::listMember = channelNode::cs_createReqPend;
    chan.searchReplySetUp ( *this, sidIn, typeIn, countIn, guard );
    // The tcp send thread runs at apriority below the udp thread 
    // so that this will not send small packets
    this->sendThreadFlushEvent.signal ();
}

bool tcpiiu :: connectNotify ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );
    bool wasExpected = false;
    // this improves robustness in the face of a server sending
    // protocol that does not match its declared protocol revision
    if ( chan.channelNode::listMember == channelNode::cs_createRespPend ) {
        this->createRespPend.remove ( chan );
        this->subscripReqPend.add ( chan );
        chan.channelNode::listMember = channelNode::cs_subscripReqPend;
        wasExpected = true;
    }
    else if ( chan.channelNode::listMember == channelNode::cs_v42ConnCallbackPend ) {
        this->v42ConnCallbackPend.remove ( chan );
        this->subscripReqPend.add ( chan );
        chan.channelNode::listMember = channelNode::cs_subscripReqPend;
        wasExpected = true;
    }
    // the TCP send thread is awakened by its receive thread whenever the receive thread
    // is about to block if this->subscripReqPend has items in it
    return wasExpected;
}

void tcpiiu::uninstallChan ( 
    epicsGuard < epicsMutex > & guard, nciu & chan )
{
    guard.assertIdenticalMutex ( this->mutex );

    switch ( chan.channelNode::listMember ) {
    case channelNode::cs_createReqPend:
        this->createReqPend.remove ( chan );
        break;
    case channelNode::cs_createRespPend:
        this->createRespPend.remove ( chan );
        break;
    case channelNode::cs_v42ConnCallbackPend:
        this->v42ConnCallbackPend.remove ( chan );
        break;
    case channelNode::cs_subscripReqPend:
        this->subscripReqPend.remove ( chan );
        break;
    case channelNode::cs_connected:
        this->connectedList.remove ( chan );
        break;
    case channelNode::cs_unrespCircuit:
        this->unrespCircuit.remove ( chan );
        break;
    case channelNode::cs_subscripUpdateReqPend:
        this->subscripUpdateReqPend.remove ( chan );
        break;
    default:
        errlogPrintf ( 
            "cac: attempt to uninstall channel from tcp iiu, but it inst installed there?" );
    }
    chan.channelNode::listMember = channelNode::cs_none;
    this->channelCountTot--;
    if ( this->channelCountTot == 0 && ! this->isNameService() ) {
        this->initiateCleanShutdown ( guard );
    }
}

int tcpiiu :: printFormated ( 
    epicsGuard < epicsMutex > & cbGuard, 
    const char *pformat, ... )
{
    cbGuard.assertIdenticalMutex ( this->cbMutex );

    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->cacRef.varArgsPrintFormated ( cbGuard, pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void tcpiiu::flushRequest ( epicsGuard < epicsMutex > & )
{
    if ( this->sendQue.occupiedBytes () > 0 ) {
        this->sendThreadFlushEvent.signal ();
    }
}

bool tcpiiu::bytesArePendingInOS () const
{
#if 0
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
    osiSockIoctl_t bytesPending = 0; /* shut up purifys yapping */
    int status = socket_ioctl ( this->sock,
                            FIONREAD, & bytesPending );
    if ( status >= 0 ) {
        if ( bytesPending > 0 ) {
            return true;
        }
    }
    return false;
#endif
}

double tcpiiu::receiveWatchdogDelay (
    epicsGuard < epicsMutex > & ) const
{
    return this->recvDog.delay ();
}

/*
 * Certain OS, such as HPUX, do not unblock a socket system call 
 * when another thread asynchronously calls both shutdown() and 
 * close(). To solve this problem we need to employ OS specific
 * mechanisms.
 */
void tcpRecvThread::interruptSocketRecv ()
{
    epicsThreadId threadId = this->thread.getId ();
    if ( threadId ) {
        epicsSignalRaiseSigAlarm ( threadId );
    }
}
void tcpSendThread::interruptSocketSend ()
{
    epicsThreadId threadId = this->thread.getId ();
    if ( threadId ) {
        epicsSignalRaiseSigAlarm ( threadId );
    }
}

void tcpiiu::operator delete ( void * /* pCadaver */ )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about "
        "placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

unsigned tcpiiu::channelCount ( epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->channelCountTot;
}

void tcpiiu::uninstallChanDueToSuccessfulSearchResponse ( 
    epicsGuard < epicsMutex > & guard, nciu & chan, 
    const class epicsTime & currentTime )
{
    netiiu::uninstallChanDueToSuccessfulSearchResponse (
        guard, chan, currentTime );
}

bool tcpiiu::searchMsg (
    epicsGuard < epicsMutex > & guard, ca_uint32_t id, 
        const char * pName, unsigned nameLength )
{
    return netiiu::searchMsg (
        guard, id, pName, nameLength );
}

SearchDestTCP :: SearchDestTCP (
    cac & cacIn, const osiSockAddr & addrIn ) :
    _ptcpiiu ( NULL ),
    _cac ( cacIn ),
    _addr ( addrIn ),
    _active ( false )
{
}

void SearchDestTCP :: disable ()
{
    _active = false;
    _ptcpiiu = NULL;
}

void SearchDestTCP :: enable ()
{
    _active = true;
}

void SearchDestTCP :: searchRequest (
    epicsGuard < epicsMutex > & guard,
        const char * pBuf, size_t len  )
{
    // restart circuit if it was shut down
    if ( ! _ptcpiiu ) {
        tcpiiu * piiu = NULL;
        bool newIIU = _cac.findOrCreateVirtCircuit (
            guard, _addr, cacChannel::priorityDefault,
            piiu, CA_UKN_MINOR_VERSION, this );
        if ( newIIU ) {
            piiu->start ( guard );
        }
        _ptcpiiu = piiu;
    }

    // does this server support TCP-based name resolution?
    if ( CA_V412 ( _ptcpiiu->minorProtocolVersion ) ) {
        guard.assertIdenticalMutex ( _ptcpiiu->mutex );
        assert ( CA_MESSAGE_ALIGN ( len ) == len );
        comQueSendMsgMinder minder ( _ptcpiiu->sendQue, guard );
        _ptcpiiu->sendQue.pushString ( pBuf, len );
        minder.commit ();
        _ptcpiiu->flushRequest ( guard );
    }
}

void SearchDestTCP :: show (
    epicsGuard < epicsMutex > & guard, unsigned level ) const
{
    :: printf ( "tcpiiu :: SearchDestTCP\n" );
}

void tcpiiu :: versionRespNotify ( const caHdrLargeArray & msg )
{
    this->minorProtocolVersion = msg.m_count;
}

void tcpiiu :: searchRespNotify (
    const epicsTime & currentTime, const caHdrLargeArray & msg )
{    
   /*
     * the type field is abused to carry the port number
     * so that we can have multiple servers on one host
     */
    osiSockAddr serverAddr;
    if ( msg.m_cid != INADDR_BROADCAST ) {
        serverAddr.ia.sin_family = AF_INET;
        serverAddr.ia.sin_addr.s_addr = htonl ( msg.m_cid );
        serverAddr.ia.sin_port = htons ( msg.m_dataType );
    }
    else {
        serverAddr = this->address ();
    }
    cacRef.transferChanToVirtCircuit 
            ( msg.m_available, msg.m_cid, 0xffff, 
                0, minorProtocolVersion, serverAddr, currentTime );
}
