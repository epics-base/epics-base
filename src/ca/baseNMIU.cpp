
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

osiMutex baseNMIU::mutex;

baseNMIU::baseNMIU ( nciu &chanIn ) : 
    chan ( chanIn ), attachedToChannel ( true )
{
    chanIn.ioInstall ( *this );
}

baseNMIU::~baseNMIU ()
{
    this->uninstallFromChannel ();
}

void baseNMIU::uninstallFromChannel ()
{
    this->mutex.lock ();
    bool attached = this->attachedToChannel;
    this->attachedToChannel = false;
    this->mutex.unlock ();

    if ( attached ) {
        this->chan.ioUninstall ( *this );
    }
}

void baseNMIU::destroy ()
{
    delete this;
}

int baseNMIU::subscriptionMsg ()
{
    return ECA_NORMAL;
}

void baseNMIU::show ( unsigned /* level */ ) const
{
    printf ( "CA IO primitive at %p for channel %s\n", 
        this, this->chan.pName () );
}
