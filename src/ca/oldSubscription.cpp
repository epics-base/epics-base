
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "iocinf.h"
#include "oldAccess.h"

tsFreeList < struct oldSubscription > oldSubscription::freeList;

oldSubscription::oldSubscription  ( oldChannel &chanIn, caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    chan (chanIn), pFunc (pFuncIn), pPrivate (pPrivateIn)
{
}

oldSubscription::~oldSubscription ()
{
}

void oldSubscription::completionNotify (unsigned type, unsigned long count, const void *pData)
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = &this->chan;
    args.type = type;
    args.count = count;
    args.status = ECA_NORMAL;
    args.dbr = pData;
    (*this->pFunc) (args);
}

void oldSubscription::exceptionNotify (int status, const char *pContext)
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = &this->chan;
    args.type = 0;
    args.count = 0;
    args.status = status;
    args.dbr = 0;
    (*this->pFunc) (args);    
}

void oldSubscription::destroy ()
{
    delete this;
}

void * oldSubscription::operator new ( size_t size )
{
    return oldSubscription::freeList.allocate ( size );
}

void oldSubscription::operator delete ( void *pCadaver, size_t size )
{
    oldSubscription::freeList.release ( pCadaver, size );
}
