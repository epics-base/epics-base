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

#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "nciu.h"
#include "cac.h"

netWriteNotifyIO::netWriteNotifyIO ( nciu &chan, cacWriteNotify &notifyIn ) :
    baseNMIU ( chan ), notify ( notifyIn )
{
}

netWriteNotifyIO::~netWriteNotifyIO ()
{
}

void netWriteNotifyIO::show ( unsigned /* level */ ) const
{
    ::printf ( "read write notify IO at %p\n", 
        static_cast < const void * > ( this ) );
}

void netWriteNotifyIO::destroy ( cacRecycle & recycle )
{
    this->~netWriteNotifyIO ();
    recycle.recycleWriteNotifyIO ( *this );
}

void netWriteNotifyIO::completion ()
{
    this->notify.completion ();
}

void netWriteNotifyIO::exception ( int status, const char *pContext )
{
    this->notify.exception ( status, pContext, UINT_MAX, 0u );
}

void netWriteNotifyIO::exception ( int status, const char *pContext, 
                                  unsigned type, arrayElementCount count )
{
    this->notify.exception ( status, pContext, type, count );
}


void netWriteNotifyIO::completion ( unsigned /* type */, 
    arrayElementCount /* count */, const void * /* pData */ )
{
    this->chan.getClient().printf ( "Write response with data ?\n" );
}

class netSubscription * netWriteNotifyIO::isSubscription ()
{
    return 0;
}

