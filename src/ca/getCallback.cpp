
/*  
 *  $Id$
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

#include "iocinf.h"
#include "oldAccess.h"

template class tsFreeList < class getCallback, 1024 >;

tsFreeList < class getCallback, 1024 > getCallback::freeList;
epicsMutex getCallback::freeListMutex;

getCallback::getCallback ( oldChannelNotify &chanIn, 
    caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
        chan ( chanIn ), pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

getCallback::~getCallback ()
{
}

void getCallback::completion (
    unsigned type, arrayElementCount count, const void *pData )
{
    struct event_handler_args   args;
    args.usr = this->pPrivate;
    args.chid = &this->chan;
    args.type = type;
    args.count = count;
    args.status = ECA_NORMAL;
    args.dbr = pData;
    ( *this->pFunc ) ( args );
    delete this;
}

void getCallback::exception (
    int status, const char * /* pContext */, 
    unsigned type, arrayElementCount count )
{
    if ( status != ECA_CHANDESTROY ) {
        struct event_handler_args   args;
        args.usr = this->pPrivate;
        args.chid = &this->chan;
        args.type = type;
        args.count = count;
        args.status = status;
        args.dbr = 0;
        ( *this->pFunc ) ( args );
    }
    delete this;
}

