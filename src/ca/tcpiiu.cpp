

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

#include "iocinf.h"

#include "inetAddrID_IL.h"
#include "bhe_IL.h"
#include "tcpiiu_IL.h"
#include "cac_IL.h"
#include "comBuf_IL.h"
#include "comQueSend_IL.h"
#include "netiiu_IL.h"
#include "nciu_IL.h"
#include "baseNMIU_IL.h"
#include "netWriteNotifyIO_IL.h"
#include "netReadNotifyIO_IL.h"
#include "netSubscription_IL.h"
#include "net_convert.h"

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

    while ( true ) {
        bool flowControlLaborNeeded;
        bool echoLaborNeeded;

        epicsEventMustWait ( piiu->sendThreadFlushSignal );

        if ( piiu->state != iiu_connected ) {
            break;
        }

        {
            epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
            flowControlLaborNeeded = piiu->busyStateDetected != piiu->flowControlActive;
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
            if ( CA_V43 ( CA_PROTOCOL_VERSION, piiu->minorProtocolVersion ) ) {
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

    epicsEventSignal ( piiu->sendThreadExitSignal );
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
                ca_printf ("CAC: unexpected TCP send error: %s\n", SOCKERRSTR (localError) );
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
            ca_printf ( "Disconnecting from CA server %s because: %s\n", 
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
                epicsEventSignal ( piiu->recvThreadExitSignal );
                epicsEventSignal ( piiu->sendThreadExitSignal );
                piiu->cleanShutdown ();
                return;
            }
        }
        else {
            epicsEventSignal ( piiu->recvThreadExitSignal );
            epicsEventSignal ( piiu->sendThreadExitSignal );
            piiu->cleanShutdown ();
            return;
        }
    }

    unsigned nBytes = 0u;
    while ( piiu->state == iiu_connected ) {
        if ( nBytes >= maxBytesPendingTCP ) {
            epicsEventMustWait ( piiu->recvThreadRingBufferSpaceAvailableSignal );
            epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
            nBytes = piiu->recvQue.occupiedBytes ();
        }
        else {
            comBuf * pComBuf = new comBuf;
            if ( pComBuf ) {
                unsigned nBytesIn = pComBuf->fillFromWire ( *piiu );
                if ( nBytesIn ) {
                    bool msgHeaderButNoBody;
                    {
                        epicsAutoMutex autoMutex ( piiu->pCAC()->mutexRef() );
                        nBytes = piiu->recvQue.occupiedBytes ();
                        msgHeaderButNoBody = piiu->msgHeaderAvailable; 
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
                        // reschedule connection activity watchdog
                        piiu->recvDog.messageArrivalNotify (); 
                    }
                    // wake up recv thread only if
                    // 1) there are currently no bytes in the queue
                    // 2) if the recv thread is currently blocking for an incomplete msg
                    if ( nBytes < sizeof ( caHdr ) || msgHeaderButNoBody ) {
                        piiu->pCAC ()->signalRecvActivity ();
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

    epicsEventSignal ( piiu->recvThreadExitSignal );
}

//
// tcpiiu::tcpiiu ()
//
tcpiiu::tcpiiu ( cac &cac, double connectionTimeout, epicsTimerQueue &timerQueue ) :
    netiiu ( &cac ),
    recvDog ( *this, connectionTimeout, timerQueue ),
    sendDog ( *this, connectionTimeout, timerQueue ),
    sendQue ( *this ),
    pHostNameCache ( 0 ),
    curDataMax ( 0ul ),
    pBHE ( 0 ),
    pCurData ( 0 ),
    minorProtocolVersion ( 0u ),
    state ( iiu_connecting ),
    sock ( INVALID_SOCKET ),
    contigRecvMsgCount ( 0u ),
    blockingForFlush ( 0u ),
    busyStateDetected ( false ),
    flowControlActive ( false ),
    echoRequestPending ( false ),
    msgHeaderAvailable ( false ),
    sockCloseCompleted ( false ),
    fdRegCallbackNeeded ( true ),
    earlyFlush ( false )
{
    this->addr.sa.sa_family = AF_UNSPEC;

    this->sendThreadExitSignal = epicsEventCreate ( epicsEventEmpty );
    if ( ! this->sendThreadExitSignal ) {
        ca_printf ("CA: unable to create CA client send thread exit semaphore\n");
        this->fullyConstructedFlag = false;
        return;
    }

    this->recvThreadExitSignal = epicsEventCreate ( epicsEventEmpty );
    if ( ! this->recvThreadExitSignal ) {
        ca_printf ("CA: unable to create CA client send thread exit semaphore\n");
        epicsEventDestroy (this->sendThreadExitSignal);
        this->fullyConstructedFlag = false;
        return;
    }

    this->sendThreadFlushSignal = epicsEventCreate ( epicsEventEmpty );
    if ( ! this->sendThreadFlushSignal ) {
        ca_printf ("CA: unable to create sendThreadFlushSignal object\n");
        epicsEventDestroy (this->sendThreadExitSignal);
        this->fullyConstructedFlag = false;
        return;
    }

    this->flushBlockSignal = epicsEventCreate ( epicsEventEmpty );
    if ( ! this->flushBlockSignal ) {
        ca_printf ("CA: unable to create flushBlockSignal object\n");
        epicsEventDestroy (this->sendThreadExitSignal);
        epicsEventDestroy (this->sendThreadFlushSignal);
        this->fullyConstructedFlag = false;
        return;
    }

    this->recvThreadRingBufferSpaceAvailableSignal = epicsEventCreate ( epicsEventEmpty );
    if ( ! this->recvThreadRingBufferSpaceAvailableSignal ) {
        ca_printf ("CA: unable to create recvThreadRingBufferSpaceAvailableSignal object\n");
        epicsEventDestroy (this->sendThreadExitSignal);
        epicsEventDestroy (this->sendThreadFlushSignal);
        epicsEventDestroy (this->flushBlockSignal);
        this->fullyConstructedFlag = false;
        return;
    }

    this->fullyConstructedFlag = true;
}

/*
 * tcpiiu::initiateConnect ()
 */
bool tcpiiu::initiateConnect ( const osiSockAddr &addrIn, unsigned minorVersion, 
                              class bhe &bhe, ipAddrToAsciiEngine &engineIn )
{
    unsigned priorityOfRecv;
    epicsThreadBooleanStatus tbs;
    epicsThreadId tid;
    int status;
    int flag;

    this->addr = addrIn;

    this->pHostNameCache = new hostNameCache ( addrIn, engineIn );
    if ( ! this->pHostNameCache ) {
        return false;
    }

    this->pBHE = &bhe;
    bhe.bindToIIU ( *this );

    this->state = iiu_connecting;
    this->minorProtocolVersion = minorVersion;

    this->contigRecvMsgCount = 0u;
    this->busyStateDetected = false;
    this->flowControlActive = false;
    this->echoRequestPending = false;
    this->msgHeaderAvailable = false;
    this->sockCloseCompleted = false;

    // first message informs server of user and host name of client
    this->userNameSetRequest ();
    this->hostNameSetRequest ();

    this->sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if ( this->sock == INVALID_SOCKET ) {
        ca_printf ( "CAC: unable to create virtual circuit because \"%s\"\n",
            SOCKERRSTR ( SOCKERRNO ) );
        return false;
    }

    flag = true;
    status = setsockopt ( this->sock, IPPROTO_TCP, TCP_NODELAY,
                (char *) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        ca_printf ("CAC: problems setting socket option TCP_NODELAY = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
    }

    flag = true;
    status = setsockopt ( this->sock , SOL_SOCKET, SO_KEEPALIVE,
                ( char * ) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        ca_printf ( "CAC: problems setting socket option SO_KEEPALIVE = \"%s\"\n",
            SOCKERRSTR ( SOCKERRNO ) );
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
            ca_printf ("CAC: problems setting socket option SO_SNDBUF = \"%s\"\n",
                SOCKERRSTR ( SOCKERRNO ) );
        }
        i = MAX_MSG_SIZE;
        status = setsockopt ( this->sock, SOL_SOCKET, SO_RCVBUF,
                ( char * ) &i, sizeof ( i ) );
        if ( status < 0 ) {
            ca_printf ("CAC: problems setting socket option SO_RCVBUF = \"%s\"\n",
                SOCKERRSTR (SOCKERRNO));
        }
    }
#   endif

    memset ( (void *) &this->curMsg, '\0', sizeof ( this->curMsg ) );

    tbs  = epicsThreadHighestPriorityLevelBelow ( this->pCAC ()->getInitializingThreadsPriority (), &priorityOfRecv );
    if ( tbs != epicsThreadBooleanStatusSuccess ) {
        priorityOfRecv = this->pCAC ()->getInitializingThreadsPriority ();
    }

    tid = epicsThreadCreate ("CAC-TCP-recv", priorityOfRecv,
            epicsThreadGetStackSize (epicsThreadStackMedium), cacRecvThreadTCP, this);
    if ( tid == 0 ) {
        ca_printf ("CA: unable to create CA client receive thread\n");
        socket_close ( this->sock );
        return false;
    }

    return true;
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
            ca_printf ( "Unable to connect because %d=\"%s\"\n", 
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
    this->sendDog.cancel ();
    this->recvDog.cancel ();

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
    epicsEventSignal ( this->sendThreadFlushSignal );
    epicsEventSignal ( this->recvThreadRingBufferSpaceAvailableSignal );
    this->pCAC ()->signalRecvActivity ();
}

/*
 *  tcpiiu::forcedShutdown ()
 */
void tcpiiu::forcedShutdown ()
{
    this->sendDog.cancel ();
    this->recvDog.cancel ();

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

    epicsEventSignal ( this->sendThreadFlushSignal );
    epicsEventSignal ( this->recvThreadRingBufferSpaceAvailableSignal );
    this->pCAC ()->signalRecvActivity ();
}

//
// tcpiiu::~tcpiiu ()
//
tcpiiu::~tcpiiu ()
{
    if ( ! this->fullyConstructedFlag ) {
        return;
    }

    this->cleanShutdown ();

    // wait for send thread to exit
    static const double shutdownDelay = 15.0;
    epicsEventWaitStatus semStat;
    while ( true ) {
        semStat = epicsEventWaitWithTimeout ( this->sendThreadExitSignal, shutdownDelay );
        if ( semStat == epicsEventWaitOK ) {
            break;
        }
        assert ( semStat == epicsEventWaitTimeout );
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
        semStat = epicsEventWaitWithTimeout ( this->recvThreadExitSignal, shutdownDelay );
        if ( semStat == epicsEventWaitOK ) {
            break;
        }
        assert ( semStat == epicsEventWaitTimeout );
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

    /*
     * free message body cache
     */
    {
        epicsAutoMutex autoMutex ( this->pCAC()->mutexRef() );

        if ( this->pCurData ) {
            delete [] this->pCurData;
            this->pCurData = 0;
            this->curDataMax = 0u;
        }

        this->addr.sa.sa_family = AF_UNSPEC;

        this->minorProtocolVersion = 0u;
        if ( this->pHostNameCache ) {
            this->pHostNameCache->destroy ();
            this->pHostNameCache = 0;
        }
        this->pBHE = 0;
        this->sendQue.clear ();
        this->recvQue.clear ();
    }
    this->fdRegCallbackNeeded = true;

    // wakeup user threads blocking for send backlog to be reduced
    // and wait for them to stop using this IIU
    epicsEventSignal ( this->flushBlockSignal );
    while ( this->blockingForFlush ) {
        epicsThreadSleep ( 0.1 );
    }

    epicsEventDestroy ( this->sendThreadExitSignal );
    epicsEventDestroy ( this->recvThreadExitSignal );
    epicsEventDestroy ( this->sendThreadFlushSignal );
    epicsEventDestroy ( this->recvThreadRingBufferSpaceAvailableSignal );
    epicsEventDestroy ( this->flushBlockSignal );

    if ( this->pHostNameCache ) {
        this->pHostNameCache->destroy ();
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
        if ( this->pHostNameCache ) {
            this->pHostNameCache->hostName ( acc, sizeof ( acc ) );
            assert ( this->pCAC () );
            msgForMultiplyDefinedPV *pMsg = new msgForMultiplyDefinedPV ( 
                *this->pCAC (), pChannelName, acc, addrIn );
            if ( pMsg ) {
                this->pCAC ()->ipAddrToAsciiAsynchronousRequestInstall ( *pMsg );
            }
        }
    }

    return true;
}

void tcpiiu::show ( unsigned level ) const
{
    epicsAutoMutex locker ( this->pCAC()->mutexRef() );
    char buf[256];
    if ( this->pHostNameCache ) {
        this->pHostNameCache->hostName ( buf, sizeof ( buf ) );
    }
    else {
        strncpy ( buf, "<disconnected>", sizeof ( buf ) );
        buf [ sizeof ( buf ) - 1 ] = '\0';
    }
    printf ( "Virtual circuit to \"%s\" at version %u.%u state %u\n", 
        buf, CA_PROTOCOL_VERSION, this->minorProtocolVersion,
        this->state );
    if ( level > 1u ) {
        this->netiiu::show ( level - 1u );
    }
    if ( level > 2u ) {
        printf ( "\tcurrent data cache pointer = %p current data cache size = %lu\n",
            static_cast < void * > ( this->pCurData ), this->curDataMax );
        printf ( "\tcontiguous receive message count=%u, busy detect bool=%u, flow control bool=%u\n", 
            this->contigRecvMsgCount, this->busyStateDetected, this->flowControlActive );
    }
    if ( level > 3u ) {
        printf ( "\tvirtual circuit socket identifier %d\n", this->sock );
        printf ( "\tsend thread flush signal:\n" );
        epicsEventShow ( this->sendThreadFlushSignal, level-3u );
        printf ( "\trecv thread buffer space available signal:\n" );
        epicsEventShow ( this->recvThreadRingBufferSpaceAvailableSignal, level-3u );
        printf ( "\tsend thread exit signal:\n" );
        epicsEventShow ( this->sendThreadExitSignal, level-3u );
        printf ( "\trecv thread exit signal:\n" );
        epicsEventShow ( this->recvThreadExitSignal, level-3u );
        printf ( "\tfully constructed bool %u\n", this->fullyConstructedFlag );
        printf ("\techo pending bool = %u\n", this->echoRequestPending );
        printf ("\treceive message header available bool = %u\n", this->msgHeaderAvailable );
        if ( this->pBHE ) {
            this->pBHE->show ( level - 3u );
        }
        ::printf ( "IO identifier hash table:\n" );
    }
}

bool tcpiiu::setEchoRequestPending ()
{
    {
        epicsAutoMutex locker ( this->pCAC()->mutexRef() );
        this->echoRequestPending = true;
    }
    this->flushRequest ();
    if ( CA_V43 (CA_PROTOCOL_VERSION, this->minorProtocolVersion ) ) {
        // we send an echo
        return true;
    }
    else {
        // we send a NOOP
        return false;
    }
}

/*
 * tcpiiu::hostNameSetRequest ()
 */
void tcpiiu::hostNameSetRequest ()
{
    if ( ! CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion ) ) {
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

    this->sendQue.reserveSpace ( postSize + 16u );
    this->sendQue.pushUInt16 ( CA_PROTO_HOST_NAME ); // cmd
    this->sendQue.pushUInt16 ( postSize ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( static_cast <ca_uint32_t> ( 0u ) ); // cid
    this->sendQue.pushUInt32 ( static_cast <ca_uint32_t> ( 0u ) ); // available 

    this->sendQue.pushString ( pName, size );
    this->sendQue.pushString ( nillBytes, postSize - size );
}

/*
 * tcpiiu::userNameSetRequest ()
 */
void tcpiiu::userNameSetRequest ()
{
    if ( ! CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion ) ) {
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

    this->sendQue.reserveSpace ( postSize + 16u );
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

    this->sendQue.reserveSpace ( 16u );
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

    this->sendQue.reserveSpace ( 16u );
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

    this->sendQue.reserveSpace ( 16u );
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

    this->sendQue.reserveSpace ( 16u );
    this->sendQue.pushUInt16 ( CA_PROTO_ECHO ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( 0u ); // cid
    this->sendQue.pushUInt32 ( 0u ); // available 
}

//
// tcpiiu::processIncoming()
//
void tcpiiu::processIncoming ()
{
    while ( 1 ) {
        unsigned nBytes;

        //
        // fetch a complete message header
        //
        nBytes = this->recvQue.occupiedBytes ();

        if ( ! this->msgHeaderAvailable ) {

            this->msgHeaderAvailable = this->recvQue.copyOutBytes ( 
                &this->curMsg, sizeof ( this->curMsg ) );

            if ( ! this->msgHeaderAvailable ) {
                return;
            }

            //
            // fix endian of bytes 
            //
            this->curMsg.m_cmmd = ntohs ( this->curMsg.m_cmmd );
            this->curMsg.m_postsize = ntohs ( this->curMsg.m_postsize );
            this->curMsg.m_dataType = ntohs ( this->curMsg.m_dataType );
            this->curMsg.m_count = ntohs ( this->curMsg.m_count );
            this->curMsg.m_cid = ntohl ( this->curMsg.m_cid );
            this->curMsg.m_available = ntohl ( this->curMsg.m_available );

            debugPrintf (
                ( "%s Cmd=%3d Type=%3d Count=%4d Size=%4d",
                this->pHostName (),
                this->curMsg.m_cmmd,
                this->curMsg.m_dataType,
                this->curMsg.m_count,
                this->curMsg.m_postsize) );

            debugPrintf (
                ( " Avail=%8x Cid=%6d\n",
                this->curMsg.m_available,
                this->curMsg.m_cid) );
        }

        //
        // dont allow huge msg body until
        // the client library supports it
        //
        if ( this->curMsg.m_postsize > ( unsigned ) MAX_TCP ) {
            this->msgHeaderAvailable = false;
            ca_printf ( "CAC: message body was too large ( disconnecting )\n" );
            this->cleanShutdown ();
            return;
        }

        //
        // make sure we have a large enough message body cache
        //
        if ( this->curMsg.m_postsize > this->curDataMax ) {

            /* 
             * scalar DBR_STRING is sometimes clipped to the
             * actual string size so make sure this cache is
             * as large as one DBR_STRING so they will
             * not page fault if they read MAX_STRING_SIZE
             * bytes (instead of the actual string size).
             */
            unsigned cacheSize = this->curMsg.m_postsize * 2u;
            if ( cacheSize < MAX_STRING_SIZE ) {
                cacheSize = MAX_STRING_SIZE;
            }

            char *pData = new char [cacheSize];
            if ( ! pData ) {
                ca_printf ("CAC: not enough memory for message body cache (disconnecting)\n");
                this->cleanShutdown ();
                return;
            }
            if ( this->pCurData ) {
                delete [] this->pCurData;
            }
            this->pCurData = pData;
            this->curDataMax = cacheSize;
        }

        if ( this->curMsg.m_postsize > 0u ) {
            bool msgBodyAvailable = this->recvQue.copyOutBytes ( 
                        this->pCurData, this->curMsg.m_postsize );
            if ( ! msgBodyAvailable ) {
                return;
            }
        }

        if ( nBytes >= maxBytesPendingTCP && 
            this->recvQue.occupiedBytes () < maxBytesPendingTCP ) {
            epicsEventSignal ( this->recvThreadRingBufferSpaceAvailableSignal );
        }

        this->pCAC()->executeResponse ( *this, this->curMsg, this->pCurData );
         
        this->msgHeaderAvailable = false;
    }
}

void tcpiiu::writeRequest ( nciu &chan, unsigned type, unsigned nElem, const void *pValue )
{
    if ( ! chan.connected () ) {
        throw cacChannel::notConnected();
    }

    if ( ! this->sendQue.dbr_type_ok ( type ) ) {
        throw cacChannel::badType();
    }

    if ( nElem > 0xffff) {
        throw cacChannel::outOfBounds();
    }

    bool stringOptim;
    unsigned size;
    if ( type == DBR_STRING && nElem == 1 ) {
        const char *pstr = static_cast < const char * > ( pValue );
        size = strlen ( pstr ) + 1;
        stringOptim = true;
    }
    else {
        size = dbr_size_n ( type, nElem );
        stringOptim = false;
    }

    unsigned postcnt = CA_MESSAGE_ALIGN ( size );
    if ( postcnt > 0xffff ) {
        throw cacChannel::unsupportedByService();
    }

    this->sendQue.reserveSpace ( postcnt + 16u );
    this->sendQue.pushUInt16 ( CA_PROTO_WRITE ); // cmd
    this->sendQue.pushUInt16 ( postcnt ); // postsize
    this->sendQue.pushUInt16 ( type ); // dataType
    this->sendQue.pushUInt16 ( nElem ); // count
    this->sendQue.pushUInt32 ( chan.getSID () ); // cid
    this->sendQue.pushUInt32 ( ~0UL ); // available 
    if ( stringOptim ) {
        this->sendQue.pushString ( static_cast < const char * > ( pValue ), size );  
    }
    else {
        this->sendQue.push_dbr_type ( type, pValue, nElem );  
    }
    this->sendQue.pushString ( nillBytes, postcnt - size );
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

    if ( ! this->sendQue.dbr_type_ok ( type ) ) {
        throw cacChannel::badType();
    }

    if ( nElem > 0xffff ) {
        throw cacChannel::unsupportedByService();
    }

    ca_uint32_t size;
    bool stringOptim;
    if ( type == DBR_STRING && nElem == 1 ) {
        char *pstr = (char *) pValue;
        size = strlen ( pstr ) +1;
        stringOptim = true;
    }
    else {
        size = dbr_size_n ( type, nElem );
        stringOptim = false;
    }
    ca_uint32_t postcnt = CA_MESSAGE_ALIGN ( size );
    if ( postcnt > 0xffff ) {
        throw cacChannel::unsupportedByService();
    }

    this->sendQue.reserveSpace ( postcnt + 16u );
    this->sendQue.pushUInt16 ( CA_PROTO_WRITE_NOTIFY ); // cmd
    this->sendQue.pushUInt16 ( postcnt ); // postsize
    this->sendQue.pushUInt16 ( type ); // dataType
    this->sendQue.pushUInt16 ( nElem ); // count
    this->sendQue.pushUInt32 ( chan.getSID () ); // cid
    this->sendQue.pushUInt32 ( io.getID () ); // available 
    if ( stringOptim ) {
        this->sendQue.pushString ( static_cast < const char * > ( pValue ), size );  
    }
    else {
        this->sendQue.push_dbr_type ( type, pValue, nElem );  
    }
    this->sendQue.pushString ( nillBytes, postcnt - size );
}

void tcpiiu::readNotifyRequest ( nciu &chan, netReadNotifyIO &io, 
                               unsigned type, unsigned nElem )
{
    if ( ! chan.connected () ) {
        throw cacChannel::notConnected();
    }

    if ( nElem > 0xffff) {
        throw cacChannel::unsupportedByService();
     }
    if ( type > 0xffff) {
        throw cacChannel::unsupportedByService();
     }

    this->sendQue.reserveSpace ( 16u );
    this->sendQue.pushUInt16 ( CA_PROTO_READ_NOTIFY ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( type ); // dataType
    this->sendQue.pushUInt16 ( nElem ); // count
    this->sendQue.pushUInt32 ( chan.getSID () ); // cid
    this->sendQue.pushUInt32 ( io.getID () ); // available 
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

    if ( postCnt > 0xffff ) {
        throw cacChannel::unsupportedByService();
    }

    this->sendQue.reserveSpace ( postCnt + 16u );
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
    this->sendQue.pushUInt32 ( CA_MINOR_VERSION ); // available 
    if ( nameLength ) {
        this->sendQue.pushString ( pName, nameLength );
    }
    if ( postCnt > nameLength ) {
        this->sendQue.pushString ( nillBytes, postCnt - nameLength );
    }
}

void tcpiiu::clearChannelRequest ( nciu &chan )
{
    // we go ahead and send this even if the channel isnt connected
    // because we dont want to leak the resource in the server
    this->sendQue.reserveSpace ( 16u );
    this->sendQue.pushUInt16 ( CA_PROTO_CLEAR_CHANNEL ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( 0u ); // dataType
    this->sendQue.pushUInt16 ( 0u ); // count
    this->sendQue.pushUInt32 ( chan.getSID () ); // cid
    this->sendQue.pushUInt32 ( chan.getCID () ); // available 
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

    if ( subscr.getType() > 0xffff ) {
        this->pCAC()->printf ( "CAC: subscriptionRequest() ignored because of unexpected bad type that was checked earlier\n" );
        return;
    }

    if ( subscr.getMask() > 0xffff || subscr.getMask () == 0u ) {
        this->pCAC()->printf ( "CAC: subscriptionRequest() ignored because of unexpected bad mask that was checked earlier\n" );
        return;
    }

    unsigned long count = subscr.getCount ( chan );
    if ( count == 0u || count > 0xffff ) {
        this->pCAC()->printf ( "CAC: subscriptionRequest() ignored because of unexpected bad count that was checked earlier\n" );
        return;
    }

    this->sendQue.reserveSpace ( 32u );

    // header
    this->sendQue.pushUInt16 ( CA_PROTO_EVENT_ADD ); // cmd
    this->sendQue.pushUInt16 ( 16u ); // postsize
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getType () ) ); // dataType
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( count ) ); // count
    this->sendQue.pushUInt32 ( chan.getSID() ); // cid
    this->sendQue.pushUInt32 ( subscr.getID() ); // available 

    // extension
    this->sendQue.pushFloat32 ( 0.0 ); // m_lval
    this->sendQue.pushFloat32 ( 0.0 ); // m_hval
    this->sendQue.pushFloat32 ( 0.0 ); // m_toval
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getMask () ) ); // m_mask
    this->sendQue.pushUInt16 ( 0u ); // m_pad
}

void tcpiiu::subscriptionCancelRequest ( nciu &chan, netSubscription &subscr )
{
    this->sendQue.reserveSpace ( 16u );
    this->sendQue.pushUInt16 ( CA_PROTO_EVENT_CANCEL ); // cmd
    this->sendQue.pushUInt16 ( 0u ); // postsize
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getType () ) ); // dataType
    this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getCount ( chan ) ) ); // count
    this->sendQue.pushUInt32 ( chan.getSID () ); // cid
    this->sendQue.pushUInt32 ( subscr.getID() ); // available
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
            if ( ! pBuf ) {
                if ( this->blockingForFlush ) {
                    epicsEventSignal ( this->flushBlockSignal );
                }
                this->earlyFlush = false;
                return true;
            }
        }

        bool success = pBuf->flushToWire ( *this );

        pBuf->destroy ();

        if ( ! success ) {
            epicsAutoMutex autoMutex ( this->pCAC()->mutexRef() );
            while ( ( pBuf = this->sendQue.popNextComBufToSend () ) ) {
                pBuf->destroy ();
            }
            if ( this->blockingForFlush ) {
                epicsEventSignal ( this->flushBlockSignal );
            }
            return false;
        }
    }
}

void tcpiiu::blockUntilSendBacklogIsReasonable ( epicsMutex &mutex )
{
    assert ( this->blockingForFlush < UINT_MAX );
    this->blockingForFlush++;
    while ( this->sendQue.flushBlockThreshold(0u) && this->state == iiu_connected ) {
        epicsAutoMutexRelease autoMutex ( mutex );
        this->pCAC()->enableCallbackPreemption ();
        epicsEventWaitWithTimeout ( this->flushBlockSignal, 5.0 );
        this->pCAC()->disableCallbackPreemption ();
    }
    assert ( this->blockingForFlush > 0u );
    this->blockingForFlush--;
    if ( this->blockingForFlush ) {
        epicsEventSignal ( this->flushBlockSignal );
    }
}

void tcpiiu::flushRequestIfAboveEarlyThreshold ()
{
    if ( ! this->earlyFlush && this->sendQue.flushEarlyThreshold(0u) ) {
        this->earlyFlush = true;
        epicsEventSignal ( this->sendThreadFlushSignal );
    }
}

bool tcpiiu::flushBlockThreshold () const
{
    return this->sendQue.flushBlockThreshold ( 0u );
}

double tcpiiu::beaconPeriod () const
{
    if ( this->pBHE ) {
        return this->pBHE->period ();
    }
    else {
        return netiiu::beaconPeriod ();
    }
}

// not inline because its virtual
bool tcpiiu::ca_v42_ok () const
{
    return CA_V42 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion );
}




