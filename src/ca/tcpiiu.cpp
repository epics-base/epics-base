

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
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "localHostName.h"
#include "iocinf.h"
#include "virtualCircuit.h"
#include "inetAddrID.h"
#include "cac.h"
#include "netiiu.h"
#include "netIO.h"
#include "msgForMultiplyDefinedPV.h"

#define epicsExportSharedSymbols
#include "net_convert.h"
#include "bhe.h"
#undef epicsExportSharedSymbols

// nill message alignment pad bytes
static const char nillBytes [] = 
{ 
    0, 0, 0, 0,
    0, 0, 0, 0
};

//
//  cacSendThreadTCP ()
//
// care is taken to not hold the lock while sending a message
//
extern "C" void cacSendThreadTCP ( void *pParam )
{
    tcpiiu *piiu = ( tcpiiu * ) pParam;

    try {
        while ( true ) {
            bool flowControlLaborNeeded;
            bool echoLaborNeeded;

            piiu->sendThreadFlushEvent.wait ();

            if ( piiu->state != iiu_connected ) {
                break;
            }

            {
                epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
                flowControlLaborNeeded = 
                    piiu->busyStateDetected != piiu->flowControlActive;
                echoLaborNeeded = piiu->echoRequestPending;
                piiu->echoRequestPending = false;
            }

            if ( flowControlLaborNeeded ) {
                if ( piiu->flowControlActive ) {
                    piiu->disableFlowControlRequest ();
                    piiu->flowControlActive = false;
                    debugPrintf ( ( "fc off\n" ) );
                }
                else {
                    piiu->enableFlowControlRequest ();
                    piiu->flowControlActive = true;
                    debugPrintf ( ( "fc on\n" ) );
                }
            }

            if ( echoLaborNeeded ) {
                if ( CA_V43 ( piiu->minorProtocolVersion ) ) {
                    piiu->echoRequest ();
                }
                else {
                    piiu->noopRequest ();
                }
            }

            if ( ! piiu->flush () ) {
                break;
            }
        }
    }
    catch ( ... ) {
        piiu->printf ("cac: tcp send thread received an exception - diconnecting\n");
        piiu->forcedShutdown ();
    }

    piiu->sendThreadExitEvent.signal ();
}

unsigned tcpiiu::sendBytes ( const void *pBuf, 
                            unsigned nBytesInBuf )
{
    int status;
    unsigned nBytes;

    if ( this->state != iiu_connected ) {
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

            if ( status == 0 ) {
                this->cleanShutdown ();
                nBytes = 0u;
                break;
            }

            if ( localError == SOCK_SHUTDOWN ) {
                nBytes = 0u;
                break;
            }

            if ( localError == SOCK_EINTR ) {
                continue;
            }

            if ( localError != SOCK_EPIPE && localError != SOCK_ECONNRESET &&
                localError != SOCK_ETIMEDOUT && localError != SOCK_ECONNABORTED ) {
                this->printf ( "CAC: unexpected TCP send error: %s\n", SOCKERRSTR (localError) );
            }

            this->cleanShutdown ();
            nBytes = 0u;
            break;
        }
    }

    this->sendDog.cancel ();

    return nBytes;
}

unsigned tcpiiu::recvBytes ( void *pBuf, unsigned nBytesInBuf )
{
    if ( this->state != iiu_connected ) {
        return 0u;
    }

    assert ( nBytesInBuf <= INT_MAX );
    int status = ::recv ( this->sock, static_cast <char *> ( pBuf ), 
        static_cast <int> ( nBytesInBuf ), 0);
    if ( status <= 0 ) {
        int localErrno = SOCKERRNO;

        if ( status == 0 ) {
            this->cleanShutdown ();
            return 0u;
        }

        if ( localErrno == SOCK_SHUTDOWN ) {
            return 0u;
        }

        if ( localErrno == SOCK_EINTR ) {
            return 0u;
        }
        
        if ( localErrno == SOCK_ECONNABORTED ) {
            return 0u;
        }

        if ( localErrno == SOCK_ECONNRESET ) {
            return 0u;
        }

        {
            char name[64];
            this->hostName ( name, sizeof ( name ) );
            this->printf ( "Disconnecting from CA server %s because: %s\n", 
                name, SOCKERRSTR ( localErrno ) );
        }

        this->cleanShutdown ();

        return 0u;
    }
    
    assert ( static_cast <unsigned> ( status ) <= nBytesInBuf );
    return static_cast <unsigned> ( status );
}

/*
 *  cacRecvThreadTCP ()
 */
extern "C" void cacRecvThreadTCP ( void *pParam )
{
    tcpiiu *piiu = ( tcpiiu * ) pParam;

    piiu->connect ();

    {
        epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
        if ( piiu->state == iiu_connected ) {
            unsigned priorityOfSend;
            epicsThreadBooleanStatus tbs;
            epicsThreadId tid;

            tbs  = epicsThreadLowestPriorityLevelAbove ( piiu->pCAC ()->getInitializingThreadsPriority (), &priorityOfSend );
            if ( tbs != epicsThreadBooleanStatusSuccess ) {
                priorityOfSend = piiu->pCAC ()->getInitializingThreadsPriority ();
            }
            tid = epicsThreadCreate ( "CAC-TCP-send", priorityOfSend,
                    epicsThreadGetStackSize ( epicsThreadStackMedium ), cacSendThreadTCP, piiu );
            if ( ! tid ) {
                piiu->recvThreadExitEvent.signal ();
                piiu->sendThreadExitEvent.signal ();
                piiu->cleanShutdown ();
                return;
            }
        }
        else {
            piiu->recvThreadExitEvent.signal ();
            piiu->sendThreadExitEvent.signal ();
            piiu->cleanShutdown ();
            return;
        }
    }

    unsigned nBytes = 0u;
    while ( piiu->state == iiu_connected ) {
        if ( nBytes >= maxBytesPendingTCP ) {
            piiu->recvThreadRingBufferSpaceAvailableEvent.wait ();
            epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
            nBytes = piiu->recvQue.occupiedBytes ();
        }
        else {
            comBuf * pComBuf = new ( std::nothrow ) comBuf;
            if ( pComBuf ) {
                unsigned nBytesIn = pComBuf->fillFromWire ( *piiu );
                if ( nBytesIn ) {
                    bool msgHeaderButNoBody;
                    {
                        epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
                        nBytes = piiu->recvQue.occupiedBytes ();
                        msgHeaderButNoBody = piiu->oldMsgHeaderAvailable; 
                        piiu->recvQue.pushLastComBufReceived ( *pComBuf );
                        if ( nBytesIn == pComBuf->capacityBytes () ) {
                            if ( piiu->contigRecvMsgCount >= contiguousMsgCountWhichTriggersFlowControl ) {
                                piiu->busyStateDetected = true;
                            }
                            else { 
                                piiu->contigRecvMsgCount++;
                            }
                        }
                        else {
                            piiu->contigRecvMsgCount = 0u;
                            piiu->busyStateDetected = false;
                        }         
                        piiu->unacknowledgedSendBytes = 0u;
                        // reschedule connection activity watchdog
                        piiu->recvDog.messageArrivalNotify (); 
                    }

                    // wake up recv thread only if
                    // 1) there are currently no bytes in the queue
                    // 2) if the recv thread is currently blocking for an incomplete msg
                    if ( nBytes < sizeof ( caHdr ) || msgHeaderButNoBody ) {
                        piiu->pCAC()->signalRecvActivity ();
                    }

                    if ( nBytes <= UINT_MAX - nBytesIn ) {
                        nBytes += nBytesIn;
                    }
                    else {
                        nBytes = UINT_MAX;
                    }
                }
                else {
                    pComBuf->destroy ();
                    epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
                    nBytes = piiu->recvQue.occupiedBytes ();
                }
            }
            else {
                // no way to be informed when memory is available
                epicsThreadSleep ( 0.5 );
                epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
                nBytes = piiu->recvQue.occupiedBytes ();
            }
        }
    }

    piiu->recvThreadExitEvent.signal ();
}

//
// tcpiiu::tcpiiu ()
//
tcpiiu::tcpiiu ( cac &cac, double connectionTimeout, 
        epicsTimerQueue &timerQueue, const osiSockAddr &addrIn, 
        unsigned minorVersion, class bhe &bheIn, 
        ipAddrToAsciiEngine &engineIn ) :
    netiiu ( &cac ),
    recvDog ( *this, connectionTimeout, timerQueue ),
    sendDog ( *this, connectionTimeout, timerQueue ),
    sendQue ( *this ),
    addr ( addrIn ),
    curDataMax ( MAX_TCP ),
    curDataBytes ( 0ul ),
    pHostNameCache ( new hostNameCache ( addrIn, engineIn ) ),
    BHE ( bheIn ),
    pCurData ( cac.allocateSmallBufferTCP () ),
    minorProtocolVersion ( minorVersion ),
    state ( iiu_connecting ),
    sock ( INVALID_SOCKET ),
    contigRecvMsgCount ( 0u ),
    blockingForFlush ( 0u ),
    socketLibrarySendBufferSize ( 0u ),
    unacknowledgedSendBytes ( 0u ),
    busyStateDetected ( false ),
    flowControlActive ( false ),
    echoRequestPending ( false ),
    oldMsgHeaderAvailable ( false ),
    msgHeaderAvailable ( false ),
    sockCloseCompleted ( false ),
    f_trueOnceOnly ( true ),
    earlyFlush ( false )
{
    if ( ! this->pCurData ) {
        throw std::bad_alloc ();
    }

    if ( ! this->pHostNameCache.get () ) {
        throw std::bad_alloc ();
    }

    this->BHE.bindToIIU ( *this );

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
    // of user and host name of client
    this->userNameSetRequest ();
    this->hostNameSetRequest ();


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
            this->socketLibrarySendBufferSize = 0u;
        }
        else {
            this->socketLibrarySendBufferSize = static_cast < unsigned > ( nBytes );
        }
    }

    memset ( (void *) &this->curMsg, '\0', sizeof ( this->curMsg ) );

    unsigned priorityOfRecv;
    epicsThreadBooleanStatus tbs = epicsThreadHighestPriorityLevelBelow ( 
        this->pCAC ()->getInitializingThreadsPriority (), &priorityOfRecv );
    if ( tbs != epicsThreadBooleanStatusSuccess ) {
        priorityOfRecv = this->pCAC ()->getInitializingThreadsPriority ();
    }

    epicsThreadId tid = epicsThreadCreate ( "CAC-TCP-recv", priorityOfRecv,
            epicsThreadGetStackSize ( epicsThreadStackMedium ), 
            cacRecvThreadTCP, this);
    if ( tid == 0 ) {
        this->printf ("CA: unable to create CA client receive thread\n");
        cac.releaseSmallBufferTCP ( this->pCurData );
        socket_close ( this->sock );
        throw std::bad_alloc ();
    }
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
    while ( ! this->sockCloseCompleted ) {

        int status = ::connect ( this->sock, &this->addr.sa, sizeof ( addr.sa ) );

        if ( status == 0 ) {

            this->sendDog.cancel ();

            epicsAutoMutex autoMutex ( this->pCAC()->mutexRef() );

            if ( this->state == iiu_connecting ) {
                // put the iiu into the connected state
                this->state = iiu_connected;

                // start connection activity watchdog
                this->recvDog.connectNotify (); 
            }

            return;
        }

        int errnoCpy = SOCKERRNO;

        if ( errnoCpy == SOCK_EINTR ) {
            if ( this->state != iiu_connecting ) {
                this->sendDog.cancel ();
                return;
            }
            else {
                continue;
            }
        }
        else if ( errnoCpy == SOCK_SHUTDOWN ) {
            this->sendDog.cancel ();
            return;
        }
        else {  
            this->sendDog.cancel ();
            this->printf ( "Unable to connect because %d=\"%s\"\n", 
                errnoCpy, SOCKERRSTR ( errnoCpy ) );
            this->cleanShutdown ();
            return;
        }
    }
}

/*
 *  tcpiiu::cleanShutdown ()
 */
void tcpiiu::cleanShutdown ()
{
    epicsAutoMutex autoMutex ( this->pCAC()->mutexRef() );

    if ( this->state == iiu_connected ) {
        int status = ::shutdown ( this->sock, SD_BOTH );
        if ( status ) {
            errlogPrintf ("CAC TCP socket shutdown error was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
            status = socket_close ( this->sock );
            if ( status ) {
                errlogPrintf ("CAC TCP socket close error was %s\n", 
                    SOCKERRSTR (SOCKERRNO) );
            }
            else {
                this->sockCloseCompleted = true;
                this->state = iiu_disconnected;
            }
        }
        else {
            this->state = iiu_disconnected;
        }
    }
    else if ( this->state == iiu_connecting ) {
        int status = socket_close ( this->sock );
        if ( status ) {
            errlogPrintf ("CAC TCP socket close error was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
        }
        else {
            this->sockCloseCompleted = true;
            this->state = iiu_disconnected;
        }
    }
    this->sendThreadFlushEvent.signal ();
    this->recvThreadRingBufferSpaceAvailableEvent.signal ();
    this->pCAC ()->signalRecvActivity ();
}

/*
 *  tcpiiu::forcedShutdown ()
 */
void tcpiiu::forcedShutdown ()
{
    epicsAutoMutex autoMutex ( this->pCAC()->mutexRef() );

    if ( this->state != iiu_disconnected ) {
        // force abortive shutdown sequence (discard outstanding sends
        // and receives
        struct linger tmpLinger;
        tmpLinger.l_onoff = true;
        tmpLinger.l_linger = 0u;
        int status = setsockopt ( this->sock, SOL_SOCKET, SO_LINGER, 
            reinterpret_cast <char *> ( &tmpLinger ), sizeof (tmpLinger) );
        if ( status != 0 ) {
            errlogPrintf ( "CAC TCP socket linger set error was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
        }

        status = socket_close ( this->sock );
        if ( status ) {
            errlogPrintf ("CAC TCP socket close error was %s\n", 
                SOCKERRSTR (SOCKERRNO) );
        }
        else {
            this->state = iiu_disconnected;
            this->sockCloseCompleted = true;
        }
    }

    this->sendThreadFlushEvent.signal ();
    this->recvThreadRingBufferSpaceAvailableEvent.signal ();
    this->pCAC()->signalRecvActivity ();
}

//
// tcpiiu::~tcpiiu ()
//
tcpiiu::~tcpiiu ()
{
    this->sendDog.cancel ();
    this->recvDog.cancel ();

    this->cleanShutdown ();

    // wait for send thread to exit
    static const double shutdownDelay = 15.0;
    while ( true ) {
        bool signaled = this->sendThreadExitEvent.wait ( shutdownDelay );
        if ( signaled ) {
            break;
        }
        if ( ! this->sockCloseCompleted ) {
            printf ( "Gave up waiting for \"shutdown()\" to force send thread to exit after %f sec\n", 
                shutdownDelay);
            printf ( "Closing socket\n" );
            int status = socket_close ( this->sock );
            if ( status ) {
                errlogPrintf ("CAC TCP socket close error was %s\n", 
                    SOCKERRSTR ( SOCKERRNO ) );
            }
            else {
                this->sockCloseCompleted = true;
            }
        }
    }

    // wait for recv thread to exit
    while ( true ) {
        bool signaled = this->recvThreadExitEvent.wait ( shutdownDelay );
        if ( signaled ) {
            break;
        }
        if ( ! this->sockCloseCompleted ) {
            printf ( "Gave up waiting for \"shutdown()\" to force receive thread to exit after %f sec\n", 
                shutdownDelay);
            printf ( "Closing socket\n" );
            int status = socket_close ( this->sock );
            if ( status ) {
                errlogPrintf ("CAC TCP socket close error was %s\n", 
                    SOCKERRSTR ( SOCKERRNO ) );
            }
            else {
                this->sockCloseCompleted = true;
            }
        }
    }

    if ( ! this->sockCloseCompleted ) {
        int status = socket_close ( this->sock );
        if ( status ) {
            errlogPrintf ("CAC TCP socket close error was %s\n", 
                SOCKERRSTR ( SOCKERRNO ) );
        }
        else {
            this->sockCloseCompleted = true;
        }
    }

    // free message body cache
    if ( this->pCurData ) {
        if ( this->curDataMax == MAX_TCP ) {
            this->pCAC()->releaseSmallBufferTCP ( this->pCurData );
        }
        else {
            this->pCAC()->releaseLargeBufferTCP ( this->pCurData );
        }
    }

    // wakeup user threads blocking for send backlog to be reduced
    // and wait for them to stop using this IIU
    this->flushBlockEvent.signal ();
    while ( this->blockingForFlush ) {
        epicsThreadSleep ( 0.1 );
    }
}

void tcpiiu::destroy ()
{
    delete this;
}

bool tcpiiu::isVirtaulCircuit ( const char *pChannelName, const osiSockAddr &addrIn ) const
{
    osiSockAddr addrTmp = this->addr;

    if ( addrTmp.sa.sa_family == AF_UNSPEC ) {
        return false;
    }

    bool match;

    if ( addrTmp.sa.sa_family != addrIn.sa.sa_family ) {
        match = false;
    }
    else if ( addrTmp.sa.sa_family != AF_INET ) {
        match = false;
    }
    else if ( addrTmp.ia.sin_addr.s_addr !=  addrIn.ia.sin_addr.s_addr ) {
        match = false;
    }
    else if ( addrTmp.ia.sin_port !=  addrIn.ia.sin_port ) { 
        match = false;
    }
    else {
        match = true;
    }

    if ( ! match ) {
        epicsAutoMutex locker ( this->pCAC()->mutexRef() );
        char acc[64];
        this->pHostNameCache->hostName ( acc, sizeof ( acc ) );
        assert ( this->pCAC () );
        msgForMultiplyDefinedPV *pMsg = new msgForMultiplyDefinedPV ( 
            *this->pCAC (), pChannelName, acc, addrIn );
        if ( pMsg ) {
            this->pCAC ()->ipAddrToAsciiAsynchronousRequestInstall ( *pMsg );
        }
    }

    return true;
}

void tcpiiu::show ( unsigned level ) const
{
    epicsAutoMutex locker ( this->pCAC()->mutexRef() );
    char buf[256];
    this->pHostNameCache->hostName ( buf, sizeof ( buf ) );
    ::printf ( "Virtual circuit to \"%s\" at version V%u.%u state %u\n", 
        buf, CA_MAJOR_PROTOCOL_REVISION,
        this->minorProtocolVersion, this->state );
    if ( level > 1u ) {
        this->netiiu::show ( level - 1u );
    }
    if ( level > 2u ) {
        ::printf ( "\tcurrent data cache pointer = %p current data cache size = %lu\n",
            static_cast < void * > ( this->pCurData ), this->curDataMax );
        ::printf ( "\tcontiguous receive message count=%u, busy detect bool=%u, flow control bool=%u\n", 
            this->contigRecvMsgCount, this->busyStateDetected, this->flowControlActive );
    }
    if ( level > 3u ) {
        ::printf ( "\tvirtual circuit socket identifier %d\n", this->sock );
        ::printf ( "\tsend thread flush signal:\n" );
        this->sendThreadFlushEvent.show ( level-3u );
        ::printf ( "\trecv thread buffer space available signal:\n" );
        this->recvThreadRingBufferSpaceAvailableEvent.show ( level-3u );
        ::printf ( "\tsend thread exit signal:\n" );
        this->sendThreadExitEvent.show ( level-3u );
        ::printf ( "\trecv thread exit signal:\n" );
        this->recvThreadExitEvent.show ( level-3u );
        ::printf ("\techo pending bool = %u\n", this->echoRequestPending );
        ::printf ( "IO identifier hash table:\n" );
        this->BHE.show ( level - 3u );
    }
}

bool tcpiiu::setEchoRequestPending ()
{
    {
        epicsAutoMutex locker ( this->pCAC()->mutexRef() );
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
void tcpiiu::processIncoming ()
{
    while ( 1 ) {

        //
        // fetch a complete message header
        //
        unsigned nBytes = this->recvQue.occupiedBytes ();

        if ( ! this->msgHeaderAvailable ) {
            if ( ! this->oldMsgHeaderAvailable ) {
                if ( nBytes < sizeof ( caHdr ) ) {
                    return;
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
                    return;
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
                    this->pCAC()->largeBufferSizeTCP() >= this->curMsg.m_postsize ) {
                char * pBuf = this->pCAC()->allocateLargeBufferTCP ();
                if ( pBuf ) {
                    this->pCAC()->releaseSmallBufferTCP ( this->pCurData );
                    this->pCurData = pBuf;
                    this->curDataMax = this->pCAC()->largeBufferSizeTCP ();
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
                    this->recvThreadRingBufferSpaceAvailableEvent.signal ();
                    return;
                }
            }
            bool msgOK = this->pCAC()->executeResponse ( *this, 
                                this->curMsg, this->pCurData );
            if ( ! msgOK ) {
                this->cleanShutdown ();
                return;
            }
        }
        else {
            static bool once = false;
            if ( ! once ) {
                this->printf (
    "CAC: response with payload size=%u > EPICS_CA_MAX_ARRAY_BYTES ignored\n",
                    this->curMsg.m_postsize );
            }
            this->curDataBytes += this->recvQue.removeBytes ( 
                    this->curMsg.m_postsize - this->curDataBytes );
            if ( this->curDataBytes < this->curMsg.m_postsize  ) {
                this->recvThreadRingBufferSpaceAvailableEvent.signal ();
                return;
            }
        }

        if ( nBytes >= maxBytesPendingTCP && 
            this->recvQue.occupiedBytes () < maxBytesPendingTCP ) {
            this->recvThreadRingBufferSpaceAvailableEvent.signal ();
        }
 
        this->oldMsgHeaderAvailable = false;
        this->msgHeaderAvailable = false;
        this->curDataBytes = 0u;
    }
}

inline void insertRequestHeader (
    comQueSend &sendQue, ca_uint16_t request, ca_uint32_t payloadSize, 
    ca_uint16_t dataType, ca_uint32_t nElem, ca_uint32_t cid, 
    ca_uint32_t requestDependent, bool v49Ok )
{
    if ( payloadSize < 0xffff && nElem < 0xffff ) {
        sendQue.pushUInt16 ( request ); 
        sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( payloadSize ) ); 
        sendQue.pushUInt16 ( dataType ); 
        sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( nElem ) ); 
        sendQue.pushUInt32 ( cid ); 
        sendQue.pushUInt32 ( requestDependent );  
    }
    else if ( v49Ok ) {
        sendQue.pushUInt16 ( request ); 
        sendQue.pushUInt16 ( 0xffff ); 
        sendQue.pushUInt16 ( dataType ); 
        sendQue.pushUInt16 ( 0u ); 
        sendQue.pushUInt32 ( cid ); 
        sendQue.pushUInt32 ( requestDependent );  
        sendQue.pushUInt32 ( payloadSize ); 
        sendQue.pushUInt32 ( nElem ); 
    }
    else {
        throw cacChannel::outOfBounds ();
    }
}

/*
 * tcpiiu::hostNameSetRequest ()
 */
void tcpiiu::hostNameSetRequest ()
{
    if ( ! CA_V41 ( this->minorProtocolVersion ) ) {
        return;
    }

    const char *pName = localHostNameAtLoadTime.pointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( postSize + 16u ) ) {
        this->flushRequest ();
    }

    epicsAutoMutex locker ( this->pCAC()->mutexRef() );

    this->sendQue.pushUInt16 ( CA_PROTO_HOST_NAME ); // cmd
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( postSize ) ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( nillBytes, postSize - size );
}

/*
 * tcpiiu::userNameSetRequest ()
 */
void tcpiiu::userNameSetRequest ()
{
    if ( ! CA_V41 ( this->minorProtocolVersion ) ) {
        return;
    }

    const char *pName = this->pCAC ()->userNamePointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushEarlyThreshold ( postSize + 16u ) ) {
        this->flushRequest ();
    }

    epicsAutoMutex locker (  this->pCAC()->mutexRef()  );
    this->sendQue.pushUInt16 ( CA_PROTO_CLIENT_NAME ); // cmd
    this->sendQue.pushUInt16 ( postSize ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( nillBytes, postSize - size );
}

void tcpiiu::disableFlowControlRequest ()
{
    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ();
    }

    epicsAutoMutex locker (  this->pCAC()->mutexRef() );

    this->sendQue.pushUInt16 ( CA_PROTO_EVENTS_ON ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
}

void tcpiiu::enableFlowControlRequest ()
{
    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ();
    }

    epicsAutoMutex locker ( this->pCAC()->mutexRef()  );

    this->sendQue.pushUInt16 ( CA_PROTO_EVENTS_OFF ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
}

void tcpiiu::noopRequest ()
{
    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ();
    }

    epicsAutoMutex locker (  this->pCAC()->mutexRef() );

    this->sendQue.pushUInt16 ( CA_PROTO_NOOP ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
}

void tcpiiu::echoRequest ()
{
    if ( this->sendQue.flushEarlyThreshold ( 16u ) ) {
        this->flushRequest ();
    }

    epicsAutoMutex locker ( this->pCAC()->mutexRef() );

    this->sendQue.pushUInt16 ( CA_PROTO_ECHO ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
}

inline void insertRequestWithPayLoad (
    comQueSend &sendQue, ca_uint16_t request,  
    unsigned dataType, ca_uint32_t nElem, ca_uint32_t cid, 
    ca_uint32_t requestDependent, const void *pPayload,
    bool v49Ok )
{
    if ( ! sendQue.dbr_type_ok ( dataType ) ) {
        throw cacChannel::badType();
    }
    ca_uint32_t size;
    bool stringOptim;
    if ( dataType == DBR_STRING && nElem == 1 ) {
        const char *pStr = static_cast < const char * >  ( pPayload );
        size = strlen ( pStr ) + 1u;
        if ( size > MAX_STRING_SIZE ) {
            throw cacChannel::outOfBounds();
        }
        stringOptim = true;
    }
    else {
        unsigned maxBytes;
        if ( v49Ok ) {
            maxBytes = 0xffffffff;
        }
        else {
            maxBytes = MAX_TCP;
        }
        unsigned maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
        if ( nElem >= maxElem ) {
            throw cacChannel::outOfBounds();
        }
        size = dbr_size_n ( dataType, nElem );
        stringOptim = false;
    }
    ca_uint32_t payloadSize = CA_MESSAGE_ALIGN ( size );
    insertRequestHeader ( sendQue, request, payloadSize, 
        static_cast <ca_uint16_t> ( dataType ), 
        nElem, cid, requestDependent, v49Ok );
    if ( stringOptim ) {
        sendQue.pushString ( static_cast < const char * > ( pPayload ), size );  
    }
    else {
        sendQue.push_dbr_type ( dataType, pPayload, nElem );  
    }
    sendQue.pushString ( nillBytes, payloadSize - size );
}

void tcpiiu::writeRequest ( nciu &chan, unsigned type, unsigned nElem, const void *pValue )
{
    if ( ! chan.connected () ) {
        throw cacChannel::notConnected();
    }
    insertRequestWithPayLoad ( this->sendQue, CA_PROTO_WRITE,  
        type, nElem, chan.getSID(), chan.getCID(), pValue,
        CA_V49 ( this->minorProtocolVersion ) );
}


void tcpiiu::writeNotifyRequest ( nciu &chan, netWriteNotifyIO &io, unsigned type,  
                                unsigned nElem, const void *pValue )
{
    if ( ! chan.connected () ) {
        throw cacChannel::notConnected();
    }
    if ( ! this->ca_v41_ok () ) {
        throw cacChannel::unsupportedByService();
    }
    insertRequestWithPayLoad ( this->sendQue, CA_PROTO_WRITE_NOTIFY,  
        type, nElem, chan.getSID(), io.getID(), pValue,
        CA_V49 ( this->minorProtocolVersion ) );
}

void tcpiiu::readNotifyRequest ( nciu &chan, netReadNotifyIO &io, 
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
        maxBytes = this->pCAC()->largeBufferSizeTCP ();
    }
    else {
        maxBytes = MAX_TCP;
    }
    unsigned maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    insertRequestHeader ( this->sendQue, 
        CA_PROTO_READ_NOTIFY, 0u, 
        static_cast < ca_uint16_t > ( dataType ), 
        nElem, chan.getSID(), io.getID(), 
        CA_V49 ( this->minorProtocolVersion ) );
}

void tcpiiu::createChannelRequest ( nciu &chan )
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

    this->sendQue.pushUInt16 ( CA_PROTO_CLAIM_CIU ); // cmd
    this->sendQue.pushUInt16 ( postCnt ); // postsize
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
        this->sendQue.pushString ( nillBytes, postCnt - nameLength );
    }
}

void tcpiiu::clearChannelRequest ( nciu &chan )
{
    if ( chan.connected () ) {
        this->sendQue.pushUInt16 ( CA_PROTO_CLEAR_CHANNEL ); // cmd
        this->sendQue.pushUInt16 ( 0u ); // postsize
        this->sendQue.pushUInt16 ( 0u ); // dataType
        this->sendQue.pushUInt16 ( 0u ); // count
        this->sendQue.pushUInt32 ( chan.getSID () ); // cid
        this->sendQue.pushUInt32 ( chan.getCID () ); // available 
    }
}

//
// this routine return void because if this internally fails the best response
// is to try again the next time that we reconnect
//
void tcpiiu::subscriptionRequest ( nciu &chan, netSubscription & subscr )
{
    if ( ! chan.connected() ) {
        return;
    }
    unsigned mask = subscr.getMask();
    if ( mask > 0xffff ) {
        mask &= 0xffff;
        this->pCAC()->printf ( "CAC: subscriptionRequest() truncated unusual event select mask\n" );
    }
    arrayElementCount nElem = subscr.getCount ();
    unsigned maxBytes;
    if ( CA_V49 ( this->minorProtocolVersion ) ) {
        maxBytes = this->pCAC()->largeBufferSizeTCP ();
    }
    else {
        maxBytes = MAX_TCP;
    }
    unsigned dataType = subscr.getType ();
    unsigned maxElem = ( maxBytes - dbr_size[dataType] ) / dbr_value_size[dataType];
    if ( nElem > maxElem ) {
        throw cacChannel::msgBodyCacheTooSmall ();
    }
    insertRequestHeader ( this->sendQue, 
        CA_PROTO_EVENT_ADD, 16u, 
        static_cast < ca_uint16_t > ( dataType ), 
        nElem, chan.getSID(), subscr.getID(), 
        CA_V49 ( this->minorProtocolVersion ) );

    // extension
    this->sendQue.pushFloat32 ( 0.0 ); // m_lval
    this->sendQue.pushFloat32 ( 0.0 ); // m_hval
    this->sendQue.pushFloat32 ( 0.0 ); // m_toval
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( mask ) ); // m_mask
    this->sendQue.pushUInt16 ( 0u ); // m_pad
}

void tcpiiu::subscriptionCancelRequest ( nciu &chan, netSubscription &subscr )
{
    insertRequestHeader ( this->sendQue, 
        CA_PROTO_EVENT_CANCEL, 0u, 
        static_cast < ca_uint16_t > ( subscr.getType () ), 
        static_cast < ca_uint16_t > ( subscr.getCount () ), 
        chan.getSID(), subscr.getID(), 
        CA_V49 ( this->minorProtocolVersion ) );
}

void tcpiiu::lastChannelDetachNotify ()
{
    this->cleanShutdown ();
}

bool tcpiiu::flush ()
{
    while ( true ) {
        comBuf * pBuf;

        {
            epicsAutoMutex autoMutex ( this->pCAC()->mutexRef() );
            pBuf = this->sendQue.popNextComBufToSend ();
            if ( pBuf ) {
                this->unacknowledgedSendBytes += pBuf->occupiedBytes ();
            }
            else {
                if ( this->blockingForFlush ) {
                    this->flushBlockEvent.signal ();
                }
                this->earlyFlush = false;
                return true;
            }
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

        bool success = pBuf->flushToWire ( *this );

        pBuf->destroy ();

        if ( ! success ) {
            epicsAutoMutex autoMutex ( this->pCAC()->mutexRef() );
            while ( ( pBuf = this->sendQue.popNextComBufToSend () ) ) {
                pBuf->destroy ();
            }
            if ( this->blockingForFlush ) {
                this->flushBlockEvent.signal ();
            }
            return false;
        }
    }
}

// ~tcpiiu() will not return while this->blockingForFlush is greater than zero
void tcpiiu::blockUntilSendBacklogIsReasonable ( epicsMutex &mutex )
{
    assert ( this->blockingForFlush < UINT_MAX );
    this->blockingForFlush++;
    while ( this->sendQue.flushBlockThreshold(0u) && this->state == iiu_connected ) {
        epicsAutoMutexRelease autoRelease ( mutex );
        this->flushBlockEvent.wait ( 5.0 );
    }
    if ( this->blockingForFlush == 1 ) {
        this->flushBlockEvent.signal ();
    }
    assert ( this->blockingForFlush > 0u );
    this->blockingForFlush--;
}

void tcpiiu::flushRequestIfAboveEarlyThreshold ()
{
    if ( ! this->earlyFlush && this->sendQue.flushEarlyThreshold(0u) ) {
        this->earlyFlush = true;
        this->sendThreadFlushEvent.signal ();
    }
}

bool tcpiiu::flushBlockThreshold () const
{
    return this->sendQue.flushBlockThreshold ( 0u );
}

double tcpiiu::beaconPeriod () const
{
    return this->BHE.period ();
}

// not inline because its virtual
bool tcpiiu::ca_v42_ok () const
{
    return CA_V42 ( this->minorProtocolVersion );
}




