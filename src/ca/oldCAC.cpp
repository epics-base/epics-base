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

#include <stdio.h>

#include "iocinf.h"

#include "oldAccess.h"

oldCAC::oldCAC ( bool enablePreemptiveCallback,
                unsigned maxNumberOfChannels ) :
    clientCtx ( * new cac ( *this, enablePreemptiveCallback, maxNumberOfChannels ) ),
    ca_exception_func ( 0 ), ca_exception_arg ( 0 ), 
    pVPrintfFunc ( errlogVprintf ), fdRegFunc ( 0 ), fdRegArg ( 0 )
{
    if ( ! & this->clientCtx ) {
        throw -1;
    }
}

oldCAC::~oldCAC ()
{
    delete & this->clientCtx;
}

void oldCAC::changeExceptionEvent ( caExceptionHandler *pfunc, void *arg )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->ca_exception_func = pfunc;
    this->ca_exception_arg = arg;
// should block here until releated callback in progress completes
}

void oldCAC::replaceErrLogHandler ( caPrintfFunc *ca_printf_func )
{
    epicsAutoMutex autoMutex ( this->mutex );
    if ( ca_printf_func ) {
        this->pVPrintfFunc = ca_printf_func;
    }
    else {
        this->pVPrintfFunc = epicsVprintf;
    }
// should block here until releated callback in progress completes
}

void oldCAC::registerForFileDescriptorCallBack ( CAFDHANDLER *pFunc, void *pArg )
{
    epicsAutoMutex autoMutex ( this->mutex );
    this->fdRegFunc = pFunc;
    this->fdRegArg = pArg;
// should block here until releated callback in progress completes
}

int oldCAC::printf ( const char *pformat, ... ) const
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->oldCAC::vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

int oldCAC::vPrintf ( const char *pformat, va_list args ) const
{
    caPrintfFunc *pFunc;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        pFunc = this->pVPrintfFunc;
    }
    if ( pFunc ) {
        return ( *pFunc ) ( pformat, args );
    }
    else {
        return ::vfprintf ( stderr, pformat, args );
    }
}

void oldCAC::exception ( int stat, const char *pCtx, 
                        const char *pFile, unsigned lineNo )
{
    struct exception_handler_args args;
    caExceptionHandler *pFunc;
    void *pArg;
    {
        epicsAutoMutex autoMutex ( this->mutex );
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

void oldCAC::exception ( int status, const char *pContext,
    const char *pFileName, unsigned lineNo, oldChannelNotify &chan, 
    unsigned type, arrayElementCount count, unsigned op )
{
    struct exception_handler_args args;
    caExceptionHandler *pFunc;
    void *pArg;
    {
        epicsAutoMutex autoMutex ( this->mutex );
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

void oldCAC::fdWasCreated ( int fd )
{
    CAFDHANDLER *pFunc;
    void *pArg;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        pFunc = this->fdRegFunc;
        pArg = this->fdRegArg;
    }
    if ( pFunc ) {
        ( *pFunc ) ( pArg, fd, true );
    }
}

void oldCAC::fdWasDestroyed ( int fd )
{
    CAFDHANDLER *pFunc;
    void *pArg;
    {
        epicsAutoMutex autoMutex ( this->mutex );
        pFunc = this->fdRegFunc;
        pArg = this->fdRegArg;
    }
    if ( pFunc ) {
        ( *pFunc ) ( pArg, fd, false );
    }
}

void oldCAC::show ( unsigned level ) const
{
    ::printf ( "oldCAC at %p\n", 
        static_cast <const void *> ( this ) );
#if 0 // gnu compiler does not like casting func ptr to void ptr
    ::printf ( "exception func at %p arg at %p\n", 
        static_cast <const void *> ( this->ca_exception_func ),
        static_cast <const void *> ( this->ca_exception_arg ) );
    ::printf ( "printf func at %p\n", 
        static_cast <const void *> ( this->pVPrintfFunc ) );
    ::printf ( "fd registration func at %p arg at %p\n", 
        static_cast <const void *> ( this->fdRegFunc ),
        static_cast <const void *> ( this->fdRegArg ) );
#endif
    if ( level > 0u ) {
        this->mutex.show ( level - 1u );
        this->clientCtx.show ( level - 1u );
    }
}

void oldCAC::attachToClientCtx ()
{
    int status = ca_attach_context ( this );
    SEVCHK ( status, "error in virtual attach to client context" );
}



