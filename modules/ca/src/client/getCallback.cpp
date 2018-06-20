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
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include <string>
#include <stdexcept>

#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"

getCallback::getCallback ( oldChannelNotify & chanIn, 
    caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
        chan ( chanIn ), pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

getCallback::~getCallback ()
{
}

void getCallback::completion (
    epicsGuard < epicsMutex > & guard,
    unsigned type, arrayElementCount count, const void *pData )
{
    struct event_handler_args args;
    args.usr = this->pPrivate;
    args.chid = & this->chan;
    args.type = type;
    args.count = count;
    args.status = ECA_NORMAL;
    args.dbr = pData;
    caEventCallBackFunc * pFuncTmp = this->pFunc;
    // fetch client context and destroy prior to releasing
    // the lock and calling cb in case they destroy channel there
    this->chan.getClientCtx().destroyGetCallback ( guard, *this );
    if ( pFuncTmp ) {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        pFuncTmp ( args );
    }
}

void getCallback::exception (
    epicsGuard < epicsMutex > & guard,
    int status, const char * /* pContext */, 
    unsigned type, arrayElementCount count )
{
    if ( status != ECA_CHANDESTROY ) {
        struct event_handler_args args;
        args.usr = this->pPrivate;
        args.chid = & this->chan;
        args.type = type;
        args.count = count;
        args.status = status;
        args.dbr = 0;
        caEventCallBackFunc * pFuncTmp = this->pFunc;
        // fetch client context and destroy prior to releasing
        // the lock and calling cb in case they destroy channel there
        this->chan.getClientCtx().destroyGetCallback ( guard, *this );
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            ( *pFuncTmp ) ( args );
        }
    }
    else {
        this->chan.getClientCtx().destroyGetCallback ( guard, *this );
    }
}

void getCallback::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}


