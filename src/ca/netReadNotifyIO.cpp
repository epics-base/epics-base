
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

netReadNotifyIO::netReadNotifyIO ( nciu &chan, cacNotify &notifyIn ) :
    baseNMIU ( notifyIn, chan ) 
{
}

netReadNotifyIO::~netReadNotifyIO ()
{
}

void netReadNotifyIO::show ( unsigned level ) const
{
    printf ( "read notify IO at %p\n", 
        static_cast < const void * > ( this ) );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}

void netReadNotifyIO::cancel ()
{
    this->chan.getClient().destroyReadNotifyIO ( *this );
}

void netReadNotifyIO::destroy ( cacRecycle & recycle )
{
    this->~netReadNotifyIO();
    recycle.recycleReadNotifyIO ( *this );
}


