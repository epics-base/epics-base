
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

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class tsFreeList < oldSubscription, 1024 >;
template class epicsSingleton < tsFreeList < oldSubscription, 1024 > >;

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif

epicsSingleton < tsFreeList < struct oldSubscription, 1024 > > oldSubscription::pFreeList;

oldSubscription::~oldSubscription ()
{
}

void oldSubscription::current (  
    unsigned type, arrayElementCount count, const void *pData)
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = &this->chan;
    args.type = type;
    args.count = count;
    args.status = ECA_NORMAL;
    args.dbr = pData;
    ( *this->pFunc ) ( args );
}
    
void oldSubscription::exception (
    int status, const char * /* pContext */, 
    unsigned type, arrayElementCount count )
{
    if ( status == ECA_CHANDESTROY ) {
        delete this;
    }
    else if ( status != ECA_DISCONN ) {
        struct event_handler_args args;
        args.usr = this->pPrivate;
        args.chid = &this->chan;
        args.type = type;
        args.count = count;
        args.status = status;
        args.dbr = 0;
        ( *this->pFunc ) ( args );
    }
}

