
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

cacNotifyIO::cacNotifyIO ( cacNotify &notifyIn ) : notify ( notifyIn )
{
    assert ( ! this->notify.pIO );
    this->notify.pIO = this;
}

cacNotifyIO::~cacNotifyIO ()
{
    if ( this->notify.pIO == this ) {
        this->notify.pIO = 0;
        this->notify.destroy ();
    }
}

