
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

cacNotify::~cacNotify ()
{
}

void cacNotify::completionNotify ( cacChannelIO &chan )
{
    ca_printf ( "CAC: IO completion for channel %s with no handler installed?\n",
        chan.pName () );
}

void cacNotify::completionNotify ( cacChannelIO &chan, 
             unsigned type, unsigned long count, const void *pData )
{
    ca_printf ( "CAC: IO completion with no handler installed? channel=%s type=%u count=%u data pointer=%p\n",
        chan.pName (), type, count, pData );
}

void cacNotify::exceptionNotify ( cacChannelIO &chan, int status, const char *pContext )
{
    ca_signal_formated ( status, __FILE__, __LINE__, "%s channel=%s\n", 
        pContext, chan.pName () );
}

void cacNotify::exceptionNotify ( cacChannelIO &chan, int status, 
                                 const char *pContext, unsigned type, unsigned long count )
{
    ca_signal_formated ( status, __FILE__, __LINE__, "%s channel=%s type=%d count=%ld\n", 
        chan.pName (), pContext, type, count );
}
