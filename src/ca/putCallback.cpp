
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

putCallback::~putCallback ()
{
}

void putCallback::release ()
{
    delete this;
}

void putCallback::completionNotify ( cacChannelIO &io )
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = TYPENOTCONN;
    args.count = 0;
    args.status = ECA_NORMAL;
    args.dbr = 0;
    (*this->pFunc) (args);
}

void putCallback::completionNotify ( cacChannelIO &io, unsigned type, 
    unsigned long count, const void *pData )
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = type;
    args.count = count;
    args.status = ECA_NORMAL;
    args.dbr = 0;
    (*this->pFunc) (args);
}

void putCallback::exceptionNotify ( cacChannelIO &io, 
    int status, const char * /* pContext */ )
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = TYPENOTCONN;
    args.count = 0;
    args.status = status;
    args.dbr = 0;
    (*this->pFunc) (args);
}

void putCallback::exceptionNotify ( cacChannelIO &io, int status, 
    const char *pContext, unsigned type, unsigned long count )
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = type;
    args.count = count;
    args.status = status;
    args.dbr = 0;
    (*this->pFunc) (args);
}


