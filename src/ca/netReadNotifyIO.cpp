
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

tsFreeList < class netReadNotifyIO > netReadNotifyIO::freeList;

netReadNotifyIO::netReadNotifyIO ( nciu &chan, cacNotify &notifyIn ) :
    baseNMIU ( chan ), cacNotifyIO ( notifyIn )
{
}

netReadNotifyIO::~netReadNotifyIO () 
{
}

void netReadNotifyIO::destroy ()
{
    delete this;
}

void netReadNotifyIO::disconnect ( const char *pHostName )
{
    this->cacNotifyIO::exceptionNotify ( ECA_DISCONN, pHostName );
    delete this;
}

void netReadNotifyIO::completionNotify ()
{
    this->cacNotifyIO::exceptionNotify ( ECA_INTERNAL, "no data returned ?" );
}

void netReadNotifyIO::completionNotify ( unsigned type, 
                    unsigned long count, const void *pData )
{
    this->cacNotifyIO::completionNotify ( type, count, pData );
}

void netReadNotifyIO::exceptionNotify ( int status, const char *pContext )
{
    this->cacNotifyIO::exceptionNotify ( status, pContext );
}

void netReadNotifyIO::exceptionNotify ( int status, const char *pContext, 
                                       unsigned type, unsigned long count )
{
    this->cacNotifyIO::exceptionNotify ( status, pContext, type ,count );
}
