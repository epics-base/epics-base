
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

tsFreeList < class getCallback > getCallback::freeList;

getCallback::getCallback (oldChannel &chanIn, caEventCallBackFunc *pFuncIn, void *pPrivateIn) :
    chan (chanIn), pFunc (pFuncIn), pPrivate (pPrivateIn)
{
}

getCallback::~getCallback ()
{
}

void getCallback::destroy ()
{
    delete this;
}

void getCallback::completionNotify ( unsigned type, unsigned long count, const void *pData )
{
    struct event_handler_args   args;
    args.usr = this->pPrivate;
    args.chid = &this->chan;
    args.type = type;
    args.count = count;
    args.status = ECA_NORMAL;
    args.dbr = pData;
    (*this->pFunc) (args);
}

void getCallback::exceptionNotify (int status, const char *pContext)
{
    struct event_handler_args   args;
    args.usr = this->pPrivate;
    args.chid = &this->chan;
    args.type = 0;
    args.count = 0;
    args.status = status;
    args.dbr = 0;
    (*this->pFunc) (args);
}

void * getCallback::operator new ( size_t size )
{
    return getCallback::freeList.allocate ( size );
}

void getCallback::operator delete ( void *pCadaver, size_t size )
{
    getCallback::freeList.release ( pCadaver, size );
}