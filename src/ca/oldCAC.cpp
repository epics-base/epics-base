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

#include "iocinf.h"

#include "oldAccess.h"

oldCAC::oldCAC ( bool enablePreemptiveCallback ) :
    clientCtx ( * new cac ( *this, enablePreemptiveCallback ) ),
    ca_exception_func ( ca_default_exception_handler ), ca_exception_arg ( 0 ), 
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
    if ( pfunc ) {
        this->ca_exception_func = pfunc;
        this->ca_exception_arg = arg;
    }
    else {
        this->ca_exception_func = ca_default_exception_handler;
        this->ca_exception_arg = NULL;
    }
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


int oldCAC::vPrintf ( const char *pformat, va_list args )
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
        return vfprintf ( stderr, pformat, args );
    }
}

void oldCAC::exception ( int stat, const char *pCtx, const char *pFile, unsigned lineNo )
{
    this->exception ( stat, pCtx, UINT_MAX, 0, pFile, lineNo );
}

void oldCAC::exception ( int stat, const char *ctx,
    unsigned type, unsigned long count, 
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
        args.type = type;
        args.count = count;
        args.addr = NULL;
        args.stat = stat;
        args.op = CA_OP_OTHER;
        args.ctx = ctx;
        args.pFile = pFile;
        args.lineNo = lineNo;
        args.usr = pArg;
        ( *pFunc ) ( args );
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

int oldCAC::printf ( const char *pformat, ... )
{
    va_list theArgs;
    int status;

    va_start ( theArgs, pformat );
    
    status = this->vPrintf ( pformat, theArgs );
    
    va_end ( theArgs );
    
    return status;
}

void oldCAC::show ( unsigned level ) const
{
    ::printf ( "oldCAC at %p\n", 
        static_cast <const void *> ( this ) );
    ::printf ( "exception func at %p arg at %p\n", 
        static_cast <const void *> ( this->ca_exception_func ),
        static_cast <const void *> ( this->ca_exception_arg ) );
    ::printf ( "printf func at %p\n", 
        static_cast <const void *> ( this->pVPrintfFunc ) );
    ::printf ( "fd registration func at %p arg at %p\n", 
        static_cast <const void *> ( this->fdRegFunc ),
        static_cast <const void *> ( this->fdRegArg ) );
    if ( level > 0u ) {
        this->mutex.show ( level - 1u );
        this->clientCtx.show ( level - 1u );
    }
}