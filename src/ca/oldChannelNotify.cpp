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
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include <string>
#include <stdexcept>

#ifdef _MSC_VER
#   pragma warning(disable:4355)
#endif

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "errlog.h"

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"

extern "C" void cacNoopAccesRightsHandler ( struct access_rights_handler_args )
{
}

oldChannelNotify::oldChannelNotify ( 
        epicsGuard < epicsMutex > & guard, ca_client_context & cacIn, 
        const char *pName, caCh * pConnCallBackIn, 
        void * pPrivateIn, capri priority ) :
    cacCtx ( cacIn ), 
    io ( cacIn.createChannel ( guard, pName, *this, priority ) ), 
    pConnCallBack ( pConnCallBackIn ), 
    pPrivate ( pPrivateIn ), pAccessRightsFunc ( cacNoopAccesRightsHandler ),
    ioSeqNo ( 0 ), currentlyConnected ( false ), prevConnected ( false )
{
    guard.assertIdenticalMutex ( cacIn.mutexRef () );
    this->ioSeqNo = cacIn.sequenceNumberOfOutstandingIO ( guard );
    if ( pConnCallBackIn == 0 ) {
        cacIn.incrementOutstandingIO ( guard, this->ioSeqNo );
    }
}

oldChannelNotify::~oldChannelNotify ()
{
}

void  oldChannelNotify::destructor (
    epicsGuard < epicsMutex > & cbGuard,
    epicsGuard < epicsMutex > & guard )
{
    guard.assertIdenticalMutex ( this->cacCtx.mutexRef () );
    this->io.destroy ( cbGuard, guard );
    // no need to worry about a connect preempting here because
    // the io (the nciu) has been destroyed above
    if ( this->pConnCallBack == 0 && ! this->currentlyConnected ) {
        this->cacCtx.decrementOutstandingIO ( guard, this->ioSeqNo );
    }
    this->~oldChannelNotify ();
}

int oldChannelNotify::changeConnCallBack ( 
    epicsGuard < epicsMutex > & guard, caCh * pfunc )
{
    guard.assertIdenticalMutex ( this->cacCtx.mutexRef () );
    if ( ! this->currentlyConnected ) {
         if ( pfunc ) { 
            if ( ! this->pConnCallBack ) {
                this->cacCtx.decrementOutstandingIO ( guard, this->ioSeqNo );
            }
        }
        else {
            if ( this->pConnCallBack ) {
                this->cacCtx.incrementOutstandingIO ( guard, this->ioSeqNo );
            }
        }
    }
    pConnCallBack = pfunc;

    return ECA_NORMAL;
}

void oldChannelNotify::setPrivatePointer ( 
    epicsGuard < epicsMutex > & guard, void *pPrivateIn )
{
    guard.assertIdenticalMutex ( this->cacCtx.mutexRef () );
    this->pPrivate = pPrivateIn;
}

void * oldChannelNotify::privatePointer (
    epicsGuard < epicsMutex > & guard ) const
{
    guard.assertIdenticalMutex ( this->cacCtx.mutexRef () );
    return this->pPrivate;
}

int oldChannelNotify::replaceAccessRightsEvent ( 
    epicsGuard < epicsMutex > & guard, caArh * pfunc )
{
    // The order of the following is significant to guarantee that the
    // access rights handler is always gets called even if the channel connects
    // while this is running. There is some very small chance that the
    // handler could be called twice here with the same access rights state, but 
    // that will not upset the application.
    this->pAccessRightsFunc = pfunc ? pfunc : cacNoopAccesRightsHandler;
    caAccessRights tmp = this->io.accessRights ( guard );
      
    if ( this->currentlyConnected ) {
        struct access_rights_handler_args args;
        args.chid = this;
        args.ar.read_access = tmp.readPermit ();
        args.ar.write_access = tmp.writePermit ();
        epicsGuardRelease < epicsMutex > unguard ( guard );
        ( *this->pAccessRightsFunc ) ( args );
    }
    return ECA_NORMAL;
}

void oldChannelNotify::connectNotify ( 
    epicsGuard < epicsMutex > & guard )
{
    this->currentlyConnected = true;
    this->prevConnected = true;
    if ( this->pConnCallBack ) {
        struct connection_handler_args  args;
        args.chid = this;
        args.op = CA_OP_CONN_UP;
        caCh * pFunc = this->pConnCallBack;
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            ( *pFunc ) ( args );
        }
    }
    else {
        this->cacCtx.decrementOutstandingIO ( guard, this->ioSeqNo );
    }
}

void oldChannelNotify::disconnectNotify ( 
    epicsGuard < epicsMutex > & guard )
{
    this->currentlyConnected = false;
    if ( this->pConnCallBack ) {
        struct connection_handler_args args;
        args.chid = this;
        args.op = CA_OP_CONN_DOWN;
        caCh * pFunc = this->pConnCallBack;
        {
            epicsGuardRelease < epicsMutex > unguard ( guard );
            ( *pFunc ) ( args );
        }
    }
    else {
        this->cacCtx.incrementOutstandingIO ( 
            guard, this->ioSeqNo );
    }
}

void oldChannelNotify::serviceShutdownNotify (
    epicsGuard < epicsMutex > & callbackControlGuard, 
    epicsGuard < epicsMutex > & mutualExclusionGuard )
{
    this->cacCtx.destroyChannel ( 
        callbackControlGuard,
        mutualExclusionGuard,
        *this );
}

void oldChannelNotify::accessRightsNotify ( 
    epicsGuard < epicsMutex > & guard, const caAccessRights & ar )
{
    struct access_rights_handler_args args;
    args.chid = this;
    args.ar.read_access = ar.readPermit();
    args.ar.write_access = ar.writePermit();
    caArh * pFunc = this->pAccessRightsFunc;
    {
        epicsGuardRelease < epicsMutex > unguard ( guard );
        ( *pFunc ) ( args );
    }
}

void oldChannelNotify::exception ( 
    epicsGuard < epicsMutex > & guard, int status, const char * pContext )
{
    this->cacCtx.exception ( guard, status, pContext, __FILE__, __LINE__ );
}

void oldChannelNotify::readException ( 
    epicsGuard < epicsMutex > & guard, int status, const char *pContext,
    unsigned type, arrayElementCount count, void * /* pValue */ )
{
    this->cacCtx.exception ( guard, status, pContext, 
        __FILE__, __LINE__, *this, type, count, CA_OP_GET );
}

void oldChannelNotify::writeException ( 
    epicsGuard < epicsMutex > & guard, int status, const char *pContext,
    unsigned type, arrayElementCount count )
{
    this->cacCtx.exception ( guard, status, pContext, 
        __FILE__, __LINE__, *this, type, count, CA_OP_PUT );
}

void * oldChannelNotify::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void oldChannelNotify::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}

void oldChannelNotify::read ( 
    epicsGuard < epicsMutex > & guard,
    unsigned type, arrayElementCount count, 
    cacReadNotify & notify, cacChannel::ioid * pId )
{
    this->io.read ( guard, type, count, notify, pId );
}

void oldChannelNotify::write ( 
    epicsGuard < epicsMutex > & guard,
    unsigned type, arrayElementCount count, const void * pValue )
{
    this->io.write ( guard, type, count, pValue );
}

void oldChannelNotify::write ( 
    epicsGuard < epicsMutex > & guard, unsigned type, arrayElementCount count, 
    const void * pValue, cacWriteNotify & notify, cacChannel::ioid * pId )
{
    this->io.write ( guard, type, count, pValue, notify, pId );
}

void oldChannelNotify::subscribe ( 
    epicsGuard < epicsMutex > & guard, unsigned type, 
    arrayElementCount count, unsigned mask, cacStateNotify & notify,
    cacChannel::ioid & idOut)
{
    this->io.subscribe ( guard, type, count, mask, notify, &idOut );
}


