
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

#include "iocinf.h"
#include "nciu_IL.h"
#include "baseNMIU_IL.h"

baseNMIU::baseNMIU ( cacNotify &notifyIn, nciu &chanIn ) : 
    cacNotifyIO ( notifyIn ), chan ( chanIn )
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
    printf ( "CA IO primitive at %p for channel %s\n", 
        static_cast <const void *> ( this ), this->chan.pName () );
}

cacChannelIO & baseNMIU::channelIO () const
{
    return this->chan;
}


