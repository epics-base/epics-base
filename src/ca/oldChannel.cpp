
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

tsFreeList < struct oldChannel, 1024 > oldChannel::freeList;

/*
 * cacAlreadyConnHandler ()
 * This is installed into channels which dont have
 * a connection handler when ca_pend_io() times
 * out so that we will not decrement the pending
 * recv count in the future.
 */
extern "C" void cacAlreadyConnHandler ( struct connection_handler_args )
{
}

/*
 * cacNoConnHandler ()
 * This is installed into channels which dont have
 * a connection handler before ca_pend_io() times
 * out so that we will properly decrement the pending
 * recv count in the future.
 */
extern "C" void cacNoConnHandler ( struct connection_handler_args args )
{
    args.chid->lockOutstandingIO ();
    if ( args.chid->pConnCallBack == cacNoConnHandler ) {
        args.chid->pConnCallBack = cacAlreadyConnHandler;
        if ( args.op == CA_OP_CONN_UP ) {
            args.chid->decrementOutstandingIO ();
        }
    }
    args.chid->unlockOutstandingIO ();
}

extern "C" void cacNoopAccesRightsHandler ( struct access_rights_handler_args )
{
}

oldChannel::oldChannel (caCh *pConnCallBackIn, void *pPrivateIn) :
    pPrivate ( pPrivateIn ), pAccessRightsFunc ( cacNoopAccesRightsHandler )
{
    if ( ! pConnCallBackIn ) {
        this->pConnCallBack = cacNoConnHandler;
    }
    else {
        this->pConnCallBack = pConnCallBackIn;
    }
}

oldChannel::~oldChannel ()
{
    if ( this->pConnCallBack == cacNoConnHandler ) {
        this->decrementOutstandingIO ();
    }
}

void oldChannel::destroy ()
{
    delete this;
}

void oldChannel::ioAttachNotify ()
{
    this->lockOutstandingIO ();
    if ( this->pConnCallBack == cacNoConnHandler ) {
        this->incrementOutstandingIO ();
    }
    this->unlockOutstandingIO ();
}

void oldChannel::ioReleaseNotify ()
{
    this->lockOutstandingIO ();
    if ( this->pConnCallBack == cacNoConnHandler ) {
        this->decrementOutstandingIO ();
    }
    this->unlockOutstandingIO ();
}

void oldChannel::setPrivatePointer ( void *pPrivateIn )
{
    this->pPrivate = pPrivateIn;
}

void * oldChannel::privatePointer () const
{
    return this->pPrivate;
}

void oldChannel::connectTimeoutNotify ()
{
    this->lockOutstandingIO ();
    if ( this->pConnCallBack == cacNoConnHandler ) {
        this->pConnCallBack = cacAlreadyConnHandler;
    }
    this->unlockOutstandingIO ();
}

void oldChannel::connectNotify ()
{
    this->lockOutstandingIO ();
    struct connection_handler_args  args;
    args.chid = this;
    args.op = CA_OP_CONN_UP;
    (*this->pConnCallBack) (args);
    this->unlockOutstandingIO ();
}

void oldChannel::disconnectNotify ()
{
    this->lockOutstandingIO ();
    struct connection_handler_args  args;
    args.chid = this;
    args.op = CA_OP_CONN_DOWN;
    (*this->pConnCallBack) ( args );
    this->unlockOutstandingIO ();
}

int oldChannel::changeConnCallBack ( caCh *pfunc )
{
    this->lockOutstandingIO ();
    if ( ! pfunc ) {
        if ( this->pConnCallBack != cacNoConnHandler  && 
            this->pConnCallBack != cacAlreadyConnHandler ) {
            if ( this->state () == cs_never_conn ) {
                this->incrementOutstandingIO ();
                this->pConnCallBack = cacNoConnHandler;
            }
            else {
                this->pConnCallBack = cacAlreadyConnHandler;
            }
        }
    }
    else {
        if ( this->pConnCallBack == cacNoConnHandler ) { 
            this->decrementOutstandingIO ();
        }
        this->pConnCallBack = pfunc;
    }
    this->unlockOutstandingIO ();

    return ECA_NORMAL;
}

void oldChannel::accessRightsNotify ( caar ar )
{
    struct access_rights_handler_args args;
    args.chid = this;
    args.ar = ar;
    ( *this->pAccessRightsFunc ) ( args );
}

int oldChannel::replaceAccessRightsEvent ( caArh *pfunc )
{
    if ( ! pfunc ) {
        pfunc = cacNoopAccesRightsHandler;
    }

    this->pAccessRightsFunc = pfunc;
    if ( this->connected () ) {
        struct access_rights_handler_args args;
        args.chid = this;
        args.ar = this->accessRights ();
        (*pfunc) (args);
    }

    return ECA_NORMAL;
}

void * oldChannel::operator new ( size_t size )
{
    return oldChannel::freeList.allocate ( size );
}

void oldChannel::operator delete ( void *pCadaver, size_t size )
{
    oldChannel::freeList.release ( pCadaver, size );
}
