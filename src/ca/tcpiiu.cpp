

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
#include "netReadCopyIO_IL.h"
#include "netReadNotifyIO_IL.h"
#include "netSubscription_IL.h"
#include "ioCounter_IL.h"

// nill message pad bytes
static const char nillBytes [] = 
{ 
    0, 0, 0, 0,
    0, 0, 0, 0
};

// TCP protocol jump table
const tcpiiu::pProtoStubTCP tcpiiu::tcpJumpTableCAC [] = 
{
    &tcpiiu::noopAction,
    &tcpiiu::eventRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::readRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::exceptionRespAction,
    &tcpiiu::clearChannelRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::readNotifyRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::claimCIURespAction,
    &tcpiiu::writeNotifyRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::accessRightsRespAction,
    &tcpiiu::echoRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::badTCPRespAction,
    &tcpiiu::verifyAndDisconnectChan,
    &tcpiiu::verifyAndDisconnectChan
};

#ifdef DEBUG
#   define debugPrintf(argsInParen) printf argsInParen
#else
#   define debugPrintf(argsInParen)
#endif

#ifdef CONVERSION_REQUIRED 
extern CACVRTFUNC *cac_dbr_cvrt[];
#endif /*CONVERSION_REQUIRED*/

const static char nullBuff[32] = {
    0,0,0,0,0,0,0,0,0,0, 
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0
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
            epicsAutoMutex autoMutex ( piiu->mutex );
            flowControlLaborNeeded = piiu->busyStateDetected != piiu->flowControlActive;
            echoLaborNeeded = piiu->echoRequestPending;
            piiu->echoRequestPending = false;
        }

        if ( flowControlLaborNeeded ) {
            if ( piiu->flowControlActive ) {
                int status = piiu->disableFlowControlRequest ();
                if ( status == ECA_NORMAL ) {
                    piiu->flowControlActive = false;
                }
#               if defined ( DEBUG )
                    printf ( "fc off\n" );
#               endif
            }
            else {
                int status = piiu->enableFlowControlRequest ();
                if ( status == ECA_NORMAL ) {
                    piiu->flowControlActive = true;
                }
#               if defined ( DEBUG )
                    printf ( "fc on\n" );
#               endif
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

        if ( ! piiu->flushToWire ( false ) ) {
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
    unsigned        totalBytes;
    int             status;
    
    if ( this->state != iiu_connected ) {
        return 0u;
    }

    assert ( nBytesInBuf <= INT_MAX );
    status = ::recv ( this->sock, static_cast <char *> ( pBuf ), 
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
            this->hostName ( name, sizeof (name) );
            ca_printf ( "Disconnecting from CA server %s because: %s\n", 
                name, SOCKERRSTR (localErrno) );
        }

        this->cleanShutdown ();

        return 0u;
    }
    
    assert ( static_cast <unsigned> ( status ) <= nBytesInBuf );
    totalBytes = static_cast <unsigned> ( status );

    {
        epicsAutoMutex autoMutex ( this->mutex );
        if ( nBytesInBuf == totalBytes ) {
            if ( this->contigRecvMsgCount >= contiguousMsgCountWhichTriggersFlowControl ) {
                this->busyStateDetected = true;
            }
            else { 
                this->contigRecvMsgCount++;
            }
        }
        else {
            this->contigRecvMsgCount = 0u;
            this->busyStateDetected = false;
        }
    }
    
    this->recvDog.messageArrivalNotify (); // reschedule connection activity watchdog

    return totalBytes;
}

/*
 *  cacRecvThreadTCP ()
 */
extern "C" void cacRecvThreadTCP ( void *pParam )
{
    tcpiiu *piiu = ( tcpiiu * ) pParam;

    piiu->connect ();

    {
        epicsAutoMutex autoMutex ( piiu->mutex );
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

    while ( piiu->state == iiu_connected ) {
        unsigned nBytes;
        {
            epicsAutoMutex autoMutex ( piiu->mutex );
            nBytes = piiu->recvQue.occupiedBytes ();
        }
        if ( nBytes >= 0x4000 ) {
            epicsEventMustWait ( piiu->recvThreadRingBufferSpaceAvailableSignal );
        }
        else {
            comBuf * pComBuf = new comBuf;
            if ( pComBuf ) {
                unsigned nBytesIn = pComBuf->fillFromWire ( *piiu );
                if ( nBytesIn ) {
                    {
                        epicsAutoMutex autoMutex ( piiu->mutex );
                        piiu->recvQue.pushLastComBufReceived ( *pComBuf );
                    }
                    piiu->pCAC ()->signalRecvActivity ();
                }
                else {
                    pComBuf->destroy ();
                }
            }
            else {
                // no way to be informed when memory is available
                epicsThreadSleep ( 0.5 );
            }
        }
    }

    epicsEventSignal ( piiu->recvThreadExitSignal );
}

//
// tcpiiu::tcpiiu ()
//
tcpiiu::tcpiiu ( cac &cac, double connectionTimeout, osiTimerQueue &timerQueue ) :
    netiiu ( &cac ),
    recvDog ( *this, connectionTimeout, timerQueue ),
    sendDog ( *this, connectionTimeout, timerQueue ),
    ioTable ( 1024 ),
    sendQue ( *this ),
    pHostNameCache ( 0 ),
    curDataMax ( 0ul ),
    pBHE ( 0 ),
    pCurData ( 0 ),
    minorProtocolVersion ( 0u ),
    state ( iiu_connecting ),
    sock ( INVALID_SOCKET ),
    contigRecvMsgCount ( 0u ),
    busyStateDetected ( false ),
    flowControlActive ( false ),
    echoRequestPending ( false ),
    msgHeaderAvailable ( false ),
    sockCloseCompleted ( false )
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

    this->recvThreadRingBufferSpaceAvailableSignal = epicsEventCreate ( epicsEventEmpty );
    if ( ! this->recvThreadRingBufferSpaceAvailableSignal ) {
        ca_printf ("CA: unable to create recvThreadRingBufferSpaceAvailableSignal object\n");
        epicsEventDestroy (this->sendThreadExitSignal);
        epicsEventDestroy (this->sendThreadFlushSignal);
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

    epicsAutoMutex autoMutex ( this->mutex );

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

    flag = TRUE;
    status = setsockopt ( this->sock, IPPROTO_TCP, TCP_NODELAY,
                (char *) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        ca_printf ("CAC: problems setting socket option TCP_NODELAY = \"%s\"\n",
            SOCKERRSTR (SOCKERRNO));
    }

    flag = TRUE;
    status = setsockopt ( this->sock , SOL_SOCKET, SO_KEEPALIVE,
                ( char * ) &flag, sizeof ( flag ) );
    if ( status < 0 ) {
        ca_printf ( "CAC: problems setting socket option SO_KEEPALIVE = \"%s\"\n",
            SOCKERRSTR ( SOCKERRNO ) );
    }

#if 0
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
#endif

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

    CAFDHANDLER *fdRegFunc;
    void *fdRegArg;
    this->pCAC ()->getFDRegCallback ( fdRegFunc, fdRegArg );
    if ( fdRegFunc ) {
        ( *fdRegFunc ) ( fdRegArg, this->sock, TRUE );
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

            epicsAutoMutex autoMutex ( this->mutex );

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

    epicsAutoMutex autoMutex ( this->mutex );

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

    epicsAutoMutex autoMutex ( this->mutex );

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

void tcpiiu::disconnect ()
{
    assert ( this->fullyConstructedFlag );

    // if we get here and the IO is still attached then we have an
    // io block that was not registered with a channel.
    if ( this->ioTable.numEntriesInstalled () ) {
        this->pCAC ()->printf ( "CA connection disconnect with %u IO items still installed?\n",
            this->ioTable.numEntriesInstalled () );
    }

    CAFDHANDLER *fdRegFunc;
    void *fdRegArg;
    this->pCAC ()->getFDRegCallback ( fdRegFunc, fdRegArg );
    if ( fdRegFunc ) {
        ( *fdRegFunc ) ( fdRegArg, this->sock, FALSE );
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
        epicsAutoMutex autoMutex ( this->mutex );

        if ( this->pCurData ) {
            free ( this->pCurData );
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
}


//
// tcpiiu::~tcpiiu ()
//
tcpiiu::~tcpiiu ()
{
    if ( ! this->fullyConstructedFlag ) {
        return;
    }

    epicsEventDestroy ( this->sendThreadExitSignal );
    epicsEventDestroy ( this->recvThreadExitSignal );
    epicsEventDestroy ( this->sendThreadFlushSignal );
    epicsEventDestroy ( this->recvThreadRingBufferSpaceAvailableSignal );

    if ( this->pHostNameCache ) {
        this->pHostNameCache->destroy ();
    }

    // this->pBHE lifetime management is handled by the class that creates this object
}

void tcpiiu::suicide ()
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
        epicsAutoMutex autoMutex ( this->mutex );
        char acc[64];
        if ( this->pHostNameCache ) {
            this->pHostNameCache->hostName ( acc, sizeof ( acc ) );
            assert ( this->pCAC () );
            msgForMultiplyDefinedPV *pMsg = new msgForMultiplyDefinedPV ( 
                *this->pCAC (), pChannelName, acc, addr );
            if ( pMsg ) {
                this->pCAC ()->ipAddrToAsciiAsynchronousRequestInstall ( *pMsg );
            }
        }
    }

    return true;
}

void tcpiiu::show ( unsigned level ) const
{
    epicsAutoMutex autoMuext ( this->mutex );
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
            this->pCurData, this->curDataMax );
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
        this->ioTable.show ( level - 3u );
    }
}

bool tcpiiu::setEchoRequestPending ()
{
    {
        epicsAutoMutex autoMutex ( this->mutex );
        this->echoRequestPending = true;
    }
    this->flush ();
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
int tcpiiu::hostNameSetRequest ()
{
    if ( ! CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion ) ) {
        return ECA_NORMAL;
    }

    const char *pName = localHostNameAtLoadTime.pointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushThreshold ( postSize + 16u ) ) {
        this->flush ();
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( postSize + 16u );
    if ( status == ECA_NORMAL ) {
        this->sendQue.pushUInt16 ( CA_PROTO_HOST_NAME ); // cmd
        this->sendQue.pushUInt16 ( postSize ); // postsize
        this->sendQue.pushUInt16 ( 0u ); // dataType
        this->sendQue.pushUInt16 ( 0u ); // count
        this->sendQue.pushUInt32 ( static_cast <ca_uint32_t> ( 0u ) ); // cid
        this->sendQue.pushUInt32 ( static_cast <ca_uint32_t> ( 0u ) ); // available 

        this->sendQue.pushString ( pName, size );
        this->sendQue.pushString ( nillBytes, postSize - size );
    }

    return status;
}

/*
 * tcpiiu::userNameSetRequest ()
 */
int tcpiiu::userNameSetRequest ()
{
    if ( ! CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion ) ) {
        return ECA_NORMAL;
    }

    const char *pName = this->pCAC ()->userNamePointer ();
    unsigned size = strlen ( pName ) + 1u;
    unsigned postSize = CA_MESSAGE_ALIGN ( size );
    assert ( postSize < 0xffff );

    if ( this->sendQue.flushThreshold ( postSize + 16u ) ) {
        this->flush ();
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( postSize + 16u );
    if ( status == ECA_NORMAL ) {
        this->sendQue.pushUInt16 ( CA_PROTO_CLIENT_NAME ); // cmd
        this->sendQue.pushUInt16 ( postSize ); // postsize
        this->sendQue.pushUInt16 ( 0u ); // dataType
        this->sendQue.pushUInt16 ( 0u ); // count
        this->sendQue.pushUInt32 ( 0u ); // cid
        this->sendQue.pushUInt32 ( 0u ); // available 

        this->sendQue.pushString ( pName, size );
        this->sendQue.pushString ( nillBytes, postSize - size );
    }

    return status;
}

int tcpiiu::disableFlowControlRequest ()
{
    if ( this->sendQue.flushThreshold ( 16u ) ) {
        this->flush ();
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( 16u );
    if ( status == ECA_NORMAL ) {
        this->sendQue.pushUInt16 ( CA_PROTO_EVENTS_ON ); // cmd
        this->sendQue.pushUInt16 ( 0u ); // postsize
        this->sendQue.pushUInt16 ( 0u ); // dataType
        this->sendQue.pushUInt16 ( 0u ); // count
        this->sendQue.pushUInt32 ( 0u ); // cid
        this->sendQue.pushUInt32 ( 0u ); // available 
    }

    return status;
}

int tcpiiu::enableFlowControlRequest ()
{
    if ( this->sendQue.flushThreshold ( 16u ) ) {
        this->flush ();
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( 16u );
    if ( status == ECA_NORMAL ) {
        this->sendQue.pushUInt16 ( CA_PROTO_EVENTS_OFF ); // cmd
        this->sendQue.pushUInt16 ( 0u ); // postsize
        this->sendQue.pushUInt16 ( 0u ); // dataType
        this->sendQue.pushUInt16 ( 0u ); // count
        this->sendQue.pushUInt32 ( 0u ); // cid
        this->sendQue.pushUInt32 ( 0u ); // available 
    }

    return status;
}

int tcpiiu::noopRequest ()
{
    if ( this->sendQue.flushThreshold ( 16u ) ) {
        this->flush ();
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( 16u );
    if ( status == ECA_NORMAL ) {
        this->sendQue.pushUInt16 ( CA_PROTO_NOOP ); // cmd
        this->sendQue.pushUInt16 ( 0u ); // postsize
        this->sendQue.pushUInt16 ( 0u ); // dataType
        this->sendQue.pushUInt16 ( 0u ); // count
        this->sendQue.pushUInt32 ( 0u ); // cid
        this->sendQue.pushUInt32 ( 0u ); // available 
    }

    return status;
}

int tcpiiu::echoRequest ()
{
    if ( this->sendQue.flushThreshold ( 16u ) ) {
        this->flush ();
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( 16u );
    if ( status == ECA_NORMAL ) {
        this->sendQue.pushUInt16 ( CA_PROTO_ECHO ); // cmd
        this->sendQue.pushUInt16 ( 0u ); // postsize
        this->sendQue.pushUInt16 ( 0u ); // dataType
        this->sendQue.pushUInt16 ( 0u ); // count
        this->sendQue.pushUInt32 ( 0u ); // cid
        this->sendQue.pushUInt32 ( 0u ); // available 
    }

    return status;
}

void tcpiiu::noopAction ()
{
    return;
}
 
void tcpiiu::echoRespAction ()
{
    return;
}

void tcpiiu::writeNotifyRespAction ()
{
    int status = this->curMsg.m_cid;
    if ( status == ECA_NORMAL ) {
        this->ioCompletionNotifyAndDestroy ( this->curMsg.m_available );
    }
    else {
        this->ioExceptionNotifyAndDestroy ( this->curMsg.m_available, 
                    status, "write notify request rejected" );
    }
}

void tcpiiu::readNotifyRespAction ()
{
    int v41;
    int status;

    /*
     * convert the data buffer from net
     * format to host format
     */
#   ifdef CONVERSION_REQUIRED 
        if ( this->curMsg.m_dataType < NELEMENTS ( cac_dbr_cvrt ) ) {
            ( *cac_dbr_cvrt[ this->curMsg.m_dataType ] ) (
                 this->pCurData, this->pCurData, FALSE, this->curMsg.m_count);
        }
        else {
            this->curMsg.m_cid = htonl ( ECA_BADTYPE );
        }
#   endif

    /*
     * the channel id field is abused for
     * read notify status starting
     * with CA V4.1
     */
    v41 = CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion );
    if (v41) {
        status = this->curMsg.m_cid;
    }
    else{
        status = ECA_NORMAL;
    }

    if ( status == ECA_NORMAL ) {
        this->ioCompletionNotifyAndDestroy ( this->curMsg.m_available,
            this->curMsg.m_dataType, this->curMsg.m_count, this->pCurData );
    }
    else {
        this->ioExceptionNotifyAndDestroy ( this->curMsg.m_available,
            status, "read failed", this->curMsg.m_dataType, this->curMsg.m_count );
    }
}

void tcpiiu::eventRespAction ()
{
    int v41;
    int status;

    /*
     * m_postsize = 0 used to be a confirmation, but is
     * now a noop because the above hash lookup will 
     * not find a matching IO block
     */
    if ( ! this->curMsg.m_postsize ) {
        return;
    }

    /*
     * convert the data buffer from net
     * format to host format
     */
#   ifdef CONVERSION_REQUIRED 
        if ( this->curMsg.m_dataType < NELEMENTS ( cac_dbr_cvrt ) ) {
            ( *cac_dbr_cvrt [ this->curMsg.m_dataType ] )(
                 this->pCurData, this->pCurData, FALSE,
                 this->curMsg.m_count);
        }
        else {
            this->curMsg.m_cid = htonl ( ECA_BADTYPE );
        }
#   endif

    /*
     * the channel id field is abused for
     * read notify status starting
     * with CA V4.1
     */
    v41 = CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersion );
    if (v41) {
        status = this->curMsg.m_cid;
    }
    else {
        status = ECA_NORMAL;
    }
    if ( status == ECA_NORMAL ) {
        this->ioCompletionNotify ( this->curMsg.m_available,
            this->curMsg.m_dataType, this->curMsg.m_count, this->pCurData );
    }
    else {
        this->ioExceptionNotify ( this->curMsg.m_available,
                status, "subscription update failed", 
                this->curMsg.m_dataType, this->curMsg.m_count );
    }
}

void tcpiiu::readRespAction ()
{
    this->ioCompletionNotifyAndDestroy ( this->curMsg.m_available,
        this->curMsg.m_dataType, this->curMsg.m_count, this->pCurData );
}

void tcpiiu::clearChannelRespAction ()
{
    // currently a noop
}

void tcpiiu::exceptionRespAction ()
{
    char context[255];
    char hostName[64];
    caHdr *req = (caHdr *) this->pCurData;

    {
        epicsAutoMutex autoMutex ( this->mutex );

        if ( this->pHostNameCache ) {
            this->pHostNameCache->hostName ( hostName, sizeof ( hostName ) );
            if ( this->curMsg.m_postsize > sizeof (caHdr) ) {
                sprintf ( context, "detected by: %s for: %s", 
                    hostName, (char *)(req+1) );
            }
            else{
                sprintf ( context, "for: %s", (char *) ( req + 1 ) );
            }
        }
        else {
            sprintf ( context, "for: %s", (char *) ( req + 1 ) );
        }
    }

    switch ( ntohs ( req->m_cmmd ) ) {
    case CA_PROTO_READ_NOTIFY:
        this->ioExceptionNotifyAndDestroy ( ntohl (req->m_available), 
            ntohl (this->curMsg.m_available), context, 
            ntohs (req->m_dataType), ntohs (req->m_count) );
        break;
    case CA_PROTO_READ:
        this->ioExceptionNotifyAndDestroy ( ntohl (req->m_available), 
            ntohl (this->curMsg.m_available), context, 
            ntohs (req->m_dataType), ntohs (req->m_count) );
        break;
    case CA_PROTO_WRITE_NOTIFY:
        this->ioExceptionNotifyAndDestroy ( ntohl (req->m_available), 
            ntohl (this->curMsg.m_available), context, 
            ntohs (req->m_dataType), ntohs (req->m_count) );
        break;
    case CA_PROTO_WRITE:
        this->pCAC ()->exceptionNotify ( ntohl ( this->curMsg.m_available), 
                context, ntohs (req->m_dataType), ntohs (req->m_count), __FILE__, __LINE__);
        break;
    case CA_PROTO_EVENT_ADD:
        this->ioExceptionNotify ( ntohl (req->m_available), 
            ntohl (this->curMsg.m_available), context, 
            ntohs (req->m_dataType), ntohs (req->m_count) );
        break;
    case CA_PROTO_EVENT_CANCEL:
        this->ioExceptionNotifyAndDestroy ( ntohl (req->m_available), 
            ntohl (this->curMsg.m_available), context );
        break;
    default:
        this->pCAC ()->exceptionNotify (ntohl (this->curMsg.m_available), 
            context, __FILE__, __LINE__);
        break;
    }
}

void tcpiiu::accessRightsRespAction ()
{
    static caar init;
    caar arBitField = init; // shut up bounds checker
    unsigned ar;

    ar = this->curMsg.m_available;
    arBitField.read_access = ( ar & CA_PROTO_ACCESS_RIGHT_READ ) ? 1 : 0;
    arBitField.write_access = ( ar & CA_PROTO_ACCESS_RIGHT_WRITE ) ? 1 : 0;

    this->pCAC ()->accessRightsNotify ( this->curMsg.m_cid, arBitField );
}

void tcpiiu::claimCIURespAction ()
{
    this->pCAC ()->connectChannel ( this->ca_v44_ok (), this->curMsg.m_cid, 
        this->curMsg.m_dataType, this->curMsg.m_count, this->curMsg.m_available );
}

void tcpiiu::verifyAndDisconnectChan ()
{
    this->pCAC ()->disconnectChannel ( this->curMsg.m_cid );
}

void tcpiiu::badTCPRespAction ()
{
    char hostName[64];
    bool hostNameInit;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        if ( this->pHostNameCache ) {
            this->pHostNameCache->hostName ( hostName, sizeof ( hostName ) );
            hostNameInit = true;
        }
        else {
            hostNameInit = false;
        }
    }

    if ( hostNameInit ) {
        ca_printf ( "CAC: Bad response code in TCP message from %s was %u\n", 
            hostName, this->curMsg.m_cmmd);
    }
    else {
        ca_printf ( "CAC: Bad response code in TCP message was %u\n", 
            this->curMsg.m_cmmd);
    }
}

/*
 * tcpiiu::processIncoming ()
 */
void tcpiiu::processIncoming ()
{
    while ( 1 ) {

        //
        // fetch a complete message header
        //
        {
            epicsAutoMutex autoMutex ( this->mutex );

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
                void *pData;
                size_t cacheSize;

                /* 
                 * scalar DBR_STRING is sometimes clipped to the
                 * actual string size so make sure this cache is
                 * as large as one DBR_STRING so they will
                 * not page fault if they read MAX_STRING_SIZE
                 * bytes (instead of the actual string size).
                 */
                cacheSize = max ( this->curMsg.m_postsize * 2u, MAX_STRING_SIZE );
                pData = (void *) calloc (1u, cacheSize);
                if ( ! pData ) {
                    ca_printf ("CAC: not enough memory for message body cache (disconnecting)\n");
                    this->cleanShutdown ();
                    return;
                }
                if ( this->pCurData ) {
                    free ( this->pCurData );
                }
                this->pCurData = pData;
                this->curDataMax = this->curMsg.m_postsize;
            }

            if ( this->curMsg.m_postsize > 0u ) {
                bool msgBodyAvailable = this->recvQue.copyOutBytes ( 
                            this->pCurData, this->curMsg.m_postsize );
                if ( ! msgBodyAvailable ) {
                    return;
                }
            }

            epicsEventSignal ( this->recvThreadRingBufferSpaceAvailableSignal );
        }

        /*
         * execute the response message
         */
        pProtoStubTCP pStub;
        if ( this->curMsg.m_cmmd >= NELEMENTS ( tcpJumpTableCAC ) ) {
            pStub = &tcpiiu::badTCPRespAction;
        }
        else {
            pStub = tcpJumpTableCAC [this->curMsg.m_cmmd];
        }
        ( this->*pStub ) ();
         
        this->msgHeaderAvailable = false;
    }
}

inline int tcpiiu::requestStubStatus ()
{
    if ( this->state == iiu_connected ) {
        return ECA_NORMAL;
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int tcpiiu::writeRequest ( nciu &chan, unsigned type, unsigned nElem, const void *pValue )
{
    bufferReservoir reservoir;
    unsigned size, postcnt;
    bool stringOptim;

    if ( ! this->sendQue.dbr_type_ok ( type ) ) {
        return ECA_BADTYPE;
    }

    if ( nElem > 0xffff) {
        return ECA_BADCOUNT;
    }

    if ( type == DBR_STRING && nElem == 1 ) {
        char *pstr = (char *) pValue;
        size = strlen ( pstr ) +1;
        stringOptim = true;
    }
    else {
        size = dbr_size_n ( type, nElem );
        stringOptim = false;
    }

    postcnt = CA_MESSAGE_ALIGN ( size );
    if ( postcnt > 0xffff ) {
        return ECA_BADCOUNT;
    }

    if ( this->sendQue.flushThreshold ( postcnt + 16u ) ) {
        this->threadContextSensitiveFlushToWire ( true );
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( postcnt + 16u );
    if ( status == ECA_NORMAL ) {
        if ( ! chan.verifyConnected ( *this ) ) {
            status = ECA_DISCONNCHID;
        }
        else {
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
    }

    return status;
}

int tcpiiu::writeNotifyRequest ( nciu &chan, cacNotify &notify, unsigned type,  
                                unsigned nElem, const void *pValue )
{
    ca_uint32_t size, postcnt;
    bool stringOptim;

    if ( ! this->ca_v41_ok () ) {
        return ECA_NOSUPPORT;
    }

    if ( ! this->sendQue.dbr_type_ok ( type ) ) {
        return ECA_BADTYPE;
    }

    if ( nElem > 0xffff ) {
        return ECA_BADCOUNT;
    }

    if ( type == DBR_STRING && nElem == 1 ) {
        char *pstr = (char *) pValue;
        size = strlen ( pstr ) +1;
        stringOptim = true;
    }
    else {
        size = dbr_size_n ( type, nElem );
        stringOptim = false;
    }
    postcnt = CA_MESSAGE_ALIGN ( size );
    if ( postcnt > 0xffff ) {
        return ECA_BADCOUNT;
    }

    if ( this->sendQue.flushThreshold ( postcnt + 16u ) ) {
        this->threadContextSensitiveFlushToWire ( true );
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( postcnt + 16u );
    if ( status == ECA_NORMAL ) {
        if ( ! chan.verifyConnected ( *this ) ) {
            status = ECA_DISCONNCHID;
        }
        else {
            netWriteNotifyIO * pIO = new netWriteNotifyIO ( chan, notify );
            if ( ! pIO ) {
                status = ECA_ALLOCMEM;
            }
            else {
                this->ioTable.add ( *pIO );
                chan.tcpiiuPrivateListOfIO::eventq.add ( *pIO );
                this->sendQue.pushUInt16 ( CA_PROTO_WRITE_NOTIFY ); // cmd
                this->sendQue.pushUInt16 ( postcnt ); // postsize
                this->sendQue.pushUInt16 ( type ); // dataType
                this->sendQue.pushUInt16 ( nElem ); // count
                this->sendQue.pushUInt32 ( chan.getSID () ); // cid
                this->sendQue.pushUInt32 ( pIO->getID () ); // available 
                if ( stringOptim ) {
                    this->sendQue.pushString ( static_cast < const char * > ( pValue ), size );  
                }
                else {
                    this->sendQue.push_dbr_type ( type, pValue, nElem );  
                }
                this->sendQue.pushString ( nillBytes, postcnt - size );
            }
        }
    }

    return status;
}

int tcpiiu::readCopyRequest ( nciu &chan, unsigned type, unsigned nElem, void *pValue )
{
    if ( nElem > 0xffff) {
        return ECA_BADCOUNT;
    }
    if ( type > 0xffff) {
        return ECA_BADTYPE;
    }

    if ( this->sendQue.flushThreshold ( 16u ) ) {
        this->threadContextSensitiveFlushToWire ( true );
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( 16u );
    if ( status == ECA_NORMAL ) {
        if ( ! chan.verifyConnected ( *this ) ) {
            status = ECA_DISCONNCHID;
        }
        else {
            unsigned seqNo = this->pCAC ()->readSequenceOfOutstandingIO ();
            netReadCopyIO *pIO = new netReadCopyIO ( chan, type, 
                nElem, pValue, seqNo );
            if ( ! pIO ) {
                status = ECA_ALLOCMEM;
            }
            else {
                this->ioTable.add ( *pIO );
                chan.tcpiiuPrivateListOfIO::eventq.add ( *pIO );
                this->sendQue.pushUInt16 ( CA_PROTO_READ ); // cmd
                this->sendQue.pushUInt16 ( 0u ); // postsize
                this->sendQue.pushUInt16 ( type ); // dataType
                this->sendQue.pushUInt16 ( nElem ); // count
                this->sendQue.pushUInt32 ( chan.getSID () ); // cid
                this->sendQue.pushUInt32 ( pIO->getID () ); // available 
            }
        }
    }

    return status;
}

int tcpiiu::readNotifyRequest ( nciu &chan, cacNotify &notify, 
                               unsigned type, unsigned nElem )
{
    if ( nElem > 0xffff) {
        return ECA_BADCOUNT;
    }
    if ( type > 0xffff) {
        return ECA_BADTYPE;
    }

    if ( this->sendQue.flushThreshold ( 16u ) ) {
        this->threadContextSensitiveFlushToWire ( true );
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( 16u );
    if ( status == ECA_NORMAL ) {
        if ( ! chan.verifyConnected ( *this ) ) {
            status = ECA_DISCONNCHID;
        }
        else {
            netReadNotifyIO *pIO = new netReadNotifyIO ( chan, notify );
            if ( ! pIO ) {
                status = ECA_ALLOCMEM;
            }
            else {
                this->ioTable.add ( *pIO );
                chan.tcpiiuPrivateListOfIO::eventq.add ( *pIO );
                this->sendQue.pushUInt16 ( CA_PROTO_READ_NOTIFY ); // cmd
                this->sendQue.pushUInt16 ( 0u ); // postsize
                this->sendQue.pushUInt16 ( type ); // dataType
                this->sendQue.pushUInt16 ( nElem ); // count
                this->sendQue.pushUInt32 ( chan.getSID () ); // cid
                this->sendQue.pushUInt32 ( pIO->getID () ); // available 
            }
        }
    }

    return status;
}

int tcpiiu::createChannelRequest ( nciu &chan )
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
        return ECA_INTERNAL;
    }

    if ( this->sendQue.flushThreshold ( postCnt + 16u ) ) {
        this->flush ();
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( postCnt + 16u );
    if ( status == ECA_NORMAL ) {
        if ( ! chan.verifyIIU ( *this ) ) {
            status = ECA_DISCONNCHID;
        }
        else {
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
    }

    return status;
}

int tcpiiu::clearChannelRequest ( nciu &chan )
{
    int status;

    if ( this->sendQue.flushThreshold ( 16u ) ) {
        this->threadContextSensitiveFlushToWire ( true );
    }

    epicsAutoMutex autoMutex ( this->mutex );

    if ( ! chan.verifyConnected ( *this ) ) {
        status = ECA_DISCONNCHID;
    }
    else {
        status = this->sendQue.reserveSpace ( 16u );
        if ( status == ECA_NORMAL ) {
            this->sendQue.pushUInt16 ( CA_PROTO_CLEAR_CHANNEL ); // cmd
            this->sendQue.pushUInt16 ( 0u ); // postsize
            this->sendQue.pushUInt16 ( 0u ); // dataType
            this->sendQue.pushUInt16 ( 0u ); // count
            this->sendQue.pushUInt32 ( chan.getSID () ); // cid
            this->sendQue.pushUInt32 ( chan.getCID () ); // available 
        }
    }

    return status;
}

int tcpiiu::subscriptionRequest ( netSubscription &subscr, bool userThread )
{
    if ( subscr.getCount () > 0xffff ) {
        return ECA_BADCOUNT;
    }

    if ( subscr.getType () > 0xffff ) {
        return ECA_BADTYPE;
    }

    if ( subscr.getMask () > 0xffff ) {
        return ECA_BADMASK;
    }

    if ( this->sendQue.flushThreshold ( 32u ) ) {
        if ( userThread ) {
            this->threadContextSensitiveFlushToWire ( true );
        }
        else {
            this->flush ();
        }
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( 32u );
    if ( status == ECA_NORMAL ) {
        if ( ! subscr.channel ().verifyConnected ( *this ) ) {
            status = ECA_NORMAL;
        }
        else {
            this->ioTable.add ( subscr );

            // header
            this->sendQue.pushUInt16 ( CA_PROTO_EVENT_ADD ); // cmd
            this->sendQue.pushUInt16 ( 16u ); // postsize
            this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getType () ) ); // dataType
            this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getCount () ) ); // count
            this->sendQue.pushUInt32 ( subscr.channel ().getSID () ); // cid
            this->sendQue.pushUInt32 ( subscr.getID () ); // available 

            // extension
            this->sendQue.pushFloat32 ( 0.0 ); // m_lval
            this->sendQue.pushFloat32 ( 0.0 ); // m_hval
            this->sendQue.pushFloat32 ( 0.0 ); // m_toval
            this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getMask () ) ); // m_mask
            this->sendQue.pushUInt16 ( 0u ); // m_pad
        }
    }

    return status;
}

void tcpiiu::subscriptionCancelRequest ( netSubscription &subscr, bool userThread )
{
    if ( this->sendQue.flushThreshold ( 16u ) ) {
        if ( userThread ) {
            this->threadContextSensitiveFlushToWire ( true );
        }
        else {
            this->flush ();
        }
    }

    epicsAutoMutex autoMutex ( this->mutex );

    int status = this->sendQue.reserveSpace ( 16u );
    if ( status == ECA_NORMAL ) {
        if ( subscr.channel ().verifyConnected ( *this ) ) {
            this->sendQue.pushUInt16 ( CA_PROTO_EVENT_CANCEL ); // cmd
            this->sendQue.pushUInt16 ( 0u ); // postsize
            this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getType () ) ); // dataType
            this->sendQue.pushUInt16 ( static_cast < ca_uint16_t > ( subscr.getCount () ) ); // count
            this->sendQue.pushUInt32 ( subscr.channel ().getSID () ); // cid
            this->sendQue.pushUInt32 ( subscr.getID () ); // available
        }
    }
}

void tcpiiu::lastChannelDetachNotify ()
{
    this->cleanShutdown ();
}

bool tcpiiu::threadContextSensitiveFlushToWire ( bool userThread )
{
    // the recv thread is not permitted to flush as this
    // can result in a push / pull deadlock on the TCP pipe,
    // but in that case we still schedual the flush through 
    // the higher priority send thread
    if ( ! pCAC ()->flushPermit () ) {
        this->flush ();
        return true;
    }
    return this->flushToWire ( userThread );
}

bool tcpiiu::flushToWire ( bool userThread )
{
    bool success = true;

    // enable callback processing prior to taking the flush lock
    if ( userThread ) {
        this->pCAC ()->enableCallbackPreemption ();
    }

    // only one thread at a time can perform a flush. Nevertheless,
    // the primary lock must not be held while sending in order
    // to prevent push pull deadlocks
    epicsAutoMutex autoFlushMutex ( this->flushMutex );

    while ( true ) {
        comBuf * pBuf;

        {
            epicsAutoMutex autoMutex ( this->mutex );
            pBuf = this->sendQue.popNextComBufToSend ();
        }

        if ( ! pBuf ) {
            break;
        }

        success = pBuf->flushToWire ( *this );

        pBuf->destroy ();

        if ( ! success ) {
            epicsAutoMutex autoMutex ( this->mutex );
            while ( ( pBuf = this->sendQue.popNextComBufToSend () ) ) {
                pBuf->destroy ();
            }
            break; 
        }
    }

    if ( userThread ) {
        this->pCAC ()->disableCallbackPreemption ();
    }

    return success;
}

void tcpiiu::ioCompletionNotify ( unsigned id, unsigned type, 
                              unsigned long count, const void *pData )
{
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( pmiu ) {
        pmiu->completionNotify ( type, count, pData );
    }
}

void tcpiiu::ioExceptionNotify ( unsigned id, int status, const char *pContext )
{
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( pmiu ) {
        pmiu->exceptionNotify ( status, pContext );
    }
}

void tcpiiu::ioExceptionNotify ( unsigned id, int status, 
                   const char *pContext, unsigned type, unsigned long count )
{
    epicsAutoMutex autoMutex ( this->mutex );
    baseNMIU * pmiu = this->ioTable.lookup ( id );
    if ( pmiu ) {
        pmiu->exceptionNotify ( status, pContext, type, count );
    }
}

void tcpiiu::ioCompletionNotifyAndDestroy ( unsigned id )
{
    baseNMIU * pmiu;

    {
        epicsAutoMutex autoMutex ( this->mutex );
        pmiu = this->ioTable.remove ( id );
        if ( pmiu ) {
            pmiu->channel ().tcpiiuPrivateListOfIO::eventq.remove ( *pmiu );
        }
    }

    if ( pmiu ) {
        pmiu->completionNotify ();
        delete pmiu; 
    }
}

void tcpiiu::ioCompletionNotifyAndDestroy ( unsigned id, 
                        unsigned type, unsigned long count, const void *pData )
{
    baseNMIU * pmiu;

    {
        epicsAutoMutex autoMutex ( this->mutex );
        pmiu = this->ioTable.remove ( id );
        if ( pmiu ) {
            pmiu->channel ().tcpiiuPrivateListOfIO::eventq.remove ( *pmiu );
        }
    }

    if ( pmiu ) {
        pmiu->completionNotify ( type, count, pData );
        delete pmiu;
    }
}

void tcpiiu::ioExceptionNotifyAndDestroy ( unsigned id, int status, const char *pContext )
{
    baseNMIU * pmiu;

    {
        epicsAutoMutex autoMutex ( this->mutex );
        pmiu = this->ioTable.remove ( id );
        if ( pmiu ) {
            pmiu->channel ().tcpiiuPrivateListOfIO::eventq.remove ( *pmiu );
        }
    }

    if ( pmiu ) {
        pmiu->exceptionNotify ( status, pContext );
        delete pmiu;
    }
}

void tcpiiu::ioExceptionNotifyAndDestroy ( unsigned id, int status, 
                        const char *pContext, unsigned type, unsigned long count )
{
    baseNMIU * pmiu;

    {
        epicsAutoMutex autoMutex ( this->mutex );
        pmiu = this->ioTable.remove ( id );
        if ( pmiu ) {
            pmiu->channel ().tcpiiuPrivateListOfIO::eventq.remove ( *pmiu );
        }
    }

    if ( pmiu ) {
        pmiu->exceptionNotify ( status, pContext, type, count );
        delete pmiu; 
    }
}

// resubscribe for monitors from this channel
void tcpiiu::connectAllIO ( nciu &chan )
{
    epicsAutoMutex autoMutex ( this->mutex );
    if ( chan.verifyConnected ( *this ) ) {
        tsDLIterBD < baseNMIU > pNetIO = 
            chan.tcpiiuPrivateListOfIO::eventq.firstIter ();
        while ( pNetIO.valid () ) {
            tsDLIterBD < baseNMIU > next = pNetIO;
            next++;
            class netSubscription *pSubscr = pNetIO->isSubscription ();
            if ( pSubscr ) {
                this->subscriptionRequest ( *pSubscr, false );
            }
            else {
                // it shouldnt be here at this point - so uninstall it
                this->ioTable.remove ( *pNetIO );
                chan.tcpiiuPrivateListOfIO::eventq.remove ( *pNetIO );
                pNetIO->exceptionNotify ( ECA_DISCONN, this->pHostName () );
                pNetIO->destroy ();
            }
            pNetIO = next;
        }
    }
    this->flush ();
}

// cancel IO operations and monitor subscriptions
void tcpiiu::disconnectAllIO ( nciu &chan )
{
    epicsAutoMutex autoMutex ( this->mutex );
    if ( chan.verifyConnected ( *this ) ) {
        tsDLIterBD < baseNMIU > pNetIO = 
            chan.tcpiiuPrivateListOfIO::eventq.firstIter ();
        while ( pNetIO.valid () ) {
            tsDLIterBD < baseNMIU > next = pNetIO;
            next++;
            class netSubscription *pSubscr = pNetIO->isSubscription ();
            this->ioTable.remove ( *pNetIO );
            if ( pSubscr ) {
                this->subscriptionCancelRequest ( *pSubscr, false );
            }
            else {
                // no use after disconnected - so uninstall it
                chan.tcpiiuPrivateListOfIO::eventq.remove ( *pNetIO );
                pNetIO->exceptionNotify ( ECA_DISCONN, this->pHostName () );
                pNetIO->destroy ();
            }
            pNetIO = next;
        }
    }
}

//
// care is taken to not hold the lock while deleting the
// IO so that subscription delete request (sent by the
// IO's destructor) do not deadlock
//
bool tcpiiu::destroyAllIO ( nciu &chan )
{
    tsDLList < baseNMIU > eventQ;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        if ( chan.verifyIIU ( *this ) ) {
            while ( baseNMIU *pIO = eventQ.get () ) {
                this->ioTable.remove ( *pIO );
                eventQ.add ( *pIO );
            }
        }
        else {
            return false;
        }
    }
    while ( baseNMIU *pIO = eventQ.get () ) {
        pIO->destroy ();
    }
    return true;
}

bool tcpiiu::uninstallIO ( baseNMIU &io )
{
    epicsAutoMutex autoMutex ( this->mutex );
    if ( io.channel ().verifyIIU ( *this ) ) {
        this->ioTable.remove ( io );
    }
    else {
        return false;
    }
    io.channel ().tcpiiuPrivateListOfIO::eventq.remove ( io );
    return true;
}

double tcpiiu::beaconPeriod () const
{
    epicsAutoMutex autoMutex ( this->mutex );
    if ( this->pBHE ) {
        return this->pBHE->period ();
    }
    else {
        return netiiu::beaconPeriod ();
    }
}


