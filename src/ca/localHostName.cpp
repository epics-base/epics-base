
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

#include <string.h>

#include "osiSock.h"

#include "localHostName.h"

localHostName localHostNameAtLoadTime;

localHostName::localHostName ()
{
    int status = osiSockAttach ();
    if ( status ) {
        this->attachedToSockLib = true;
        status = gethostname ( this->cache, sizeof ( this->cache ) );
        if ( status ) {
            strncpy ( this->cache, "<unknown host>", sizeof ( this->cache ) );
        }
    }
    else {
        this->attachedToSockLib = false;
        strncpy ( this->cache, "<unknown host>", sizeof ( this->cache ) );
    }
    this->cache [ sizeof ( this->cache ) - 1u ] = '\0';
    this->length = strlen ( this->cache );
}

localHostName::~localHostName ()
{
    if ( this->attachedToSockLib ) {
        osiSockRelease ();
    }
}
