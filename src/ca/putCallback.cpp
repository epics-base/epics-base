
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

tsFreeList < class putCallback, 1024 > putCallback::freeList;

putCallback::putCallback (oldChannel &chanIn, caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    chan (chanIn), pFunc (pFuncIn), pPrivate (pPrivateIn)
{
}

putCallback::~putCallback ()
{
}

void putCallback::destroy ()
{
    delete this;
}

void putCallback::completionNotify ()
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = &this->chan;
    args.type = 0;
    args.count =  0;
    args.status = ECA_NORMAL;
    args.dbr = 0;
    (*this->pFunc) (args);
}

void putCallback::exceptionNotify (int status, const char *pContext)
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

void * putCallback::operator new ( size_t size )
{
    return putCallback::freeList.allocate ( size );
}

void putCallback::operator delete ( void *pCadaver, size_t size )
{
    putCallback::freeList.release ( pCadaver, size );
}
