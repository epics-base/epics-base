/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include "iocinf.h"

cacChannelNotify::~cacChannelNotify () 
{
}

void cacChannelNotify::connectNotify ( cacChannelIO & )
{
}

void cacChannelNotify::disconnectNotify ( cacChannelIO & )
{
}

void cacChannelNotify::accessRightsNotify ( cacChannelIO &, const caar & )
{
}

void cacChannelNotify::exceptionNotify ( cacChannelIO &io, int status, const char *pContext )
{
    ca_signal_formated ( status, __FILE__, __LINE__, "channel=%s context=\"%s\"\n", 
        io.pHostName (), pContext );
}

bool cacChannelNotify::includeFirstConnectInCountOfOutstandingIO () const
{
    return false;
}

class oldChannelNotify * cacChannelNotify::pOldChannelNotify ()
{
    return 0;
}
