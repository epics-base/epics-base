
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

tsFreeList < struct oldSubscription, 1024 > oldSubscription::freeList;
epicsMutex oldSubscription::freeListMutex;

oldSubscription::~oldSubscription ()
{
}

// eliminates sun pro warning
void oldSubscription::completionNotify ( cacChannelIO &io )
{
     this->cacNotify::completionNotify ( io );
}

void oldSubscription::completionNotify ( cacChannelIO &io, 
    unsigned type, unsigned long count, const void *pData)
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = type;
    args.count = count;
    args.status = ECA_NORMAL;
    args.dbr = pData;
    ( *this->pFunc ) (args);
}

void oldSubscription::exceptionNotify ( cacChannelIO &io,
    int status, const char * /* pContext */ )
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = TYPENOTCONN;
    args.count = 0;
    args.status = status;
    args.dbr = 0;
    ( *this->pFunc ) (args);    
}
    
void oldSubscription::exceptionNotify ( cacChannelIO &io,
    int status, const char * /* pContext */, 
    unsigned type, unsigned long count )
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = & io;
    args.type = type;
    args.count = count;
    args.status = status;
    args.dbr = 0;
    ( *this->pFunc ) (args);
}

void oldSubscription::release ()
{
    delete this;
}


