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

#include <stdexcept>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "nciu.h"
#include "cac.h"

netWriteNotifyIO::netWriteNotifyIO ( nciu & chanIn, cacWriteNotify & notifyIn ) :
    notify ( notifyIn ), chan ( chanIn )
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

nciu & netWriteNotifyIO::channel () const
{
    return this->chan;
}



