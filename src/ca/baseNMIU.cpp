
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

baseNMIU::baseNMIU ( nciu &chanIn ) : chan ( chanIn )
{
    chanIn.ioInstall ( *this );
}

baseNMIU::~baseNMIU ()
{
    // private NOOP forces pool allocation
}

void baseNMIU::destroy ()
{
    this->chan.ioDestroy ( this->getId () );
}

int baseNMIU::subscriptionMsg ()
{
    return ECA_NORMAL;
}
