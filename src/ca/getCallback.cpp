
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

tsFreeList < class getCallback, 1024 > getCallback::freeList;
epicsMutex getCallback::freeListMutex;

getCallback::~getCallback ()
{
}

void getCallback::release ()
{
    delete this;
}

// eliminates SUN PRO warning
void getCallback::completionNotify ( cacChannelIO &io )
{
     this->cacNotify::completionNotify ( io );
}

void getCallback::completionNotify ( cacChannelIO &io, 
    unsigned type, unsigned long count, const void *pData )
{
    struct event_handler_args   args;
    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = type;
    args.count = count;
    args.status = ECA_NORMAL;
    args.dbr = pData;
    ( *this->pFunc ) ( args );
}

void getCallback::exceptionNotify ( cacChannelIO &io, int status, 
    const char * /* pContext */)
{
    struct event_handler_args   args;
    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = TYPENOTCONN;
    args.count = 0;
    args.status = status;
    args.dbr = 0;
    ( *this->pFunc ) ( args );
}

void getCallback::exceptionNotify ( cacChannelIO &io, 
    int status, const char * /* pContext */, 
    unsigned type, unsigned long count )
{
    struct event_handler_args   args;
    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = type;
    args.count = count;
    args.status = status;
    args.dbr = 0;
    ( *this->pFunc ) ( args );
}

