
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

void cacNotifyIO::destroy ()
{
    delete this;
}

void cacNotifyIO::completionNotify ()
{
    this->notify.completionNotify ();
}

void cacNotifyIO::completionNotify ( unsigned type, unsigned long count, const void *pData )
{
    this->notify.completionNotify ( type, count, pData );
}

void cacNotifyIO::exceptionNotify ( int status, const char *pContext )
{
    this->notify.exceptionNotify ( status, pContext );
}

void cacNotifyIO::exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count )
{
    this->notify.exceptionNotify ( status, pContext, type, count );
}

