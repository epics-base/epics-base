
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"

#if defined ( _MSC_VER )
#   pragma warning ( push )
#   pragma warning ( disable: 4660 )
#endif

template class tsFreeList < struct oldChannelNotify, 1024 >;

#if defined ( _MSC_VER )
#   pragma warning ( pop )
#endif

tsFreeList < struct oldChannelNotify, 1024 > oldChannelNotify::freeList;
epicsMutex oldChannelNotify::freeListMutex;

extern "C" void cacNoopConnHandler ( struct connection_handler_args )
{
}

extern "C" void cacNoopAccesRightsHandler ( struct access_rights_handler_args )
{
}

oldChannelNotify::oldChannelNotify ( oldCAC & cacIn, const char *pName, 
        caCh * pConnCallBackIn, void * pPrivateIn, capri priority ) :
    io ( cacIn.createChannel ( pName, *this, priority ) ), 
    cacCtx ( cacIn ), 
    pConnCallBack ( pConnCallBackIn ? pConnCallBackIn : cacNoopConnHandler ), 
    pPrivate ( pPrivateIn ), pAccessRightsFunc ( cacNoopAccesRightsHandler )
{
}

oldChannelNotify::~oldChannelNotify ()
{
    delete & this->io;
}

void oldChannelNotify::setPrivatePointer ( void *pPrivateIn )
{
    this->pPrivate = pPrivateIn;
}

void * oldChannelNotify::privatePointer () const
{
    return this->pPrivate;
}

int oldChannelNotify::changeConnCallBack ( caCh *pfunc )
{
    this->pConnCallBack = pfunc ? pfunc : cacNoopConnHandler;
    // test for NOOP connection handler does _not_ occur here because the
    // lock is not applied
    this->io.notifyStateChangeFirstConnectInCountOfOutstandingIO ();
    return ECA_NORMAL;
}

int oldChannelNotify::replaceAccessRightsEvent ( caArh *pfunc )
{
    // The order of the following is significant to guarantee that the
    // access rights handler is always gets called even if the channel connects
    // while this is running. There is some very small chance that the
    // handler could be called twice here with the same access rights state, but 
    // that will not upset the application.
    this->pAccessRightsFunc = pfunc ? pfunc : cacNoopAccesRightsHandler;
    if ( this->io.connected () ) {
        struct access_rights_handler_args args;
        args.chid = this;
        caAccessRights tmp = this->io.accessRights ();
        args.ar.read_access = tmp.readPermit ();
        args.ar.write_access = tmp.writePermit ();
        ( *pfunc ) ( args );
    }
    return ECA_NORMAL;
}

void oldChannelNotify::connectNotify ()
{
    struct connection_handler_args  args;
    args.chid = this;
    args.op = CA_OP_CONN_UP;
    ( *this->pConnCallBack ) ( args );
}

void oldChannelNotify::disconnectNotify ()
{
    struct connection_handler_args args;
    args.chid = this;
    args.op = CA_OP_CONN_DOWN;
    ( *this->pConnCallBack ) ( args );
}

void oldChannelNotify::accessRightsNotify ( const caAccessRights &ar )
{
    struct access_rights_handler_args args;
    args.chid = this;
    args.ar.read_access = ar.readPermit();
    args.ar.write_access = ar.writePermit();
    ( *this->pAccessRightsFunc ) ( args );
}

void oldChannelNotify::exception ( int status, const char *pContext )
{
    this->cacCtx.exception ( status, pContext, __FILE__, __LINE__ );
}

void oldChannelNotify::readException ( int status, const char *pContext,
    unsigned type, arrayElementCount count, void * /* pValue */ )
{
    this->cacCtx.exception ( status, pContext, 
        __FILE__, __LINE__, *this, type, count, CA_OP_GET );
}

void oldChannelNotify::writeException ( int status, const char *pContext,
    unsigned type, arrayElementCount count )
{
    this->cacCtx.exception ( status, pContext, 
        __FILE__, __LINE__, *this, type, count, CA_OP_PUT );
}

bool oldChannelNotify::includeFirstConnectInCountOfOutstandingIO () const
{
    return ( this->pConnCallBack == cacNoopConnHandler );
}

void * oldChannelNotify::operator new ( size_t size )
{
    epicsAutoMutex locker ( oldChannelNotify::freeListMutex );
    return oldChannelNotify::freeList.allocate ( size );
}

void oldChannelNotify::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( oldChannelNotify::freeListMutex );
    oldChannelNotify::freeList.release ( pCadaver, size );
}
