
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
#include "baseNMIU_IL.h"

tsFreeList < class netReadNotifyIO, 1024 > netReadNotifyIO::freeList;

netReadNotifyIO::netReadNotifyIO ( nciu &chan, cacNotify &notifyIn ) :
    cacNotifyIO ( notifyIn ), baseNMIU ( chan ) {}

netReadNotifyIO::~netReadNotifyIO () 
{
}

void netReadNotifyIO::destroy ()
{
    delete this;
}

void netReadNotifyIO::completionNotify ()
{
    this->notify ().exceptionNotify ( this->channelIO (), ECA_INTERNAL, "no data returned ?" );
}

void netReadNotifyIO::completionNotify ( unsigned type, 
                    unsigned long count, const void *pData )
{
    this->notify ().completionNotify ( this->channelIO (), type, count, pData );
}

void netReadNotifyIO::exceptionNotify ( int status, const char *pContext )
{
    this->notify ().exceptionNotify ( this->channelIO (), status, pContext );
}

void netReadNotifyIO::exceptionNotify ( int status, const char *pContext, 
                                       unsigned type, unsigned long count )
{
    this->notify ().exceptionNotify ( this->channelIO (), status, pContext, type ,count );
}

cacChannelIO & netReadNotifyIO::channelIO () const
{
    return this->channel ();
}

void netReadNotifyIO::show ( unsigned level ) const
{
    printf ( "read notify IO at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}
