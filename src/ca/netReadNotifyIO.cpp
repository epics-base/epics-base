/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include <string>
#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "nciu.h"
#include "cac.h"

netReadNotifyIO::netReadNotifyIO ( nciu & chanIn, cacReadNotify & notify ) :
    notify ( notify ), chan ( chanIn )
{
}

netReadNotifyIO::~netReadNotifyIO ()
{
}

void netReadNotifyIO::show ( unsigned /* level */ ) const
{
    ::printf ( "read notify IO at %p\n", 
        static_cast < const void * > ( this ) );
}

void netReadNotifyIO::destroy ( cacRecycle & recycle )
{
    this->~netReadNotifyIO();
    recycle.recycleReadNotifyIO ( *this );
}

void netReadNotifyIO::completion ()
{
    this->chan.getClient().printf ( "Read response w/o data ?\n" );
}

void netReadNotifyIO::exception ( int status, const char *pContext )
{
    this->notify.exception ( status, pContext, UINT_MAX, 0u );
}

void netReadNotifyIO::exception ( int status, const char *pContext, 
                                 unsigned type, arrayElementCount count )
{
    this->notify.exception ( status, pContext, type, count );
}

void netReadNotifyIO::completion ( unsigned type, 
    arrayElementCount count, const void *pData )
{
    this->notify.completion ( type, count, pData );
}

class netSubscription * netReadNotifyIO::isSubscription ()
{
    return 0;
}

nciu & netReadNotifyIO::channel () const
{
    return this->chan;
}

void * netReadNotifyIO::operator new ( size_t ) // X aCC 361
{
    // The HPUX compiler seems to require this even though no code
    // calls it directly
    throw std::logic_error ( "why is the compiler calling private operator new" );
}

void netReadNotifyIO::operator delete ( void * )
{
    // Visual C++ .net appears to require operator delete if
    // placement operator delete is defined? I smell a ms rat
    // because if I declare placement new and delete, but
    // comment out the placement delete definition there are
    // no undefined symbols.
    errlogPrintf ( "%s:%d this compiler is confused about placement delete - memory was probably leaked",
        __FILE__, __LINE__ );
}



