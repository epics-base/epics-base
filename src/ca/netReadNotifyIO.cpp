
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

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "nciu.h"
#include "netIO.h"
#include "cac.h"

netReadNotifyIO::netReadNotifyIO ( nciu &chan, cacReadNotify &notify ) :
    baseNMIU ( chan ), notify ( notify )
{
}

netReadNotifyIO::~netReadNotifyIO ()
{
}

void netReadNotifyIO::show ( unsigned level ) const
{
    ::printf ( "read notify IO at %p\n", 
        static_cast < const void * > ( this ) );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
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
    this->notify.exception ( status, pContext, UINT_MAX, 0 );
}

void netReadNotifyIO::completion ( unsigned type, 
    unsigned long count, const void *pData )
{
    this->notify.completion ( type, count, pData );
}

void netReadNotifyIO::exception ( int status, 
    const char *pContext, unsigned type, unsigned long count )
{
    this->notify.exception ( status, pContext, type, count );
}



