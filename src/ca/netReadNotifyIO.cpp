
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
#include "netReadNotifyIO_IL.h"
#include "nciu_IL.h"

tsFreeList < class netReadNotifyIO, 1024 > netReadNotifyIO::freeList;

netReadNotifyIO::netReadNotifyIO ( nciu &chan, cacNotify &notifyIn ) :
    cacNotifyIO ( notifyIn ), baseNMIU ( chan ) {}

// private NOOP forces pool allocation
netReadNotifyIO::~netReadNotifyIO () {}

void netReadNotifyIO::uninstall ()
{
    this->chan.getPIIU ()->uninstallIO ( *this );
}

void netReadNotifyIO::completionNotify ()
{
    this->cacNotifyIO::exceptionNotify ( ECA_INTERNAL, "no data returned ?" );
}

void netReadNotifyIO::completionNotify ( unsigned type, 
                    unsigned long count, const void *pData )
{
    this->cacNotifyIO::completionNotify ( type, count, pData );
}

void netReadNotifyIO::exceptionNotify ( int status, const char *pContext )
{
    this->cacNotifyIO::exceptionNotify ( status, pContext );
}

void netReadNotifyIO::exceptionNotify ( int status, const char *pContext, 
                                       unsigned type, unsigned long count )
{
    this->cacNotifyIO::exceptionNotify ( status, pContext, type ,count );
}

void netReadNotifyIO::show ( unsigned level ) const
{
    printf ( "read notify IO at %p\n", this );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}
