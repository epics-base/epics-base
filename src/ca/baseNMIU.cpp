
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, the Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "nciu.h"
#include "netIO.h"

baseNMIU::baseNMIU ( nciu &chanIn ) : 
    chan ( chanIn )
{
}

baseNMIU::~baseNMIU ()
{
}

class netSubscription * baseNMIU::isSubscription ()
{
    return 0;
}

void baseNMIU::show ( unsigned /* level */ ) const
{
    ::printf ( "CA IO primitive at %p\n", 
        static_cast <const void *> ( this ) );
}

