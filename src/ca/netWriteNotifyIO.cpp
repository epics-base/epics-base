
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

tsFreeList < class netWriteNotifyIO, 1024 > netWriteNotifyIO::freeList;

netWriteNotifyIO::netWriteNotifyIO (nciu &chan, cacNotify &notifyIn) :
    cacNotifyIO (notifyIn), baseNMIU (chan)
{
}

netWriteNotifyIO::~netWriteNotifyIO () 
{
    // private NOOP forces pool allocation
}

void netWriteNotifyIO::destroy ()
{
    this->baseNMIU::destroy ();
}

void netWriteNotifyIO::completionNotify ()
{
    this->cacNotifyIO::completionNotify ();
}

void netWriteNotifyIO::completionNotify ( 
    unsigned /* type */, unsigned long /* count */, 
    const void * /* pData */ )
{
    this->cacNotifyIO::completionNotify ();
}

void netWriteNotifyIO::exceptionNotify ( int status, const char *pContext )
{
    this->cacNotifyIO::exceptionNotify (status, pContext);
}

void netWriteNotifyIO::exceptionNotify ( int status, const char *pContext, unsigned type, unsigned long count )
{
    this->cacNotifyIO::exceptionNotify (status, pContext, type, count);
}

void netWriteNotifyIO::show ( unsigned level ) const
{
    printf ( "read write notify IO at %p\n", this );
    if ( level > 0u ) {
        this->baseNMIU::show ( level - 1u );
    }
}
