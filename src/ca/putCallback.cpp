
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

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"

#ifdef _MSC_VER
#   pragma warning ( push )
#   pragma warning ( disable:4660 )
#endif

template class tsFreeList < class putCallback, 1024 >;
template class epicsSingleton < tsFreeList < class putCallback, 1024 > >;

#ifdef _MSC_VER
#   pragma warning ( pop )
#endif

epicsSingleton < tsFreeList < class putCallback, 1024 > > putCallback::pFreeList;

putCallback::putCallback ( oldChannelNotify &chanIn, 
                                 caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    chan ( chanIn ), pFunc ( pFuncIn ), pPrivate ( pPrivateIn )
{
}

putCallback::~putCallback ()
{
}

void putCallback::completion ()
{
    struct event_handler_args args;

    args.usr = this->pPrivate;
    args.chid = & this->chan;
    args.type = TYPENOTCONN;
    args.count = 0;
    args.status = ECA_NORMAL;
    args.dbr = 0;
    ( *this->pFunc ) (args);
    delete this;
}

void putCallback::exception (  
    int status, const char * /* pContext */, 
    unsigned type, arrayElementCount count )
{
    if ( status != ECA_CHANDESTROY ) {
        struct event_handler_args args;
        args.usr = this->pPrivate;
        args.chid = & this->chan;
        args.type = type;
        args.count = count;
        args.status = status;
        args.dbr = 0;
        ( *this->pFunc ) (args);
    }
    delete this;
}


