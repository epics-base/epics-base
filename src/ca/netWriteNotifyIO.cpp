
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
#include "netWriteNotifyIO_IL.h"
#include "nciu_IL.h"
#include "baseNMIU_IL.h"

tsFreeList < class netWriteNotifyIO, 1024 > netWriteNotifyIO::freeList;
epicsMutex netWriteNotifyIO::freeListMutex;

netWriteNotifyIO::netWriteNotifyIO ( nciu &chan, cacNotify &notifyIn ) :
    baseNMIU ( notifyIn, chan )
{
}

netWriteNotifyIO::~netWriteNotifyIO () 
{
}

void netWriteNotifyIO::show ( unsigned level ) const
{
    printf ( "read write notify IO at %p\n", 
        static_cast < const void * > ( this ) );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}
