/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
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
#include <string> // vxWorks 6.0 requires this include
#include <stdio.h>

#include "epicsExit.h"
#include "errlog.h"
#include "locationException.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"

epicsShareDef epicsThreadPrivateId caClientCallbackThreadId;

static epicsThreadOnceId cacOnce = EPICS_THREAD_ONCE_INIT;

const unsigned ca_client_context :: flushBlockThreshold = 0x58000;

extern "C" void cacExitHandler ( void *)
{
    epicsThreadPrivateDelete ( caClientCallbackThreadId );
    caClientCallbackThreadId = 0;
    delete ca_client_context::pDefaultServiceInstallMutex;
}

// runs once only for each process
extern "C" void cacOnceFunc ( void * )
{
    caClientCallbackThreadId = epicsThreadPrivateCreate ();
    assert ( caClientCallbackThreadId );
    ca_client_context::pDefaultServiceInstallMutex = newEpicsMutex;
    epicsAtExit ( cacExitHandler,0 );
}

extern epicsThreadPrivateId caClientContextId;

cacService * ca_client_context::pDefaultService = 0;
epicsMutex * ca_client_context::pDefaultServiceInstallMutex;

ca_client_context::ca_client_context ( bool enablePreemptiveCallback ) :
    createdByThread ( epicsThreadGetIdSelf () ),
    ca_exception_func ( 0 ), ca_exception_arg ( 0 ),
    pVPrintfFunc ( errlogVprintf ), fdRegFunc ( 0 ), fdRegArg ( 0 ),
    pndRecvCnt ( 0u ), ioSeqNo ( 0u ), callbackThreadsPending ( 0u ),
    localPort ( 0 ), fdRegFuncNeedsToBeCalled ( false ),
    noWakeupSincePend ( true )
{
    static const unsigned short PORT_ANY = 0u;

    if ( ! osiSockAttach () ) {
        throwWithLocation ( noSocket () );
    }

    epicsThreadOnce ( & cacOnce, cacOnceFunc, 0 );
    {
        epicsGuard < epicsMutex > guard ( *ca_client_context::pDefaultServiceInstallMutex );
        if ( ca_client_context::pDefaultService ) {
            this->pServiceContext.reset (
                & ca_client_context::pDefaultService->contextCreate (
                    this->mutex, this->cbMutex, *this ) );
        }
        else {
            this->pServiceContext.reset ( new cac ( this->mutex, this->cbMutex, *this ) );
        }
    }

    this->sock = epicsSocketCreate ( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    if ( this->sock == INVALID_SOCKET ) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        this->printFormated (
            "ca_client_context: unable to create "
            "datagram socket because = \"%s\"\n",
            sockErrBuf );
        throwWithLocation ( noSocket () );
    }

    {
        osiSockIoctl_t yes = true;
        int status = socket_ioctl ( this->sock,
                                    FIONBIO, & yes);
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            epicsSocketDestroy ( this->sock );
            this->printFormated (
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
        addr.ia.sin_addr.s_addr = htonl ( INADDR_ANY );
        addr.ia.sin_port = htons ( PORT_ANY );
        int status = bind (this->sock, &addr.sa, sizeof (addr) );
        if ( status < 0 ) {
            char sockErrBuf[64];
            epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
            epicsSocketDestroy (this->sock);
            this->printFormated (
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
            this->printFormated ( "CAC: getsockname () error was \"%s\"\n", sockErrBuf );
            throwWithLocation ( noSocket () );
        }
        if ( tmpAddr.sa.sa_family != AF_INET) {
            epicsSocketDestroy ( this->sock );
            this->printFormated ( "CAC: UDP socket was not inet addr family\n" );
            throwWithLocation ( noSocket () );
        }
        this->localPort = htons ( tmpAddr.ia.sin_port );
    }

    std::auto_ptr < CallbackGuard > pCBGuard;
    if ( ! enablePreemptiveCallback ) {
        pCBGuard.reset ( new CallbackGuard ( this->cbMutex ) );
    }

    // multiple steps ensure exception safety
    this->pCallbackGuard = pCBGuard;
}

ca_client_context::~ca_client_context ()
{
    if ( this->fdRegFunc ) {
        ( *this->fdRegFunc )
            ( this->fdRegArg, this->sock, false );
    }
    epicsSocketDestroy ( this->sock );

    osiSockRelease ();

    // force a logical shutdown order
    // so that the cac class does not hang its
    // receive threads during their shutdown sequence
    // and so that classes using this classes mutex
    // are destroyed before the mutex is destroyed
    if ( this->pCallbackGuard.get() ) {
        epicsGuardRelease < epicsMutex > unguard ( *this->pCallbackGuard );
        this->pServiceContext.reset ( 0 );
    }
    else {
        this->pServiceContext.reset ( 0 );
    }
}

void ca_client_context::destroyGetCopy (
    epicsGuard < epicsMutex > & guard, getCopy & gc )
{
    guard.assertIdenticalMutex ( this->mutex );
    gc.~getCopy ();
    this->getCopyFreeList.release ( & gc );
}

void ca_client_context::destroyGetCallback (
    epicsGuard < epicsMutex > & guard, getCallback & gcb )
{
    guard.assertIdenticalMutex ( this->mutex );
    gcb.~getCallback ();
    this->getCallbackFreeList.release ( & gcb );
}

void ca_client_context::destroyPutCallback (
    epicsGuard < epicsMutex > & guard, putCallback & pcb )
{
    guard.assertIdenticalMutex ( this->mutex );
    pcb.~putCallback ();
    this->putCallbackFreeList.release ( & pcb );
}

void ca_client_context::destroySubscription (
    epicsGuard < epicsMutex > & guard, oldSubscription & os )
{
    guard.assertIdenticalMutex ( this->mutex );
    os.~oldSubscription ();
    this->subscriptionFreeList.release ( & os );
}

void ca_client_context::changeExceptionEvent (
    caExceptionHandler * pfunc, void * arg )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->ca_exception_func = pfunc;
    this->ca_exception_arg = arg;
// should block here until releated callback in progress completes
}

void ca_client_context::replaceErrLogHandler (
    caPrintfFunc * ca_printf_func )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    if ( ca_printf_func ) {
        this->pVPrintfFunc = ca_printf_func;
    }
    else {
        this->pVPrintfFunc = epicsVprintf;
    }
// should block here until releated callback in progress completes
}

void ca_client_context::registerForFileDescriptorCallBack (
    CAFDHANDLER *pFunc, void *pArg )
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->fdRegFunc = pFunc;
    this->fdRegArg = pArg;
    this->fdRegFuncNeedsToBeCalled = true;
    if ( pFunc ) {
        // the receive thread might already be blocking
        // w/o having sent the wakeup message
        this->_sendWakeupMsg ();
    }
// should block here until releated callback in progress completes
}

int ca_client_context :: printFormated (
    const char *pformat, ... ) const
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );

    status = this->ca_client_context :: varArgsPrintFormated ( pformat, theArgs );

    va_end ( theArgs );

    return status;
}

int ca_client_context :: varArgsPrintFormated (
    const char *pformat, va_list args ) const
{
    caPrintfFunc * pFunc;
    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        pFunc = this->pVPrintfFunc;
    }
    if ( pFunc ) {
        return ( *pFunc ) ( pformat, args );
    }
    else {
        return :: vfprintf ( stderr, pformat, args );
    }
}

void ca_client_context::exception (
    epicsGuard < epicsMutex > & guard, int stat, const char * pCtx,
        const char * pFile, unsigned lineNo )
{
    struct exception_handler_args args;
    caExceptionHandler * pFunc = this->ca_exception_func;
    void * pArg = this->ca_exception_arg;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
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
            this->signal ( stat, pFile, lineNo, pCtx );
        }
    }
}

void ca_client_context::exception (
    epicsGuard < epicsMutex > & guard, int status, const char * pContext,
    const char * pFileName, unsigned lineNo, oldChannelNotify & chan,
    unsigned type, arrayElementCount count, unsigned op )
{
    struct exception_handler_args args;
    caExceptionHandler * pFunc = this->ca_exception_func;
    void * pArg = this->ca_exception_arg;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
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
            this->signal ( status, pFileName, lineNo,
                "op=%u, channel=%s, type=%s, count=%lu, ctx=\"%s\"",
                op, ca_name ( &chan ),
                dbr_type_to_text ( static_cast <int> ( type ) ),
                count, pContext );
        }
    }
}

void ca_client_context::signal ( int ca_status, const char * pfilenm,
                     int lineno, const char * pFormat, ... )
{
    va_list theArgs;
    va_start ( theArgs, pFormat );
    this->vSignal ( ca_status, pfilenm, lineno, pFormat, theArgs);
    va_end ( theArgs );
}

void ca_client_context :: vSignal (
    int ca_status, const char *pfilenm,
        int lineno, const char *pFormat, va_list args )
{
    static const char *severity[] =
    {
        "Warning",
        "Success",
        "Error",
        "Info",
        "Fatal",
        "Fatal",
        "Fatal",
        "Fatal"
    };

    this->printFormated ( "CA.Client.Exception...............................................\n" );

    this->printFormated ( "    %s: \"%s\"\n",
        severity[ CA_EXTRACT_SEVERITY ( ca_status ) ],
        ca_message ( ca_status ) );

    if  ( pFormat ) {
        this->printFormated ( "    Context: \"" );
        this->varArgsPrintFormated ( pFormat, args );
        this->printFormated ( "\"\n" );
    }

    if ( pfilenm ) {
        this->printFormated ( "    Source File: %s line %d\n",
            pfilenm, lineno );
    }

    epicsTime current = epicsTime::getCurrent ();
    char date[64];
    current.strftime ( date, sizeof ( date ), "%a %b %d %Y %H:%M:%S.%f");
    this->printFormated ( "    Current Time: %s\n", date );

    /*
     *  Terminate execution if unsuccessful
     */
    if( ! ( ca_status & CA_M_SUCCESS ) &&
        CA_EXTRACT_SEVERITY ( ca_status ) != CA_K_WARNING ){
        errlogFlush ();
        abort ();
    }

    this->printFormated ( "..................................................................\n" );
}

void ca_client_context::show ( unsigned level ) const
{
    epicsGuard < epicsMutex > guard ( this->mutex );

    ::printf ( "ca_client_context at %p pndRecvCnt=%u ioSeqNo=%u\n",
        static_cast <const void *> ( this ),
        this->pndRecvCnt, this->ioSeqNo );

    if ( level > 0u ) {
        this->pServiceContext->show ( guard, level - 1u );
        ::printf ( "\tpreemptive callback is %s\n",
            this->pCallbackGuard.get() ? "disabled" : "enabled" );
        ::printf ( "\tthere are %u unsatisfied IO operations blocking ca_pend_io()\n",
                this->pndRecvCnt );
        ::printf ( "\tthe current io sequence number is %u\n",
                this->ioSeqNo );
        ::printf ( "IO done event:\n");
        this->ioDone.show ( level - 1u );
        ::printf ( "Synchronous group identifier hash table:\n" );
        this->sgTable.show ( level - 1u );
    }
}

void ca_client_context::attachToClientCtx ()
{
    assert ( ! epicsThreadPrivateGet ( caClientContextId ) );
    epicsThreadPrivateSet ( caClientContextId, this );
}

void ca_client_context::incrementOutstandingIO (
    epicsGuard < epicsMutex > & guard, unsigned ioSeqNoIn )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->ioSeqNo == ioSeqNoIn ) {
        assert ( this->pndRecvCnt < UINT_MAX );
        this->pndRecvCnt++;
    }
}

void ca_client_context::decrementOutstandingIO (
    epicsGuard < epicsMutex > & guard, unsigned ioSeqNoIn )
{
    guard.assertIdenticalMutex ( this->mutex );
    if ( this->ioSeqNo == ioSeqNoIn ) {
        assert ( this->pndRecvCnt > 0u );
        this->pndRecvCnt--;
        if ( this->pndRecvCnt == 0u ) {
            this->ioDone.signal ();
        }
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

    epicsGuard < epicsMutex > guard ( this->mutex );

    this->flush ( guard );

    while ( this->pndRecvCnt > 0 ) {
        if ( remaining < CAC_SIGNIFICANT_DELAY ) {
            status = ECA_TIMEOUT;
            break;
        }

        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->blockForEventAndEnableCallbacks ( this->ioDone, remaining );
        }

        double delay = epicsTime::getCurrent () - beg_time;
        if ( delay < timeout ) {
            remaining = timeout - delay;
        }
        else {
            remaining = 0.0;
        }
    }

    this->ioSeqNo++;
    this->pndRecvCnt = 0u;

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

    {
        epicsGuard < epicsMutex > guard ( this->mutex );
        this->flush ( guard );
    }

    // process at least once if preemptive callback is disabled
    if ( this->pCallbackGuard.get() ) {
        epicsGuardRelease < epicsMutex > cbUnguard ( *this->pCallbackGuard );
        epicsGuard < epicsMutex > guard ( this->mutex );

        //
        // This is needed because in non-preemptive callback mode
        // legacy applications that use file descriptor managers
        // will register for ca receive thread activity and keep
        // calling ca_pend_event until all of the socket data has
        // been read. We must guarantee that other threads get a
        // chance to run if there is data in any of the sockets.
        //
        if ( this->fdRegFunc ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );

            // remove short udp message sent to wake
            // up a file descriptor manager
            osiSockAddr tmpAddr;
            osiSocklen_t addrSize = sizeof ( tmpAddr.sa );
            char buf = 0;
            int status = 0;
            do {
                status = recvfrom ( this->sock, & buf, sizeof ( buf ),
                        0, & tmpAddr.sa, & addrSize );
            } while ( status > 0 );
        }
        while ( this->callbackThreadsPending > 0 ) {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            this->callbackThreadActivityComplete.wait ( 30.0 );
        }
        this->noWakeupSincePend = true;
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

void ca_client_context::callbackProcessingInitiateNotify ()
{
    // if preemptive callback is enabled then this is a noop
    if ( this->pCallbackGuard.get() ) {
        bool sendNeeded = false;
        {
            epicsGuard < epicsMutex > guard ( this->mutex );
            this->callbackThreadsPending++;
            if ( this->fdRegFunc && this->noWakeupSincePend ) {
                this->noWakeupSincePend = false;
                sendNeeded = true;
            }
        }
        if ( sendNeeded ) {
            _sendWakeupMsg ();
        }
    }
}

void ca_client_context :: _sendWakeupMsg ()
{
    // send short udp message to wake up a file descriptor manager
    // when a message arrives
    osiSockAddr tmpAddr;
    tmpAddr.ia.sin_family = AF_INET;
    tmpAddr.ia.sin_addr.s_addr = htonl ( INADDR_LOOPBACK );
    tmpAddr.ia.sin_port = htons ( this->localPort );
    char buf = 0;
    sendto ( this->sock, & buf, sizeof ( buf ),
            0, & tmpAddr.sa, sizeof ( tmpAddr.sa ) );
}

void ca_client_context::callbackProcessingCompleteNotify ()
{
    // if preemptive callback is enabled then this is a noop
    if ( this->pCallbackGuard.get() ) {
        bool signalNeeded = false;
        {
            epicsGuard < epicsMutex > guard ( this->mutex );
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

cacChannel & ca_client_context::createChannel (
    epicsGuard < epicsMutex > & guard, const char * pChannelName,
    cacChannelNotify & chan, cacChannel::priLev pri )
{
    guard.assertIdenticalMutex ( this->mutex );
    return this->pServiceContext->createChannel (
        guard, pChannelName, chan, pri );
}

void ca_client_context::flush ( epicsGuard < epicsMutex > & guard )
{
    this->pServiceContext->flush ( guard );
}

unsigned ca_client_context::circuitCount () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->pServiceContext->circuitCount ( guard );
}

unsigned ca_client_context::beaconAnomaliesSinceProgramStart () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    return this->pServiceContext->beaconAnomaliesSinceProgramStart ( guard );
}

void ca_client_context::installCASG (
    epicsGuard < epicsMutex > & guard, CASG & sg )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->sgTable.idAssignAdd ( sg );
}

void ca_client_context::uninstallCASG (
    epicsGuard < epicsMutex > & guard, CASG & sg )
{
    guard.assertIdenticalMutex ( this->mutex );
    this->sgTable.remove ( sg );
}

CASG * ca_client_context::lookupCASG (
    epicsGuard < epicsMutex > & guard, unsigned idIn )
{
    guard.assertIdenticalMutex ( this->mutex );
    CASG * psg = this->sgTable.lookup ( idIn );
    if ( psg ) {
        if ( ! psg->verify ( guard ) ) {
            psg = 0;
        }
    }
    return psg;
}

void ca_client_context::selfTest () const
{
    epicsGuard < epicsMutex > guard ( this->mutex );
    this->sgTable.verify ();
    this->pServiceContext->selfTest ( guard );
}

epicsMutex & ca_client_context::mutexRef () const
{
    return this->mutex;
}

cacContext & ca_client_context::createNetworkContext (
    epicsMutex & mutexIn, epicsMutex & cbMutexIn )
{
    return * new cac ( mutexIn, cbMutexIn, *this );
}

void ca_client_context::installDefaultService ( cacService & service )
{
    epicsThreadOnce ( & cacOnce, cacOnceFunc, 0 );

    epicsGuard < epicsMutex > guard ( *ca_client_context::pDefaultServiceInstallMutex );
    if ( ca_client_context::pDefaultService ) {
        throw std::logic_error
            ( "CA in-memory service already installed and can't be replaced");
    }
    ca_client_context::pDefaultService = & service;
}

void epicsShareAPI caInstallDefaultService ( cacService & service )
{
    ca_client_context::installDefaultService ( service );
}

epicsShareFunc int epicsShareAPI ca_clear_subscription ( evid pMon )
{
    oldChannelNotify & chan = pMon->channel ();
    ca_client_context & cac = chan.getClientCtx ();
    // !!!! the order in which we take the mutex here prevents deadlocks
    {
        epicsGuard < epicsMutex > guard ( cac.mutex );
        try {
            // if this stalls out on a live circuit then an exception
            // can be forthcoming which we must ignore as the clear
            // request must always be successful
            chan.eliminateExcessiveSendBacklog ( guard );
        }
        catch ( cacChannel::notConnected & ) {
            // intentionally ignored
        }
    }
    if ( cac.pCallbackGuard.get() &&
        cac.createdByThread == epicsThreadGetIdSelf () ) {
      epicsGuard < epicsMutex > guard ( cac.mutex );
      pMon->cancel ( *cac.pCallbackGuard.get(), guard );
    }
    else {
      //
      // we will definately stall out here if all of the
      // following are true
      //
      // o user creates non-preemtive mode client library context
      // o user doesnt periodically call a ca function
      // o user calls this function from an auxiillary thread
      //
      CallbackGuard cbGuard ( cac.cbMutex );
      epicsGuard < epicsMutex > guard ( cac.mutex );
      pMon->cancel ( cbGuard, guard );
    }
    return ECA_NORMAL;
}

void ca_client_context :: eliminateExcessiveSendBacklog (
    epicsGuard < epicsMutex > & guard, cacChannel & chan )
{
    if ( chan.requestMessageBytesPending ( guard ) >
            ca_client_context :: flushBlockThreshold ) {
        if ( this->pCallbackGuard.get() &&
            this->createdByThread == epicsThreadGetIdSelf () ) {
            // we need to be very careful about lock hierarchy
            // inversion in this situation
            epicsGuardRelease < epicsMutex > unguard ( guard );
            {
                epicsGuardRelease < epicsMutex > cbunguard (
                    * this->pCallbackGuard.get() );
                {
                    epicsGuard < epicsMutex > nestedGuard ( this->mutex );
                    chan.flush ( nestedGuard );
                }
            }
        }
        else {
            chan.flush ( guard );
        }
    }
}
