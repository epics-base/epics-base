
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
#include "netiiu_IL.h"

claimsPendingIIU::claimsPendingIIU ( tcpiiu &tcpIIUIn ) : 
    netiiu ( tcpIIUIn.clientCtx () ), tcpIIU ( tcpIIUIn )
{
}

claimsPendingIIU::~claimsPendingIIU ()
{
}

const char * claimsPendingIIU::pHostName () const
{
    return this->tcpIIU.pHostName ();
}

void claimsPendingIIU::hostName ( char *pBuf, unsigned bufLength ) const
{
    this->tcpIIU.hostName ( pBuf, bufLength );
}

bool claimsPendingIIU::connectionInProgress ( const char *pChannelName, const osiSockAddr &addr ) const
{
    return this->tcpIIU.connectionInProgress ( pChannelName, addr );
}
