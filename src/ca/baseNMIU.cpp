
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

baseNMIU::baseNMIU ( nciu &chanIn ) : chan (chanIn)
{
    this->chan.installIO ( *this );
}

void baseNMIU::destroy ()
{
    delete this;
}

baseNMIU::~baseNMIU ()
{
    this->chan.uninstallIO ( *this );
}

int baseNMIU::subscriptionMsg ()
{
    return ECA_NORMAL;
}
