
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

baseNMIU::baseNMIU ( nciu &chanIn ) : 
    chan ( chanIn )
{
}

baseNMIU::~baseNMIU ()
{
}

void baseNMIU::destroy ()
{
    delete this;
}

int baseNMIU::subscriptionMsg ()
{
    return ECA_NORMAL;
}

void baseNMIU::subscriptionCancelMsg ()
{
}

bool baseNMIU::isSubscription () const
{
    return false;
}

void baseNMIU::show ( unsigned /* level */ ) const
{
    printf ( "CA IO primitive at %p for channel %s\n", 
        this, this->chan.pName () );
}



