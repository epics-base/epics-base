
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

oldChannel::oldChannel (caCh *pConnCallBackIn, void *pPrivateIn) :
    pConnCallBack (pConnCallBackIn), pPrivate (pPrivateIn), pAccessRightsFunc (0)
{
}

oldChannel::~oldChannel ()
{
    if ( ! this->pConnCallBack ) {
        this->decrementOutstandingIO ();
    }
}

void oldChannel::destroy ()
{
    delete this;
}

void oldChannel::ioAttachNotify ()
{
    this->lock ();
    if ( ! this->pConnCallBack ) {
        this->incrementOutstandingIO ();
    }
    this->unlock ();
}

void oldChannel::ioReleaseNotify ()
{
    this->lock ();
    if ( ! this->pConnCallBack ) {
        this->decrementOutstandingIO ();
    }
    this->unlock ();
}

void oldChannel::setPrivatePointer (void *pPrivateIn)
{
    this->pPrivate = pPrivateIn;
}

void * oldChannel::privatePointer () const
{
    return this->pPrivate;
}

/*
 * cacAlreadyConnHandler ()
 * This is installed into channels which dont have
 * a connection handler when ca_pend_io() times
 * out so that we will not decrement the pending
 * recv count in the future.
 */
extern "C" void cacAlreadyConnHandler (struct connection_handler_args)
{
}

void oldChannel::connectTimeoutNotify ()
{
    this->lock ();
    if ( ! this->pConnCallBack ) {
        this->pConnCallBack = cacAlreadyConnHandler;
    }
    this->unlock ();
}

void oldChannel::connectNotify ()
{
    this->lock ();
    if ( this->pConnCallBack ) {
        caCh *pCCB = this->pConnCallBack;
        struct connection_handler_args  args;
        args.chid = this;
        args.op = CA_OP_CONN_UP;

        (*pCCB) (args);
    }
    else {
        this->pConnCallBack = cacAlreadyConnHandler;
        this->decrementOutstandingIO ();
    }
    this->unlock ();
}

void oldChannel::disconnectNotify ()
{
    if ( this->pConnCallBack ) {
        struct connection_handler_args  args;
        args.chid = this;
        args.op = CA_OP_CONN_DOWN;
        (*this->pConnCallBack) (args);
    }
}

void oldChannel::accessRightsNotify (caar ar)
{
    if ( this->pAccessRightsFunc ) {
        struct access_rights_handler_args args;
        args.chid = this;
        args.ar = ar;
        ( *this->pAccessRightsFunc ) ( args );
    }
}

int oldChannel::changeConnCallBack (caCh *pfunc)
{
    this->lock ();
    if ( pfunc == 0) {
        if ( this->pConnCallBack != 0 ) {
            if ( this->pConnCallBack != cacAlreadyConnHandler ) {
                this->incrementOutstandingIO ();
                this->pConnCallBack = 0;
            }
        }
    }
    else {
        if ( this->pConnCallBack == 0 ) { 
            this->decrementOutstandingIO ();
        }
        this->pConnCallBack = pfunc;
    }
    this->unlock ();

    return ECA_NORMAL;
}

int oldChannel::replaceAccessRightsEvent (caArh *pfunc)
{
    this->lock ();
    this->pAccessRightsFunc = pfunc;
    if ( pfunc && this->connected () ) {
        struct access_rights_handler_args args;
        args.chid = this;
        args.ar = this->accessRights ();
        (*pfunc) (args);
    }
    this->unlock ();

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