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

extern epicsThreadPrivateId caClientContextId;

ca_client_context::ca_client_context ( bool enablePreemptiveCallback ) :
    clientCtx ( * new cac ( *this, enablePreemptiveCallback ) ),
    pCallbackGuard ( 0 ), ca_exception_func ( 0 ), ca_exception_arg ( 0 ), 
    pVPrintfFunc ( errlogVprintf ), fdRegFunc ( 0 ), fdRegArg ( 0 ),
    pndRecvCnt ( 0u ), ioSeqNo ( 0u )
{
    if ( ! enablePreemptiveCallback ) {
        this->pCallbackGuard = new epicsGuard < callbackMutex > 
            ( this->clientCtx.callbackGuardFactory () );
    }
}

ca_client_context::~ca_client_context ()
{
    delete this->pCallbackGuard;
    delete & this->clientCtx;
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
        this->clientCtx.signal ( stat, pFile, lineNo, pCtx );
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
        this->clientCtx.signal ( status, pFileName, lineNo, 
            "op=%u, channel=%s, type=%s, count=%lu, ctx=\"%s\"", 
            op, ca_name ( &chan ), 
            dbr_type_to_text ( static_cast <int> ( type ) ), 
            count, pContext );
    }
}

void ca_client_context::fdWasCreated ( int fd )
{
    CAFDHANDLER *pFunc;
    void *pArg;
    {
        epicsGuard < ca_client_context_mutex > autoMutex ( this->mutex );
        pFunc = this->fdRegFunc;
        pArg = this->fdRegArg;
    }
    if ( pFunc ) {
        ( *pFunc ) ( pArg, fd, true );
    }
}

void ca_client_context::fdWasDestroyed ( int fd )
{
    CAFDHANDLER *pFunc;
    void *pArg;
    {
        epicsGuard < ca_client_context_mutex > autoMutex ( this->mutex );
        pFunc = this->fdRegFunc;
        pArg = this->fdRegArg;
    }
    if ( pFunc ) {
        ( *pFunc ) ( pArg, fd, false );
    }
}

void ca_client_context::show ( unsigned level ) const
{
    ::printf ( "ca_client_context at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        this->mutex.show ( level - 1u );
        this->clientCtx.show ( level - 1u );
        ::printf ( "\tpreemptive callback is %s\n",
            this->pCallbackGuard ? "disabled" : "enabled" );
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
    if ( this->pCallbackGuard ) {
        epicsGuardRelease < callbackMutex > unguard ( *this->pCallbackGuard );
        this->clientCtx.waitUntilNoRecvThreadsPending ();
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
        if ( this->pCallbackGuard ) {
            epicsGuardRelease < callbackMutex > unguard ( *this->pCallbackGuard );
            epicsThreadSleep ( delay );
        }
        else {
            epicsThreadSleep ( delay );
        }
    }

    return ECA_TIMEOUT;
}

void ca_client_context::blockForEventAndEnableCallbacks ( epicsEvent & event, double timeout )
{
    if ( this->pCallbackGuard ) {
        epicsGuardRelease < callbackMutex > unguard ( *this->pCallbackGuard );
        event.wait ( timeout );
    }
    else {
        event.wait ( timeout );
    }
}
