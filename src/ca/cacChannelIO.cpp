

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

cacChannelIO::cacChannelIO ( cacChannel &chanIn ) :
    chan ( chanIn ) 
{
    chan.attachIO ( *this );
}

cacChannelIO::~cacChannelIO ()
{
    this->chan.lock ();
    this->chan.ioReleaseNotify ();
    this->chan.pChannelIO = 0;
    this->chan.unlock ();
}

void cacChannelIO::connectNotify ()
{
    this->chan.connectNotify ();
}

void cacChannelIO::disconnectNotify ()
{
    this->chan.disconnectNotify ();
}

void cacChannelIO::connectTimeoutNotify ()
{
    this->chan.connectTimeoutNotify ();
}

void cacChannelIO::accessRightsNotify ( caar ar )
{
    this->chan.accessRightsNotify ( ar );
}

channel_state cacChannelIO::state () const 
{
    return cs_conn;
}

caar cacChannelIO::accessRights () const 
{
    caar ar;
    ar.read_access = true;
    ar.write_access = true;
    return ar;
}

unsigned cacChannelIO::searchAttempts () const 
{
    return 0u;
}

bool cacChannelIO::ca_v42_ok () const 
{
    return true;
}

bool cacChannelIO::connected () const 
{
    return true;
}

unsigned cacChannelIO::readSequence () const 
{
    return 0u;
}

void cacChannelIO::hostName ( char *pBuf, unsigned bufLength ) const 
{
    if ( bufLength ) {
        int status = gethostname ( pBuf, bufLength );
        if ( status ) {
            strncpy ( pBuf, "<unknown host>", bufLength );
            pBuf[ bufLength - 1u ] = '\0';
        }
    }
}

void cacChannelIO::incrementOutstandingIO ()
{
}

void cacChannelIO::decrementOutstandingIO ()
{
}
