/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifdef _MSC_VER
#   pragma warning(disable:4355)
#endif

#include <stdexcept>

#include <stdio.h>

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"

extern epicsThreadPrivateId caClientContextId;

ca_client_context::ca_client_context ( bool enablePreemptiveCallback ) :
    ca_exception_func ( 0 ), ca_exception_arg ( 0 ), 
    pVPrintfFunc ( errlogVprintf ), fdRegFunc ( 0 ), fdRegArg ( 0 ),
    pndRecvCnt ( 0u ), ioSeqNo ( 0u ), callbackThreadsPending ( 0u ), 
    localPort ( 0 ), fdRegFuncNeedsToBeCalled ( false ), 
    noWakeupSincePend ( true )
{
    static const unsigned short PORT_ANY = 0u;

    this->sock = epicsSocketCreate ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( this->sock == INVALID_SOCKET ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        this->printf (
            "ca_client_context: unable to create "
            "datagram socket because = \"%s\"\n",
            sockErrBuf );
        throwWithLocation ( noSocket () );
    }

    {
        osiSockIoctl_t yes = true;
        int status = socket_ioctl ( this->sock, // X aCC 392
                                    FIONBIO, & yes);
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            epicsSocketDestroy ( this->sock );
            this->printf (
                "%s: non blocking IO set fail because \"%s\"\n",
                            __FILE__, sockErrBuf );
            throwWithLocation ( noSocket () );
        }
    }

    // force a bind to an unconstrained address so we can obtain
    // the local port number below
    {
        osiSockAddr addr;
        memset ( (char *)&addr, 0 , sizeof ( addr ) );
        addr.ia.sin_family = AF_INET;
        addr.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_ANY ); 
        addr.ia.sin_port = epicsHTON16 ( PORT_ANY ); // X aCC 818
        int status = bind (this->sock, &addr.sa, sizeof (addr) );
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            epicsSocketDestroy (this->sock);
            this->printf (
                "CAC: unable to bind to an unconstrained "
                "address because = \"%s\"\n",
                sockErrBuf );
            throwWithLocation ( noSocket () );
        }
    }
    
    {
        osiSockAddr tmpAddr;
        osiSocklen_t saddr_length = sizeof ( tmpAddr );
        int status = getsockname ( this->sock, & tmpAddr.sa, & saddr_length );
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            epicsSocketDestroy ( this->sock );
            this->printf ( "CAC: getsockname () error was \"%s\"\n", sockErrBuf );
            throwWithLocation ( noSocket () );
        }
        if ( tmpAddr.sa.sa_family != AF_INET) {
            epicsSocketDestroy ( this->sock );
            this->printf ( "CAC: UDP socket was not inet addr family\n" );
            throwWithLocation ( noSocket () );
        }
        this->localPort = epicsNTOH16 ( tmpAddr.ia.sin_port );
    }

    epics_auto_ptr < cac > pCAC ( 
        new cac ( *this ) );

    epics_auto_ptr < epicsGuard < epicsMutex > > pCBGuard;
    if ( ! enablePreemptiveCallback ) {
        pCBGuard.reset ( new epicsGuard < epicsMutex > ( this->callbackMutex ) );
    }

    // multiple steps ensure exception safety
    this->pCallbackGuard = pCBGuard;
    this->pClientCtx = pCAC;
}

ca_client_context::~ca_client_context ()
{
    if ( this->fdRegFunc ) {
        ( *this->fdRegFunc ) 
            ( this->fdRegArg, this->sock, false );
    }
    epicsSocketDestroy ( this->sock );
}

void ca_client_context::destroyChannel ( oldChannelNotify & chan )
{
    chan.~oldChannelNotify ();
    this->oldChannelNotifyFreeList.release ( & chan );
}

void ca_client_context::destroyGetCopy ( getCopy & gc )
{
    gc.~getCopy ();
    this->getCopyFreeList.release ( & gc );
}

void ca_client_context::destroyGetCallback ( getCallback & gcb )
{
    gcb.~getCallback ();
    this->getCallbackFreeList.release ( & gcb );
}

void ca_client_context::destroyPutCallback ( putCallback & pcb )
{
    pcb.~putCallback ();
    this->putCallbackFreeList.release ( & pcb );
}

void ca_client_context::destroySubscription ( oldSubscription & os )
{
    os.~oldSubscription ();
    this->subscriptionFreeList.release ( & os );
}

void ca_client_context::changeExceptionEvent ( caExceptionHandler *pfunc, void *arg )
{
    epicsGuard < ca_client_context_mutex > guard ( this->mutex );
    this->ca_exception_func = pfunc;
    this->ca_exception_arg = arg;
// should block here until releated callback in progress completes
}

void ca_client_context::replaceErrLogHandler ( caPrintfFunc *ca_printf_func )
{
    epicsGuard < ca_client_context_mutex > autoMutex ( this->mutex );
    if ( ca_printf_func ) {
        this->pVPrintfFunc = ca_printf_func;
    }
    else {
        this->pVPrintfFunc = epicsVprintf;
    }
// should block here until releated callback in progress completes
}

void ca_client_context::registerForFileDescriptorCallBack ( CAFDHANDLER *pFunc, void *pArg )
{
    epicsGuard < ca_client_context_mutex > autoMutex ( this->mutex );
    this->fdRegFunc = pFunc;
    this->fdRegArg = pArg;
    this->fdRegFuncNeedsToBeCalled = true;
// should block here until releated callback in progress completes
}

int ca_client_context::printf ( const char *pformat, ... ) const
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->ca_client_context::vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

int ca_client_context::vPrintf ( const char *pformat, va_list args ) const // X aCC 361
{
    caPrintfFunc *pFunc;
    {
        epicsGuard < ca_client_context_mutex > autoMutex ( this->mutex );
        pFunc = this->pVPrintfFunc;
    }
    if ( pFunc ) {
        return ( *pFunc ) ( pformat, args );
    }
    else {
        return ::vfprintf ( stderr, pformat, args );
    }
}

void ca_client_context::exception ( int stat, const char *pCtx, 
                        const char *pFile, unsigned lineNo )
{
    struct exception_handler_args args;
    caExceptionHandler *pFunc;
    void *pArg;
    {
        epicsGuard < ca_client_context_mutex > autoMutex ( this->mutex );
        pFunc = this->ca_exception_func;
        pArg = this->ca_exception_arg;
    }

    // NOOP if they disable exceptions
    if ( pFunc ) {
        args.chid = NULL;
        args.type = TYPENOTCONN;
        args.count = 0;
        args.addr = NULL;
        args.stat = stat;
        args.op = CA_OP_OTHER;
        args.ctx = pCtx;
        args.pFile = pFile;
        args.lineNo = lineNo;
        args.usr = pArg;
        ( *pFunc ) ( args );
    }
    else {
        this->pClientCtx->signal ( stat, pFile, lineNo, pCtx );
    }
}

void ca_client_context::exception ( int status, const char *pContext,
    const char *pFileName, unsigned lineNo, oldChannelNotify &chan, 
    unsigned type, arrayElementCount count, unsigned op )
{
    struct exception_handler_args args;
    caExceptionHandler *pFunc;
    void *pArg;
    {
        epicsGuard < ca_client_context_mutex > autoMutex ( this->mutex );
        pFunc = this->ca_exception_func;
        pArg = this->ca_exception_arg;
    }

    // NOOP if they disable exceptions
    if ( pFunc ) {
        args.chid = &chan;
        args.type = type;
        args.count = count;
        args.addr = NULL;
        args.stat = status;
        args.op = op;
        args.ctx = pContext;
        args.pFile = pFileName;
        args.lineNo = lineNo;
        args.usr = pArg;
        ( *pFunc ) ( args );
    }
    else {
        this->pClientCtx->signal ( status, pFileName, lineNo, 
            "op=%u, channel=%s, type=%s, count=%lu, ctx=\"%s\"", 
            op, ca_name ( &chan ), 
            dbr_type_to_text ( static_cast <int> ( type ) ), 
            count, pContext );
    }
}

void ca_client_context::show ( unsigned level ) const
{
    ::printf ( "ca_client_context at %p pndRecvCnt=%u ioSeqNo=%u\n", 
        static_cast <const void *> ( this ),
        this->pndRecvCnt, this->ioSeqNo );
    if ( level > 0u ) {
        this->mutex.show ( level - 1u );
        this->pClientCtx->show ( level - 1u );
        ::printf ( "\tpreemptive callback is %s\n",
            this->pCallbackGuard.get() ? "disabled" : "enabled" );
        ::printf ( "\tthere are %u unsatisfied IO operations blocking ca_pend_io()\n",
                this->pndRecvCnt );
        ::printf ( "\tthe current io sequence number is %u\n",
                this->ioSeqNo );
        ::printf ( "IO done event:\n");
        this->ioDone.show ( level - 1u );
    }
}

void ca_client_context::attachToClientCtx ()
{
    assert ( ! epicsThreadPrivateGet ( caClientContextId ) );
    epicsThreadPrivateSet ( caClientContextId, this );
}

void ca_client_context::incrementOutstandingIO ( unsigned ioSeqNoIn )
{
    epicsGuard < ca_client_context_mutex > guard ( this->mutex );
    if ( this->ioSeqNo == ioSeqNoIn ) {
        assert ( this->pndRecvCnt < UINT_MAX );
        this->pndRecvCnt++;
    }
}

void ca_client_context::decrementOutstandingIO ( unsigned ioSeqNoIn )
{
    bool signalNeeded;
    {
        epicsGuard < ca_client_context_mutex > guard ( this->mutex ); 
        if ( this->ioSeqNo == ioSeqNoIn ) {
            assert ( this->pndRecvCnt > 0u );
            this->pndRecvCnt--;
            if ( this->pndRecvCnt == 0u ) {
                signalNeeded = true;
            }
            else {
                signalNeeded = false;
            }
        }
        else {
            signalNeeded = false;
        }
    }

    if ( signalNeeded ) {
        this->ioDone.signal ();
    }
}

// !!!! This routine is only visible in the old interface - or in a new ST interface. 
// !!!! In the old interface we restrict thread attach so that calls from threads 
// !!!! other than the initializing thread are not allowed if preemptive callback 
// !!!! is disabled. This prevents the preemptive callback lock from being released
// !!!! by other threads than the one that locked it.
//
int ca_client_context::pendIO ( const double & timeout )
{
    // prevent recursion nightmares by disabling calls to 
    // pendIO () from within a CA callback. 
    if ( epicsThreadPrivateGet ( caClientCallbackThreadId ) ) {
        return ECA_EVDISALLOW;
    }

    int status = ECA_NORMAL;
    epicsTime beg_time = epicsTime::getCurrent ();
    double remaining = timeout;

    this->flushRequest ();
   
    while ( this->pndRecvCnt > 0 ) {
        if ( remaining < CAC_SIGNIFICANT_DELAY ) {
            status = ECA_TIMEOUT;
            break;
        }

        this->blockForEventAndEnableCallbacks ( this->ioDone, remaining );

        double delay = epicsTime::getCurrent () - beg_time;
        if ( delay < timeout ) {
            remaining = timeout - delay;
        }
        else {
            remaining = 0.0;
        }
    }

    {
        epicsGuard < ca_client_context_mutex > guard ( this->mutex );
        this->ioSeqNo++;
        this->pndRecvCnt = 0u;
    }

    return status;
}

// !!!! This routine is only visible in the old interface - or in a new ST interface. 
// !!!! In the old interface we restrict thread attach so that calls from threads 
// !!!! other than the initializing thread are not allowed if preemptive callback 
// !!!! is disabled. This prevents the preemptive callback lock from being released
// !!!! by other threads than the one that locked it.
//
int ca_client_context::pendEvent ( const double & timeout )
{
    // prevent recursion nightmares by disabling calls to 
    // pendIO () from within a CA callback. 
    if ( epicsThreadPrivateGet ( caClientCallbackThreadId ) ) {
        return ECA_EVDISALLOW;
    }

    epicsTime current = epicsTime::getCurrent ();

    this->flushRequest ();

    // process at least once if preemptive callback is disabled
    if ( this->pCallbackGuard.get() ) {
        //
        // This is needed because in non-preemptive callback mode 
        // legacy applications that use file descriptor managers 
        // will register for ca receive thread activity and keep
        // calling ca_pend_event until all of the socket data has
        // been read. We must guarantee that other threads get a 
        // chance to run if there is data in any of the sockets.
        //
        epicsGuardRelease < epicsMutex > unguardcb ( *this->pCallbackGuard );
        epicsGuard < ca_client_context_mutex > guard ( this->mutex );
        if ( this->fdRegFunc ) {
            epicsGuardRelease < ca_client_context_mutex > unguard ( guard );
            // remove short udp message sent to wake up a file descriptor manager
            osiSockAddr tmpAddr;
            osiSocklen_t addrSize = sizeof ( tmpAddr.sa );
            char buf = 0;
            int status = 0;
            do {
                status = recvfrom ( this->sock, & buf, sizeof ( buf ),
                        0, & tmpAddr.sa, & addrSize );
            } while ( status > 0 );
        }
        this->noWakeupSincePend = true;
        while ( this->callbackThreadsPending > 0 ) {
            epicsGuardRelease < ca_client_context_mutex > unguard ( guard );
            this->callbackThreadActivityComplete.wait ( 30.0 );
        }
    }

    double elapsed = epicsTime::getCurrent() - current;
    double delay;

    if ( timeout > elapsed ) {
        delay = timeout - elapsed;
    }
    else {
        delay = 0.0;
    }

    if ( delay >= CAC_SIGNIFICANT_DELAY ) {
        if ( this->pCallbackGuard.get() ) {
            epicsGuardRelease < epicsMutex > unguard ( *this->pCallbackGuard );
            epicsThreadSleep ( delay );
        }
        else {
            epicsThreadSleep ( delay );
        }
    }

    return ECA_TIMEOUT;
}

void ca_client_context::blockForEventAndEnableCallbacks ( 
    epicsEvent & event, const double & timeout )
{
    if ( this->pCallbackGuard.get() ) {
        epicsGuardRelease < epicsMutex > unguard ( *this->pCallbackGuard );
        event.wait ( timeout );
    }
    else {
        event.wait ( timeout );
    }
}

void ca_client_context::callbackLock ()
{

    // if preemptive callback is enabled then this is a noop
    if ( this->pCallbackGuard.get() ) {
        bool sendNeeded = false;
        {
            epicsGuard < ca_client_context_mutex > guard ( this->mutex );
            this->callbackThreadsPending++;
            if ( this->fdRegFunc && this->noWakeupSincePend ) {
                this->noWakeupSincePend = false;
                sendNeeded = true;
            }
        }
        if ( sendNeeded ) {
            // send short udp message to wake up a file descriptor manager
            // when a message arrives
            osiSockAddr tmpAddr;
            tmpAddr.ia.sin_family = AF_INET;
            tmpAddr.ia.sin_addr.s_addr = epicsHTON32 ( INADDR_LOOPBACK );
            tmpAddr.ia.sin_port = epicsHTON16 ( this->localPort );
            char buf = 0;
            sendto ( this->sock, & buf, sizeof ( buf ),
                    0, & tmpAddr.sa, sizeof ( tmpAddr.sa ) );
        }
    }

    this->callbackMutex.lock ();
}

void ca_client_context::callbackUnlock ()
{
    this->callbackMutex.unlock ();

    // if preemptive callback is enabled then this is a noop
    if ( this->pCallbackGuard.get() ) {
        bool signalNeeded = false;
        {
            epicsGuard < ca_client_context_mutex > guard ( this->mutex );
            if ( this->callbackThreadsPending <= 1 ) {
                if ( this->callbackThreadsPending == 1 ) {
                    this->callbackThreadsPending = 0;
                    signalNeeded = true;
                }
            }
            else {
                this->callbackThreadsPending--;
            }
        }
        if ( signalNeeded ) {
            this->callbackThreadActivityComplete.signal ();
        }
    }
}

void ca_client_context::changeConnCallBack ( 
    caCh * pfunc, caCh * & pConnCallBack, const bool & currentlyConnected )
{
    epicsGuard < epicsMutex > callbackGuard ( this->callbackMutex );
    if ( ! currentlyConnected ) {
         if ( pfunc ) { 
            if ( ! pConnCallBack ) {
                this->decrementOutstandingIO ( this->ioSeqNo );
            }
        }
        else {
            if ( pConnCallBack ) {
                this->incrementOutstandingIO ( this->ioSeqNo );
            }
        }
    }
    pConnCallBack = pfunc;
}

void ca_client_context::registerService ( cacService &service )
{
    this->pClientCtx->registerService ( service );
}

cacChannel & ca_client_context::createChannel ( const char * name_str, 
             oldChannelNotify & chan, cacChannel::priLev pri )
{
    return this->pClientCtx->createChannel ( name_str, chan, pri );
}

void ca_client_context::flushRequest ()
{
    this->pClientCtx->flushRequest ();
}

unsigned ca_client_context::connectionCount () const
{
    return this->pClientCtx->connectionCount ();
}

unsigned ca_client_context::beaconAnomaliesSinceProgramStart () const
{
    return this->pClientCtx->beaconAnomaliesSinceProgramStart ();
}

CASG * ca_client_context::lookupCASG ( unsigned id )
{
    return this->pClientCtx->lookupCASG ( id );
}

void ca_client_context::installCASG ( CASG &sg )
{
    this->pClientCtx->installCASG ( sg );
}

void ca_client_context::uninstallCASG ( CASG &sg )
{
    this->pClientCtx->uninstallCASG ( sg );
}

void ca_client_context::vSignal ( int ca_status, const char *pfilenm, 
                     int lineno, const char *pFormat, va_list args )
{
    this->pClientCtx->vSignal ( ca_status, pfilenm, 
                     lineno, pFormat, args );
}

void ca_client_context::selfTest ()
{
    this->pClientCtx->selfTest ();
}
