
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

tsFreeList < class oldChannelNotify, 1024 > oldChannelNotify::freeList;

extern "C" void cacNoopConnHandler ( struct connection_handler_args )
{
}

extern "C" void cacNoopAccesRightsHandler ( struct access_rights_handler_args )
{
}

oldChannelNotify::oldChannelNotify ( caCh *pConnCallBackIn, void *pPrivateIn ) :
pConnCallBack ( pConnCallBackIn ? pConnCallBackIn : cacNoopConnHandler ), 
    pPrivate ( pPrivateIn ), pAccessRightsFunc ( cacNoopAccesRightsHandler )
{
}

oldChannelNotify::~oldChannelNotify ()
{
}

void oldChannelNotify::release ()
{
    delete this;
}

void oldChannelNotify::setPrivatePointer ( void *pPrivateIn )
{
    this->pPrivate = pPrivateIn;
}

void * oldChannelNotify::privatePointer () const
{
    return this->pPrivate;
}

int oldChannelNotify::changeConnCallBack ( cacChannelIO &chan, caCh *pfunc )
{
    this->pConnCallBack = pfunc ? pfunc : cacNoopConnHandler;
    // test for NOOP connection handler does _not_ occur here because the
    // lock is not applied
    chan.notifyStateChangeFirstConnectInCountOfOutstandingIO ();
    return ECA_NORMAL;
}

int oldChannelNotify::replaceAccessRightsEvent ( cacChannelIO &chan, caArh *pfunc )
{
    // The order of the following is significant to guarantee that the
    // access rights handler is always gets called even if the channel connects
    // while this is running. There is some very small chance that the
    // handler could be called twice here with the same access rights state, but 
    // that will not upset the application.
    this->pAccessRightsFunc = pfunc ? pfunc : cacNoopAccesRightsHandler;
    if ( chan.connected () ) {
        struct access_rights_handler_args args;
        args.chid = &chan;
        args.ar = chan.accessRights ();
        ( *pfunc ) ( args );
    }
    return ECA_NORMAL;
}

void oldChannelNotify::connectNotify ( cacChannelIO &chan )
{
    struct connection_handler_args  args;
    args.chid = &chan;
    args.op = CA_OP_CONN_UP;
    ( *this->pConnCallBack ) ( args );
}

void oldChannelNotify::disconnectNotify ( cacChannelIO &chan )
{
    struct connection_handler_args args;
    args.chid = &chan;
    args.op = CA_OP_CONN_DOWN;
    ( *this->pConnCallBack ) ( args );
}

void oldChannelNotify::accessRightsNotify ( cacChannelIO &chan, const caar &ar )
{
    struct access_rights_handler_args args;
    args.chid = &chan;
    args.ar = ar;
    ( *this->pAccessRightsFunc ) ( args );
}

bool oldChannelNotify::includeFirstConnectInCountOfOutstandingIO () const
{
    return ( this->pConnCallBack == cacNoopConnHandler );
}

class oldChannelNotify * oldChannelNotify::pOldChannelNotify ()
{
    return this;
}

void * oldChannelNotify::operator new ( size_t size )
{
    return oldChannelNotify::freeList.allocate ( size );
}

void oldChannelNotify::operator delete ( void *pCadaver, size_t size )
{
    oldChannelNotify::freeList.release ( pCadaver, size );
}
